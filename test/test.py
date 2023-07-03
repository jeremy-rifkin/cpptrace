import os
import sys

MAX_LINE_DIFF = 2

def main():
    if len(sys.argv) != 2:
        print("Expected one argument")
        sys.exit(1)

    if sys.argv[1].startswith("gcc") or sys.argv[1].startswith("g++"):
        name = "gcc"
    elif sys.argv[1].startswith("clang"):
        name = "clang"
    elif sys.argv[1].startswith("cl"):
        name = "msvc"
    if os.name == "nt":
        name = "windows_" + name

    with open(os.path.join(os.path.dirname(os.path.realpath(__file__)), "expected/", name + ".txt"), "r") as f:
        expected = f.read()

    output = sys.stdin.read()

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
        print("Test output:", file=sys.stderr)
        print(raw_output, file=sys.stderr)
        sys.exit(1)

main()
