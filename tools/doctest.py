import re
import sys
import os
import argparse
import subprocess
import tempfile

CC = "clang"
CFLAGS = ["-Wall", "-Werror"]


def extract_code_blocks(filename: str):
    with open(filename, 'r') as f:
        lines = f.readlines()

    blocks = []
    current_block = []
    in_code_block = False

    for line in lines:
        raw = line.strip()

        content = ""
        if raw.startswith("//!"):
            content = raw[3:]
        elif raw.startswith("///"):
            content = raw[3:]
        else:
            continue

        if content.startswith(" "):
            content = content[1:]

        if content.startswith("```c"):
            in_code_block = True
            continue

        if content.startswith("```") and in_code_block:
            in_code_block = False

            if current_block:
                blocks.append("\n".join(current_block))
                current_block = []

            continue

        if in_code_block:
            current_block.append(content)

    return blocks


def test_file(header_file: str, include_directories: List[str], source_files: List[str]):
    print(f"Testing examples in {header_file}...")
    blocks = extract_code_blocks(header_file)

    success: bool = True

    if not blocks:
        print("No exampels found.")
        return success

    for i, code in enumerate(blocks):
        if "// ---" in code:
            parts = code.split("// ---")
            global_section = parts[0]
            main_section = parts[1]
            test_source_file: str = f"""
            {global_section}
            int main(void) {{
                {main_section}
                return 0;
            }}
            """
        else:
            test_source_file: str = f"""
            int main(void)
            {{
                {code}
                return 0;
            }}
            """

        with tempfile.NamedTemporaryFile(suffix='.c', mode='w', delete=False) as tmp:
            tmp.write(test_source_file)
            tmp_name = tmp.name

        exe_name = tmp_name.replace(".c", "")
        command: str = [CC] + ["-I"] + include_directories + CFLAGS + [tmp_name] + source_files + ["-o", exe_name]
        result = subprocess.run(command, capture_output=True, text=True)

        if result.returncode != 0:
            print(f"[FAIL] Example {i+1}")
            print("Compilation failed:")
            print(result.stderr)
            success = False
        else:
            print(f"[PASS] Example {i+1}")
            subprocess.run([exe_name])

        try:
            os.remove(tmp_name)
            if os.path.exists(exe_name):
                os.remove(exe_name)
        except OSError:
            pass

    return success

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Run C documentation examples.")
    parser.add_argument("header", help="The header file to parse")
    parser.add_argument("--sources", nargs='+', help="Implementation source files to link against")
    parser.add_argument("--includes", nargs='+', help="Include directories")

    args = parser.parse_args()

    if not test_file(args.header, args.includes, args.sources):
        sys.exit(1)
