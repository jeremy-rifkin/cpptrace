import sys
import re

def main():
    output = sys.stdin.read()

    print(output)

    print("-" * 50)

    time = int(re.search(r"\d+ tests? from \d+ test suites? ran. \((\d+) ms total\)", output).group(1))

    # current threshold: 100ms
    if time > 100:
        print(f"Error: Test program took {time} ms")
        sys.exit(1)
    else:
        print(f"Success: Test program took {time} ms")

main()
