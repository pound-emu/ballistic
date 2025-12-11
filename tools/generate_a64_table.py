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

import os
import sys
import glob
import argparse
import xml.etree.ElementTree as ET
from dataclasses import dataclass
from typing import List, Tuple, Optional

DEFAULT_DECODER_GENERATED_HEADER_NAME = "decoder_table_gen.h"
DEFAULT_DECODER_GENERATED_SOURCE_NAME = "decoder_table_gen.c"
DEFAULT_OUTPUT_DIRECTORY = "../src/"
DEFAULT_XML_DIRECTORY_PATH = "../spec/arm64_xml/"

DECODER_HEADER_NAME = "decoder.h"
DECODER_METADATA_STRUCT_NAME = "bal_decoder_instruction_metadata_t"
# There is no prefix because this struct will not be public
DECODER_HASH_TABLE_BUCKET_STRUCT_NAME = "decoder_bucket_t"

DECODER_ARM64_INSTRUCTIONS_SIZE_NAME = "BAL_DECODER_ARM64_INSTRUCTIONS_SIZE"
DECODER_ARM64_HASH_TABLE_BUCKET_SIZE_NAME = "BAL_DECODER_ARM64_HASH_TABLE_BUCKET_SIZE"
DECODER_ARM64_GLOBAL_INSTRUCTIONS_ARRAY_NAME = "g_bal_decoder_arm64_instructions"

DECODER_HASH_TABLE_SIZE = 2048

# Top 11 bits
DECODER_HASH_BITS_MASK = 0xFFE00000

GENERATED_FILE_WARNING = """/*
 *GENERATED FILE - DO NOT EDIT
 *Generated with tools/generate_a64_table.py
 */"""


@dataclass
class A64Instruction:
    mnemonic: str
    mask: int
    value: int
    priority: int  # Higher number of set bits in mask = higher priority.
    array_index: int


def process_box(
    box: ET.Element, current_mask: int, current_value: int
) -> Tuple[int, int]:
    """Process a specific bit-field box fron the XML diagram."""
    try:
        # The high-bit position in the 32-bit word.
        hibit = int(box.attrib.get("hibit"))
        # The width of this bitfield.
        width = int(box.attrib.get("width", "1"))
    except TypeError:
        return (current_mask, current_value)

    if hibit >= 32:
        raise ValueError(f"Bit positiin {hibit} exceeds 31")

    # Get the constraint content (e.g., "11", "0", "101")
    # If 'c' is missing, it's usually a variable field (register/imm)
    c_elements = box.findall("c")
    content = ""
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


def get_mnemonic_from_element(element: ET.Element) -> Optional[str]:
    """
    Helper to extract 'mnemonic' from a <docvars> block inside an element.
    Returns None if not found.
    """
    for docvars in element.findall("docvars"):
        for docvar in docvars.findall("docvar"):
            if docvar.get("key") == "mnemonic":
                return docvar.get("value")
    return None


def parse_xml_file(
    filepath: str, total_instructions_size: int
) -> Tuple[List[A64Instruction], int]:
    try:
        tree = ET.parse(filepath)
        root = tree.getroot()
    except ET.ParseError:
        print(f"Failed to parse XML file `{filepath}", file=sys.stderr)
        return ([], total_instructions_size)

    if root.get("type") == "alias":
        return ([], total_instructions_size)

    # Try to get file-level default mnemonic
    file_mnemonic: Optional[str] = get_mnemonic_from_element(root)

    # If no docvar, try the Heading/Title as a fallback for the file default.
    if file_mnemonic is None:
        heading = root.find(".//heading")
        if heading is not None and heading.text is not None:
            candidate = heading.text.split()[0]
            if "<" not in candidate:
                file_mnemonic = candidate

    # Instructions in one XML file
    instructions: List[A64Instruction] = []

    # Extract mask and value.
    for iclass in root.findall(".//iclass"):

        # The diagram box contains the bit definitions.
        box_diagram = iclass.find("regdiagram")
        if box_diagram is None:
            continue

        # Is 32-bit instruction?
        if box_diagram.get("form") != "32":
            continue

        # Determine the mnemonic for this specific class.
        # Priority: Class-specific > File-default.
        class_mnemonic: Optional[str] = get_mnemonic_from_element(iclass)
        if class_mnemonic is None:
            class_mnemonic = file_mnemonic

        if class_mnemonic is None:
            class_mnemnonic = "[UNKNOWN]"

        class_mask: int = 0
        class_value: int = 0

        # Process global diagram bits inherited by all encoding class.
        try:
            for box in box_diagram.findall("box"):
                (class_mask, class_value) = process_box(box, class_mask, class_value)
        except ValueError as e:
            print(f"Skipping malformed box in {filepath}: {e}", file=sys.stderr)
            continue

        priority: int = bin(class_mask).count("1")

        # Check for negatives
        class_instruction = A64Instruction(
            mnemonic=class_mnemonic,
            mask=class_mask,
            value=class_value,
            priority=priority,
            array_index=total_instructions_size,
        )

        instructions.append(class_instruction)
        total_instructions_size += 1

        # Refine with specific encoding bits.
        # <encoding> blocks often override specific boxes to different variants.
        for encoding in iclass.findall("encoding"):
            asm_template = encoding.find("asmtemplate")
            if asm_template is None:
                continue

            # Check if Encoding overrides mnemonic.
            encoding_mnemonic_temp: Optional[str] = get_mnemonic_from_element(encoding)
            encoding_mnemonic = (
                encoding_mnemonic_temp
                if encoding_mnemonic_temp is not None
                else class_mnemonic
            )

            encoding_mask = class_mask
            encoding_value = class_value

            try:
                for box in encoding.findall("box"):
                    (total_mask, total_value) = process_box(
                        box, encoding_mask, encoding_value
                    )
            except ValueError:
                continue

        priority: int = bin(encoding_mask).count("1")

        encoding_instruction = A64Instruction(
            mnemonic=encoding_mnemonic,
            mask=encoding_mask,
            value=encoding_value,
            priority=priority,
            array_index=total_instructions_size - 1,  #
        )
        if encoding_instruction != class_instruction:
            total_instructions_size += 1
            encoding_instruction.array_index = total_instructions_size
            instructions.append(encoding_instruction)

    return (instructions, total_instructions_size)


def generate_hash_table(instructions):
    buckets = {i: [] for i in range(DECODER_HASH_TABLE_SIZE)}

    # Iterate over every possible hash index to determine which instructions
    # belong in it
    for i in range(DECODER_HASH_TABLE_SIZE):
        probe_val = i << 21

        for inst in instructions:
            # Check if this instruction matches this hash index.
            # An instruction matches if its FIXED bits (mask) match the Probe bits
            # for the specific positions used by the hash.

            mask = inst.mask & DECODER_HASH_BITS_MASK
            value = inst.value & DECODER_HASH_BITS_MASK

            if (probe_val & mask) == value:
                buckets[i].append(inst)
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
    args = parser.parse_args()

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
    instructions_size: int = 0
    files_to_ignore: List[str] = [os.path.join(xml_directory + "onebigfile.xml")]
    for f in files:
        # Skip index and shared pseudo-code files.
        if "index" in f or "shared" in f:
            continue

        if f in files_to_ignore:
            continue

        instructions: List[A64Instruction]
        (instructions, instructions_size) = parse_xml_file(f, instructions_size)
        all_instructions.extend(instructions)

    # TODO(GloriousTacoo): Sorting by priority puts instructions in the wrong
    # bucket and I have no idea why.
    # all_instructions.sort(key=lambda x: x.priority, reverse=True)

    # -------------------------------------------------------------------------
    # Generate Header File
    # -------------------------------------------------------------------------
    with open(output_header_path, "w") as f:
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

    buckets = generate_hash_table(all_instructions)
    with open(output_source_path, "w") as f:
        f.write(f"{GENERATED_FILE_WARNING}\n\n")
        f.write(f"/* Generated {len(all_instructions)} instructions */\n")
        f.write(f'#include "{decoder_generated_header_name}"\n\n')
        f.write(
            f"const {DECODER_METADATA_STRUCT_NAME} {DECODER_ARM64_GLOBAL_INSTRUCTIONS_ARRAY_NAME}[{DECODER_ARM64_INSTRUCTIONS_SIZE_NAME}] = {{\n"
        )
        for inst in all_instructions:
            f.write(
                f'    {{ "{inst.mnemonic}", 0x{inst.mask:08X}, 0x{inst.value:08X} }}, \n'
            )
        f.write("};")

        # Generate the lookup table arrays first
        for i in range(DECODER_HASH_TABLE_SIZE):
            if len(buckets[i]) > 0:
                # Create a unique name for this bucket's array
                array_name: str = f"g_bucket_{i}_instructions"
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
                array_name: str = f"g_bucket_{i}_instructions"
                f.write(
                    f"    [{i:#05x}] = {{ .instructions = {array_name}, .count = {len(buckets[i])}U }},\n"
                )
            else:
                f.write(f"    [{i:#05x}] = {{ .instructions = NULL, .count = 0 }},\n")
        f.write("};\n")

    print(f"Generated ARM decoder table source file -> {output_source_path}")
