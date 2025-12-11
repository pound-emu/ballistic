"""
If you built the entire project in a build/ folder at the root directory,
you can run this program like this:

python3 fuzz_decoder.py ../build/decoder_cli
"""

import os
import re
import subprocess
import argparse
import multiprocessing
from typing import List, Tuple

MAX_ITER = 1_000
BINARY_FILE = "fuzz_decoder.bin"

# Global variable for the worker processes to access the binary path
DECODER_BIN_PATH = ""


def init_worker(binary_path):
    """Initialize globals in worker processes."""
    global DECODER_BIN_PATH
    DECODER_BIN_PATH = binary_path


def check_instruction(data: Tuple[int, str]) -> Tuple[bool, str]:
    """
    Worker function to check a single instruction.
    Input: (instruction_int_value, hex_string_for_cli)
    Output: (success_bool, error_message_or_None)
    """
    instr_val, hex_str = data

    proc = subprocess.run([DECODER_BIN_PATH, hex_str], capture_output=True)

    # Byte comparison faster than string.
    output: bytes = proc.stdout
    if output.strip() == b"UNDEFINED":
        return True, None

    try:
        out_str = output.decode("utf-8")
        parts = out_str.split(" - ")

        mask = int(parts[1].split(": ")[1], 16)
        expected = int(parts[2].split(": ")[1], 16)

        if (instr_val & mask) != expected:
            mnemonic = parts[0].split(": ")[1].strip()
            return False, (
                f"Integrity Violation For {mnemonic}: "
                f"{hex_str} & {hex(mask)} != {hex(expected)}"
            )

    except (IndexError, ValueError) as e:
        return False, f"Output Parsing Error for {hex_str}: {out_str} ({e})"

    return True, None


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Ballistic Decoder Fuzzer (Optimized)")
    parser.add_argument("decoder_binary", help="Path to the decoder cli binary file")
    parser.add_argument("--count", help="Instruction count", default=MAX_ITER, type=int)
    args = parser.parse_args()

    print(f"Generating {args.count} instructions...")
    numbers = [int.from_bytes(os.urandom(4), "big") for _ in range(args.count)]

    with open(BINARY_FILE, "wb") as f:
        for number in numbers:
            f.write(number.to_bytes(4, byteorder="little"))

    print(f"View with this command: objdump -D -b binary -m aarch64 {BINARY_FILE}")
    print("Make sure binutils is installed on your system before running")

    print(f"Disassembling with objdump...")
    result = subprocess.run(
        [
            "objdump",
            "-D",
            "-b",
            "binary",
            "-m",
            "aarch64",
            "-M",
            "no-aliases",
            BINARY_FILE,
        ],
        capture_output=True,
        text=True,
    )

    pattern = re.compile(r"^\s*[0-9a-fA-F]+:\s+([0-9a-fA-F]{8})\s+(\S+)")
    work_items: List[Tuple[int, str]] = []

    for line in result.stdout.splitlines():
        match = pattern.search(line)
        if match:
            raw_hex = match.group(1)
            int_val = int(raw_hex, 16)
            # Format as '0x...' string once here so we don't do it in the loop
            hex_str = f"0x{raw_hex}"
            work_items.append((int_val, hex_str))

    cpu_count = os.cpu_count() or 4
    print(f"Fuzzing using {cpu_count} cores...")

    successful_count = 0
    failures = []

    # Passing larger chunks to workers reduces IPC overhead
    chunk_size = max(1, len(work_items) // (cpu_count * 4))

    with multiprocessing.Pool(
        processes=cpu_count, initializer=init_worker, initargs=(args.decoder_binary,)
    ) as pool:
        for success, error_msg in pool.imap_unordered(
            check_instruction, work_items, chunksize=chunk_size
        ):
            if success:
                successful_count += 1
            else:
                failures.append(error_msg)
                print(error_msg)  # Print failures immediately

            # Progress report every 10%
            if successful_count % (max(1, len(work_items) // 10)) == 0:
                print(f"Progress: {successful_count}/{len(work_items)}")

    if not failures:
        print(f"SUCCESS: Decoded all {successful_count} instructions successfully.")
    else:
        print(f"FAILURE: Found {len(failures)} violations.")
        exit(1)
