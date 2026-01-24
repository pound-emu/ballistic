"""
If you're in the same directory where this script is located you will be able
to run it without passing any parameters:

$ python3 generate_a64_table.py
Using default XML directory: ../spec/arm64_xml/
Using default output directory: ../src/
Found 2038 XML files
Generated ARM decoder table header file -> ../src/decoder_table_gen.h
Generated ARM decoder table source file -> ../src/decoder_table_gen.c
"""

import io
import os
import re
import sys
import glob
import argparse
import xml.etree.ElementTree as ET
from xml.etree.ElementTree import ElementTree
from xml.etree.ElementTree import Element
from dataclasses import dataclass
from typing import List, Dict, Tuple, Optional

DEFAULT_DECODER_GENERATED_HEADER_NAME = "decoder_table_gen.h"
DEFAULT_DECODER_GENERATED_SOURCE_NAME = "decoder_table_gen.c"
DEFAULT_OUTPUT_DIRECTORY = "../src/"
DEFAULT_XML_DIRECTORY_PATH = "../spec/arm64_xml/"

DECODER_HEADER_NAME = "bal_decoder.h"
DECODER_METADATA_STRUCT_NAME = "bal_decoder_instruction_metadata_t"
DECODER_HASH_TABLE_BUCKET_STRUCT_NAME = "decoder_bucket_t"

DECODER_ARM64_INSTRUCTIONS_SIZE_NAME = "BAL_DECODER_ARM64_INSTRUCTIONS_SIZE"
DECODER_ARM64_HASH_TABLE_BUCKET_SIZE_NAME = "BAL_DECODER_ARM64_HASH_TABLE_BUCKET_SIZE"
DECODER_ARM64_GLOBAL_INSTRUCTIONS_ARRAY_NAME = "g_bal_decoder_arm64_instructions"

DECODER_HASH_TABLE_SIZE = 2048

# Top 11 bits
DECODER_HASH_BITS_MASK = 0xFFE00000

GENERATED_FILE_WARNING = """/*
 * GENERATED FILE - DO NOT EDIT
 * Generated with tools/generate_a64_table.py
 */"""


@dataclass
class Operand:
    type_enum: str
    bit_position: int
    bit_width: int


@dataclass
class A64Instruction:
    mnemonic: str
    mask: int
    value: int
    priority: int  # Higher number of set bits in mask = higher priority.
    array_index: int  # Position in the hash table bucket.
    operands: List[Operand]


def parse_register_diagram(register_diagram: ET.Element) -> Dict[str, Tuple[int, int]]:
    """Parse the register diagram to map field names to (bit_position, bit_width)."""
    fields: Dict[str, Tuple[int, int]] = {}

    for box in register_diagram.findall("box"):
        name: Optional[str] = box.get("name")

        if name is None:
            continue

        hibit: int = int(box.get("hibit"))
        width: int = int(box.get("width", "1"))
        bit_position: int = hibit - width + 1

        if name not in fields:
            fields[name] = (bit_position, width)

    return fields


def process_box(box: Element, current_mask: int, current_value: int) -> Tuple[int, int]:
    """Process a specific bit-field box fron the XML diagram."""
    # The high-bit position in the 32-bit word.
    hibit_str: Optional[str] = box.attrib.get("hibit")

    if hibit_str is None:
        return (current_mask, current_value)

    hibit: int = int(hibit_str)

    # The width of this bitfield.
    width_str: Optional[int] = int(box.attrib.get("width", "1"))

    if width_str is None:
        return (current_mask, current_value)

    width: int = int(width_str)

    if hibit >= 32:
        raise ValueError(f"Bit positiin {hibit} exceeds 31")

    # Get the constraint content (e.g., "11", "0", "101")
    # If 'c' is missing, it's usually a variable field (register/imm)

    c_elements: List[Element[str]] = box.findall("c")
    content: str = ""

    for c_element in c_elements:
        content += c_element.text if c_element.text is not None else "x"

    if len(content) < width:
        content = content.rjust(width, "x")

    # Apply bits
    for i, char in enumerate(content):
        bit_position: int = hibit - i

        if bit_position < 0:
            raise ValueError(f"Bit position underflow: {hibit} - {i}")

        if char == "1":
            current_mask |= 1 << bit_position
            current_value |= 1 << bit_position
        elif char == "0":
            current_mask |= 1 << bit_position
            current_value &= ~(1 << bit_position)

    return (current_mask, current_value)


def parse_explanations(root: Element) -> Dict[str, str]:
    """
    Parses the <explanations> section of map symbol links (e.g. 'sa_rd')
    to the encoded bitfield name (e.g 'Rd').
    """
    mapping: Dict[str, str] = {}

    for explanation in root.findall(".//explanation"):
        symbol_tag: Optional[str] = explanation.find("symbol")
        if symbol_tag is None:
            continue

        link: Optional[str] = symbol_tag.get("link")

        if link is None:
            continue

        encoded_in: Optional[str] = None
        account: Optional[str] = explanation.find("account")

        if account is not None:
            encoded_in = account.get("encodedin")

        if encoded_in is None:
            definition = explanation.find("definition")

            if definition is not None:
                encoded_in = definition.get("encodedin")

        if encoded_in is not None:
            mapping[link] = encoded_in

    return mapping


def derive_operand_type(text: str, hover: str) -> str:
    """Derives the BAL_OPERAND_TYPE based on syntax and description."""
    # Normalize text: <Wd> -> WD, <Wd|WSP> -> WD|WSP
    t = text.strip().replace("<", "<").replace(">", ">").replace("<", "").replace(">", "").upper()
    h = hover.lower()
    # Immediate checks
    if any(
        k in h for k in ["immediate", "amount", "offset", "index", "label", "shift"]
    ) or t.startswith("#"):
        return "BAL_OPERAND_TYPE_IMMEDIATE"

    # Register checks by name prefix
    # W = 32-bit GP, S = 32-bit FP
    if t.startswith("W") or t.startswith("S"):
        return "BAL_OPERAND_TYPE_REGISTER_32"

    # X = 64-bit GP, D = 64-bit FP, SP = Stack Pointer
    if t.startswith("X") or t.startswith("D") or t == "SP" or t == "WSP":
        # WSP is 32-bit SP
        if t == "WSP":
            return "BAL_OPERAND_TYPE_REGISTER_32"

        return "BAL_OPERAND_TYPE_REGISTER_64"

    # V = Vector, Q = 128-bit FP, Z = SVE Vector
    if t.startswith("V") or t.startswith("Q") or t.startswith("Z"):
        return "BAL_OPERAND_TYPE_REGISTER_128"

    # P = SVE Predicate.
    if t.startswith("P"):
        return "BAL_OPERAND_TYPE_REGISTER_32"

    # B = 8-bit FP/Vector, H = 16-bit FP/Vector
    # Usually treated as SIMD/FP registers.
    if t.startswith("B") or t.startswith("H"):
        return "BAL_OPERAND_TYPE_REGISTER_128"

    # Fallback to hover text analysis
    if "32-bit" in h and ("general-purpose" in h or "register" in h):
        return "BAL_OPERAND_TYPE_REGISTER_32"
    if "64-bit" in h and ("general-purpose" in h or "register" in h):
        return "BAL_OPERAND_TYPE_REGISTER_64"
    if "128-bit" in h or "simd" in h or "vector" in h or "scalable" in h:
        return "BAL_OPERAND_TYPE_REGISTER_128"

    if "condition" in h or "cond" in t.lower():
        return "BAL_OPERAND_TYPE_CONDITION"

    return "BAL_OPERAND_TYPE_NONE"


def parse_operands(
    asmtemplate: ET.Element,
    field_map: Dict[str, Tuple[int, int]],
    explanation_map: Dict[str, str],
) -> List[Operand]:
    """
    Parses `asmtemplate` to find operands and map them to bit fields.
    """
    operands: List[Operand] = []

    for anchor in asmtemplate.findall("a"):
        link: Optional[str] = anchor.get("link")
        hover: Optional[str] = anchor.get("hover", "")
        text: str = anchor.text if anchor.text else ""

        if link is None:
            continue

        encoded_field: Optional[str] = explanation_map.get(link)

        if encoded_field is None:
            clean_text = text.strip().replace("<", "").replace(">", "")

            if clean_text in field_map:
                encoded_field = clean_text
            elif text in field_map:
                encoded_field = text
            else:
                pass

        if encoded_field is None:
            continue

        if encoded_field in field_map:
            bit_position, bit_width = field_map[encoded_field]
            operand_type = derive_operand_type(text, hover)

            if operand_type == "BAL_OPERAND_TYPE_NONE":
                continue



            operand: Operand = Operand(operand_type, bit_position, bit_width)

            if operand not in operands:
                operands.append(operand)

        if len(operands) >= 4:
            break

    return operands


def get_mnemonic_from_element(element: Element) -> Optional[str]:
    """
    Helper to extract 'mnemonic' from a <docvars> block inside an element.
    Returns None if not found.
    """
    docvars: Element[str]

    for docvars in element.findall("docvars"):
        docvar: Element[str]

        for docvar in docvars.findall("docvar"):
            if docvar.get("key") == "mnemonic":
                return docvar.get("value")

    return None


def parse_xml_file(filepath: str) -> List[A64Instruction]:
    try:
        tree: Optional[ElementTree[Element[str]]] = ET.parse(filepath)

        if tree is None:
            raise ET.ParseError

        root: Element[str] = tree.getroot()
    except ET.ParseError:
        print(f"Failed to parse XML file `{filepath}", file=sys.stderr)
        return []

    if root.get("type") == "alias":
        return []

    # Try to get file-level default mnemonic
    file_mnemonic: Optional[str] = get_mnemonic_from_element(root)

    # If no docvar, try the Heading/Title as a fallback for the file default.
    if file_mnemonic is None:
        heading: Optional[Element] = root.find(".//heading")

        if heading is not None and heading.text is not None:
            candidate: str = heading.text.split()[0]

            if "<" not in candidate:
                file_mnemonic = candidate

    # Unique instructions in one XML file
    # Dict[(mask, value), instruction]
    instructions: Dict[Tuple[int, int], A64Instruction] = {}

    explanation_map: Dict[str, str] = parse_explanations(root)
    iclass: Element[str]

    for iclass in root.findall(".//iclass"):

        # The diagram contains the bit definitions.
        register_diagram: Optional[Element[str]] = iclass.find("regdiagram")

        if register_diagram is None:
            continue

        # Is not a 32-bit instruction?
        if register_diagram.get("form") != "32":
            continue

        # Determine the mnemonic for this specific class.
        # Priority: Class-specific > File-default.
        class_mnemonic: Optional[str] = get_mnemonic_from_element(iclass)
        if class_mnemonic is None:
            class_mnemonic = file_mnemonic

        if class_mnemonic is None:
            class_mnemonic = "[UNKNOWN]"

        field_map: Dict[str, Tuple[int, int]] = parse_register_diagram(register_diagram)

        class_mask: int = 0
        class_value: int = 0

        # Process global diagram bits inherited by all encoding class.
        try:
            for box in register_diagram.findall("box"):
                (class_mask, class_value) = process_box(box, class_mask, class_value)
        except ValueError as e:
            print(f"Skipping malformed box in {filepath}: {e}", file=sys.stderr)
            continue

        priority: int = bin(class_mask).count("1")

        # Refine with specific encoding bits.
        # <encoding> blocks often override specific boxes to different variants.
        encoding: Element[str]

        for encoding in iclass.findall("encoding"):
            asmtemplate = encoding.find("asmtemplate")

            if asmtemplate is None:
                continue

            # Check if Encoding overrides mnemonic.
            encoding_mnemonic_temp: Optional[str] = get_mnemonic_from_element(encoding)
            encoding_mnemonic: str = (
                encoding_mnemonic_temp
                if encoding_mnemonic_temp is not None
                else class_mnemonic
            )

            encoding_mask: int = class_mask
            encoding_value: int = class_value

            try:
                for box in encoding.findall("box"):
                    (encoding_mask, encoding_value) = process_box(
                        box, encoding_mask, encoding_value
                    )
            except ValueError:
                continue

            operands: List[Operand] = parse_operands(
                asmtemplate, field_map, explanation_map
            )
            operands.sort(key=lambda x: x.bit_position)

            key: Tuple[int, int] = (encoding_mask, encoding_value)
            priority = bin(encoding_mask).count("1")

            if key not in instructions:
                instructions[key] = A64Instruction(
                    mnemonic=encoding_mnemonic,
                    mask=encoding_mask,
                    value=encoding_value,
                    priority=priority,
                    array_index=0,
                    operands=operands,
                )

    return list(instructions.values())


def derive_opcode(mnemonic: str) -> str:
    """Maps an ARM mnemonic to a Ballistic IR Opcode."""
    m = mnemonic.upper()
    if m in ["MOVZ", "MOVN", "MOVK"]:
        return "OPCODE_CONST"
    if m in ["ORR", "MOV"]:
        return "OPCODE_MOV"
    if m.startswith("ADD"):
        return "OPCODE_ADD"
    if m.startswith("SUB"):
        return "OPCODE_SUB"
    if m.startswith("MUL") or m.startswith("MADD"):
        return "OPCODE_MUL"
    if m.startswith("SDIV") or m.startswith("UDIV"):
        return "OPCODE_DIV"
    if m.startswith("AND"):
        return "OPCODE_AND"
    if m.startswith("EOR"):
        return "OPCODE_XOR"
    if m.startswith("LDR") or m.startswith("LDP"):
        return "OPCODE_LOAD"
    if m.startswith("STR") or m.startswith("STP"):
        return "OPCODE_STORE"
    if m == "B":
        return "OPCODE_JUMP"
    if m == "BL":
        return "OPCODE_CALL"
    if m == "RET":
        return "OPCODE_RETURN"
    if m.startswith("CMP"):
        return "OPCODE_CMP"

    # If we don't know what it is, map it to TRAP.
    return "OPCODE_TRAP"


def generate_hash_table(
    instructions: List[A64Instruction],
) -> Dict[int, List[A64Instruction]]:
    buckets: Dict[int, List[A64Instruction]] = {
        i: [] for i in range(DECODER_HASH_TABLE_SIZE)
    }

    # Iterate over every possible hash index to determine which instructions
    # belong in it
    for i in range(DECODER_HASH_TABLE_SIZE):
        probe_val: int = i << 21

        for inst in instructions:
            # Check if this instruction matches this hash index.
            # An instruction matches if its FIXED bits (mask) match the Probe bits
            # for the specific positions used by the hash.
            mask: int = inst.mask & DECODER_HASH_BITS_MASK
            value: int = inst.value & DECODER_HASH_BITS_MASK

            if (probe_val & mask) == value:
                buckets[i].append(inst)

        buckets[i].sort(key=lambda x: x.priority, reverse=True)

    return buckets


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generate ARM64 Decoder Tables")
    parser.add_argument(
        "--xml-directory", help="Directory containing the ARM XML files"
    )
    parser.add_argument(
        "--output-directory", help="Directory to store the generated files"
    )
    parser.add_argument("--output-header", help="Name of the generated header file")
    parser.add_argument("--output-source", help="Name of the generated source file")

    # -------------------------------------------------------------------------
    # CLI Arg Parsing
    # -------------------------------------------------------------------------
    args: argparse.Namespace = parser.parse_args()

    xml_directory: str = DEFAULT_XML_DIRECTORY_PATH
    if args.xml_directory is not None:
        xml_directory = args.xml_directory
    else:
        print(f"Using default XML directory: {DEFAULT_XML_DIRECTORY_PATH}")

    if not os.path.exists(xml_directory):
        print(f"XML directory does not exist: {xml_directory}", file=sys.stderr)
        sys.exit(1)

    output_directory: str = DEFAULT_OUTPUT_DIRECTORY
    if args.output_directory is not None:
        output_directory = args.output_directory
    else:
        print(f"Using default output directory: {DEFAULT_OUTPUT_DIRECTORY}")

    if not os.path.exists(output_directory):
        print(f"Output directory does not exit: {output_directory}", file=sys.stderr)
        sys.exit(1)

    output_header_path: str = os.path.join(
        DEFAULT_OUTPUT_DIRECTORY + DEFAULT_DECODER_GENERATED_HEADER_NAME
    )
    output_source_path: str = os.path.join(
        DEFAULT_OUTPUT_DIRECTORY + DEFAULT_DECODER_GENERATED_SOURCE_NAME
    )
    if output_directory != DEFAULT_OUTPUT_DIRECTORY:
        output_header_path = os.path.join(
            output_directory + DEFAULT_DECODER_GENERATED_HEADER_NAME
        )
        output_source_path = os.path.join(
            output_directory + DEFAULT_DECODER_GENERATED_SOURCE_NAME
        )

    if args.output_header is not None:
        output_header_path = os.path.join(output_directory + args.output_header)

    if args.output_source is not None:
        output_source_path = os.path.join(output_directory + args.output_source)

    # -------------------------------------------------------------------------
    # Process XML Files
    # -------------------------------------------------------------------------
    files = glob.glob(os.path.join(xml_directory, "*.xml"))

    if len(files) < 1:
        print(f"No XML files found in {xml_directory}")
        sys.exit(1)

    print(f"Found {len(files)} XML files")

    all_instructions: List[A64Instruction] = []
    files_to_ignore: List[str] = [os.path.join(xml_directory + "onebigfile.xml")]

    for f in files:
        # Skip index and shared pseudo-code files.
        if "index" in f or "shared" in f:
            continue

        if f in files_to_ignore:
            continue

        instructions: List[A64Instruction] = parse_xml_file(f)
        all_instructions.extend(instructions)

    for i, instruction in enumerate(all_instructions):
        instruction.array_index = i

    # -------------------------------------------------------------------------
    # Generate Header File
    # -------------------------------------------------------------------------
    with open(output_header_path, "w", encoding="utf-8") as f:
        f.write(f"{GENERATED_FILE_WARNING}\n\n")
        f.write(f'#include "{DECODER_HEADER_NAME}"\n')
        f.write("#include <stdint.h>\n")
        f.write("#include <stddef.h>\n\n")
        f.write(
            f"#define {DECODER_ARM64_INSTRUCTIONS_SIZE_NAME} {len(all_instructions)}\n\n"
        )
        f.write("typedef struct {\n")
        f.write(f"    const {DECODER_METADATA_STRUCT_NAME} *const *instructions;\n")
        f.write("    size_t count;\n")
        f.write(f"}} {DECODER_HASH_TABLE_BUCKET_STRUCT_NAME};\n\n")
        f.write(
            f"extern const {DECODER_HASH_TABLE_BUCKET_STRUCT_NAME} g_decoder_lookup_table[{DECODER_HASH_TABLE_SIZE}];\n\n"
        )
        f.write(
            f"extern const {DECODER_METADATA_STRUCT_NAME} {DECODER_ARM64_GLOBAL_INSTRUCTIONS_ARRAY_NAME}[{DECODER_ARM64_INSTRUCTIONS_SIZE_NAME}];\n"
        )
    print(f"Generated ARM decoder table header file -> {output_header_path}")

    # -------------------------------------------------------------------------
    # Generate Source File
    # -------------------------------------------------------------------------
    decoder_generated_header_name: str = DEFAULT_DECODER_GENERATED_HEADER_NAME

    if args.output_header is not None:
        decoder_generated_header_name = args.output_header

    buckets: Dict[int, List[A64Instruction]] = generate_hash_table(all_instructions)

    with open(output_source_path, "w", encoding="utf-8") as f:
        f.write(f"{GENERATED_FILE_WARNING}\n\n")
        f.write(f"/* Generated {len(all_instructions)} instructions */\n")
        f.write(f'#include "{decoder_generated_header_name}"\n\n')
        f.write(f'#include "bal_types.h"\n\n')

        f.write(
            f"const {DECODER_METADATA_STRUCT_NAME} {DECODER_ARM64_GLOBAL_INSTRUCTIONS_ARRAY_NAME}[{DECODER_ARM64_INSTRUCTIONS_SIZE_NAME}] = {{\n"
        )

        for inst in all_instructions:
            ir_opcode: str = derive_opcode(inst.mnemonic)
            operands_str: str = ""

            for i in range(4):
                if i < len(inst.operands):
                    operand: Operand = inst.operands[i]
                    operands_str += f"{{ {operand.type_enum}, {operand.bit_position}, {operand.bit_width} }},\n"
                else:
                    operands_str += "{ BAL_OPERAND_TYPE_NONE, 0, 0 },\n"

            f.write(
                f'    {{ "{inst.mnemonic}", 0x{inst.mask:08X}, 0x{inst.value:08X}, {ir_opcode},\n{{ {operands_str} }}  }},\n'
            )

        f.write("};")

        # Generate the lookup table arrays first
        array_name: str = ""
        for i in range(DECODER_HASH_TABLE_SIZE):
            if len(buckets[i]) > 0:
                # Create a unique name for this bucket's array
                array_name = f"g_bucket_{i}_instructions"
                f.write(
                    f"static const {DECODER_METADATA_STRUCT_NAME} *{array_name}[] = {{\n"
                )
                for inst in buckets[i]:
                    f.write(
                        f"    &{DECODER_ARM64_GLOBAL_INSTRUCTIONS_ARRAY_NAME}[{inst.array_index}],\n"
                    )
                f.write("};\n\n")

        # Generate the main hash table
        f.write(
            f"const {DECODER_HASH_TABLE_BUCKET_STRUCT_NAME} g_decoder_lookup_table[{DECODER_HASH_TABLE_SIZE}] = {{\n"
        )
        for i in range(DECODER_HASH_TABLE_SIZE):
            if len(buckets[i]) > 0:
                array_name = f"g_bucket_{i}_instructions"
                f.write(
                    f"    [{i:#05x}] = {{ .instructions = {array_name}, .count = {len(buckets[i])}U }},\n"
                )
            else:
                f.write(f"    [{i:#05x}] = {{ .instructions = NULL, .count = 0 }},\n")
        f.write("};\n")

    print(f"Generated ARM decoder table source file -> {output_source_path}")
