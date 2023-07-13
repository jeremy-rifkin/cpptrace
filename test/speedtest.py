import sys
import re

def main():
    output = sys.stdin.read()

    time = int(re.search(r'\d+ tests? from \d+ test suites? ran. \((\d+) ms total\)', output).group(1))

    # current threshold: 100ms
    if time > 100:
        print(f"Error: Test program took {time} ms")
        sys.exit(1)
    else:
        print(f"Test program took {time} ms")

main()
