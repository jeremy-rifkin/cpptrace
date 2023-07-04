import os
import sys
from typing import List

MAX_LINE_DIFF = 2

def similarity(name: str, target: List[str]) -> int:
    parts = name.split(".txt")[0].split(".")
    c = 0
    for part in parts:
        if part in target:
            c += 1
        else:
            return -1
    return c

def main():
    if len(sys.argv) < 2:
        print("Expected at least one arg")
        sys.exit(1)

    target = []

    if sys.argv[1].startswith("gcc") or sys.argv[1].startswith("g++"):
        target.append("gcc")
    elif sys.argv[1].startswith("clang"):
        target.append("clang")
    elif sys.argv[1].startswith("cl"):
        target.append("msvc")

    if os.name == "nt":
        target.append("windows")
    else:
        target.append("linux")

    other_configs = sys.argv[2:]
    for config in other_configs:
        assert "WITH_" in config
        target.append(config.split("WITH_")[1].lower())

    print(f"Searching for expected file best matching {target}")

    expected_dir = os.path.join(os.path.dirname(os.path.realpath(__file__)), "expected/")
    files = [f for f in os.listdir(expected_dir) if os.path.isfile(os.path.join(expected_dir, f))]
    if len(files) == 0:
        print(f"Error: No expected files to use (searching {expected_dir})", file=sys.stderr)
        sys.exit(1)
    files = list(map(lambda f: (f, similarity(f, target)), files))
    m = max(files, key=lambda entry: entry[1])[1]
    if m <= 0:
        print(f"Error: Could not find match for {target} in {files}", file=sys.stderr)
        sys.exit(1)
    files = [entry[0] for entry in files if entry[1] == m]
    if len(files) > 1:
        print(f"Error: Ambiguous expected file to use ({files})", file=sys.stderr)
        sys.exit(1)

    file = files[0]
    print(f"Reading from {file}")

    with open(os.path.join(os.path.dirname(os.path.realpath(__file__)), "expected/", file), "r") as f:
        expected = f.read()

    output = sys.stdin.read()

    print(output) # for debug reasons

    if output.strip() == "":
        print(f"Error: No output from test", file=sys.stderr)
        sys.exit(1)

    raw_output = output

    expected = [line.split("||") for line in expected.split("\n")]
    output = [line.split("||") for line in output.split("\n")]

    errored = False

    for i, ((output_file, output_line, output_symbol), (expected_file, expected_line, expected_symbol)) in enumerate(zip(output, expected)):
        if output_file != expected_file:
            print(f"Error: File name mismatch on line {i + 1}, found \"{output_file}\" expected \"{expected_file}\"", file=sys.stderr)
            errored = True
        if abs(int(output_line) - int(expected_line)) > MAX_LINE_DIFF:
            print(f"Error: File line mismatch on line {i + 1}, found {output_line} expected {expected_line}", file=sys.stderr)
            errored = True
        if output_symbol != expected_symbol:
            print(f"Error: File symbol mismatch on line {i + 1}, found \"{output_symbol}\" expected \"{expected_symbol}\"", file=sys.stderr)
            errored = True
        if expected_symbol == "main" or expected_symbol == "main()":
            break

    if errored:
        print("Test failed")
        sys.exit(1)
    else:
        print("Test passed")

main()
