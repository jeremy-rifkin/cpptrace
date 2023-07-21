import sys
import re

def main():
    output = sys.stdin.read()

    print(output)

    print("-" * 50)

    time = int(re.search(r"\d+ tests? from \d+ test suites? ran. \((\d+) ms total\)", output).group(1))

    dwarf4 = any(["DWARF4" in arg for arg in sys.argv[1:]])
    dwarf5 = any(["DWARF5" in arg for arg in sys.argv[1:]])
    expect_slow = dwarf4

    threshold = 100 # ms

    if expect_slow:
        if time > 100:
            print(f"Success (expecting slow): Test program took {time} ms")
        else:
            print(f"Error (expecting slow): Test program took {time} ms")
            sys.exit(1)
    else:
        if time > 100:
            print(f"Error: Test program took {time} ms")
            sys.exit(1)
        else:
            print(f"Success: Test program took {time} ms")


main()
