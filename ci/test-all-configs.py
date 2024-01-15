import argparse
import os
import platform
import shutil
import subprocess
import sys
from typing import Tuple
from colorama import Fore, Back, Style

from util import *

sys.stdout.reconfigure(encoding='utf-8') # for windows gh runner

failed = False

expected_dir = os.path.join(os.path.dirname(os.path.realpath(__file__)), "../test/expected/")

def get_c_compiler_counterpart(compiler: str) -> str:
    return compiler.replace("clang++", "clang").replace("g++", "gcc")

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

def output_matches(output: str, params: Tuple[str]):
    target = []

    if params[0].startswith("gcc") or params[0].startswith("g++"):
        target.append("gcc")
    elif params[0].startswith("clang"):
        target.append("clang")
    elif params[0].startswith("cl"):
        target.append("msvc")

    if platform.system() == "Windows":
        target.append("windows")
    elif platform.system() == "Darwin":
        target.append("macos")
    else:
        target.append("linux")

    other_configs = params[1:]
    for config in other_configs:
        assert "WITH_" in config
        target.append(config.split("WITH_")[1].lower())

    print(f"Searching for expected file best matching {target}")

    files = [f for f in os.listdir(expected_dir) if os.path.isfile(os.path.join(expected_dir, f))]
    if len(files) == 0:
        print(f"Error: No expected files to use (searching {expected_dir})")
        sys.exit(1)
    files = list(map(lambda f: (f, similarity(f, target)), files))
    m = max(files, key=lambda entry: entry[1])[1]
    if m <= 0:
        print(f"Error: Could not find match for {target} in {files}")
        sys.exit(1)
    files = [entry[0] for entry in files if entry[1] == m]
    if len(files) > 1:
        print(f"Error: Ambiguous expected file to use ({files})")
        sys.exit(1)

    file = files[0]
    print(f"Reading from {file}")

    with open(os.path.join(expected_dir, file), "r") as f:
        expected = f.read()

    if output.strip() == "":
        print(f"Error: No output from test")
        return False

    expected = [line.strip().split("||") for line in expected.split("\n")]
    output = [line.strip().split("||") for line in output.split("\n")]

    max_line_diff = 0

    errored = False

    for i, ((output_file, output_line, output_symbol), (expected_file, expected_line, expected_symbol)) in enumerate(zip(output, expected)):
        if output_file != expected_file:
            print(f"Error: File name mismatch on line {i + 1}, found \"{output_file}\" expected \"{expected_file}\"")
            errored = True
        if abs(int(output_line) - int(expected_line)) > max_line_diff:
            print(f"Error: File line mismatch on line {i + 1}, found {output_line} expected {expected_line}")
            errored = True
        if output_symbol != expected_symbol:
            print(f"Error: File symbol mismatch on line {i + 1}, found \"{output_symbol}\" expected \"{expected_symbol}\"")
            errored = True
        if expected_symbol == "main" or expected_symbol == "main()":
            break

    return not errored

def run_command(*args: List[str]):
    global failed
    print(f"{Fore.CYAN}{Style.BRIGHT}Running Command \"{' '.join(args)}\"{Style.RESET_ALL}")
    p = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = p.communicate()
    print(Style.RESET_ALL, end="") # makefile in parallel sometimes messes up colors
    if p.returncode != 0:
        print(f"{Fore.RED}{Style.BRIGHT}Command failed{Style.RESET_ALL}")
        print("stdout:")
        print(stdout.decode("utf-8"), end="")
        print("stderr:")
        print(stderr.decode("utf-8"), end="")
        failed = True
        return False
    else:
        print(f"{Fore.GREEN}{Style.BRIGHT}Command succeeded{Style.RESET_ALL}")
        return True

def run_test(test_binary, params: Tuple[str]):
    global failed
    print(f"{Fore.CYAN}{Style.BRIGHT}Running test{Style.RESET_ALL}")
    test = subprocess.Popen([test_binary], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    test_stdout, test_stderr = test.communicate()
    print(Style.RESET_ALL, end="") # makefile in parallel sometimes messes up colors

    if test.returncode != 0:
        print("[🔴 Test command failed]")
        print("stderr:")
        print(test_stderr.decode("utf-8"), end="")
        print("stdout:")
        print(test_stdout.decode("utf-8"), end="")
        failed = True
        return False
    else:
        if len(test_stderr) != 0:
            print("stderr:")
            print(test_stderr.decode("utf-8"), end="")
        if output_matches(test_stdout.decode("utf-8"), params):
            print(f"{Fore.GREEN}{Style.BRIGHT}Test succeeded{Style.RESET_ALL}")
            return True
        else:
            print(f"{Fore.RED}{Style.BRIGHT}Test failed{Style.RESET_ALL}")
            failed = True
            return False

def build(matrix):
    if platform.system() != "Windows":
        args = [
            "cmake",
            "..",
            f"-DCMAKE_BUILD_TYPE={matrix['target']}",
            f"-DCMAKE_CXX_COMPILER={matrix['compiler']}",
            f"-DCMAKE_C_COMPILER={get_c_compiler_counterpart(matrix['compiler'])}",
            f"-DCMAKE_CXX_STANDARD={matrix['std']}",
            f"-DCPPTRACE_USE_EXTERNAL_LIBDWARF=On",
            f"-D{matrix['unwind']}=On",
            f"-D{matrix['symbols']}=On",
            f"-D{matrix['demangle']}=On",
            "-DCPPTRACE_BACKTRACE_PATH=/usr/lib/gcc/x86_64-linux-gnu/10/include/backtrace.h",
            "-DCPPTRACE_BUILD_TESTING=On",
            f"-DBUILD_SHARED_LIBS={matrix['shared']}"
        ]
        if matrix['symbols'] == "CPPTRACE_GET_SYMBOLS_WITH_LIBDL":
           args.append("-DCPPTRACE_BUILD_TEST_RDYNAMIC=On")
        succeeded = run_command(*args)
        if succeeded:
            return run_command("make", "-j")
    else:
        args = [
            "cmake",
            "..",
            f"-DCMAKE_BUILD_TYPE={matrix['target']}",
            f"-DCMAKE_CXX_COMPILER={matrix['compiler']}",
            f"-DCMAKE_C_COMPILER={get_c_compiler_counterpart(matrix['compiler'])}",
            f"-DCMAKE_CXX_STANDARD={matrix['std']}",
            f"-DCPPTRACE_USE_EXTERNAL_LIBDWARF=On",
            f"-D{matrix['unwind']}=On",
            f"-D{matrix['symbols']}=On",
            f"-D{matrix['demangle']}=On",
            "-DCPPTRACE_BUILD_TESTING=On",
            f"-DBUILD_SHARED_LIBS={matrix['shared']}"
        ]
        if matrix["compiler"] == "g++":
            args.append("-GUnix Makefiles")
        succeeded = run_command(*args)
        if succeeded:
            if matrix["compiler"] == "g++":
                return run_command("make", "-j")
            else:
                return run_command("msbuild", "cpptrace.sln")
    return False

def build_full_or_auto(matrix):
    if platform.system() != "Windows":
        args = [
            "cmake",
            "..",
            f"-DCMAKE_BUILD_TYPE={matrix['target']}",
            f"-DCMAKE_CXX_COMPILER={matrix['compiler']}",
            f"-DCMAKE_C_COMPILER={get_c_compiler_counterpart(matrix['compiler'])}",
            f"-DCMAKE_CXX_STANDARD={matrix['std']}",
            f"-DCPPTRACE_USE_EXTERNAL_LIBDWARF=On",
            f"-DCPPTRACE_BACKTRACE_PATH=/usr/lib/gcc/x86_64-linux-gnu/10/include/backtrace.h",
            "-DCPPTRACE_BUILD_TESTING=On",
            f"-DBUILD_SHARED_LIBS={matrix['shared']}"
        ]
        if matrix["config"] != "":
            args.append(f"{matrix['config']}")
        succeeded = run_command(*args)
        if succeeded:
            return run_command("make", "-j")
    else:
        args = [
            "cmake",
            "..",
            f"-DCMAKE_BUILD_TYPE={matrix['target']}",
            f"-DCMAKE_CXX_COMPILER={matrix['compiler']}",
            f"-DCMAKE_C_COMPILER={get_c_compiler_counterpart(matrix['compiler'])}",
            f"-DCMAKE_CXX_STANDARD={matrix['std']}",
            f"-DCPPTRACE_USE_EXTERNAL_LIBDWARF=On",
            "-DCPPTRACE_BUILD_TESTING=On",
            f"-DBUILD_SHARED_LIBS={matrix['shared']}"
        ]
        if matrix["config"] != "":
            args.append(f"{matrix['config']}")
        if matrix["compiler"] == "g++":
            args.append("-GUnix Makefiles")
        succeeded = run_command(*args)
        if succeeded:
            if matrix["compiler"] == "g++":
                return run_command("make", "-j")
            else:
                return run_command("msbuild", "cpptrace.sln")
    return False

def test(matrix):
    if platform.system() != "Windows":
        return run_test(
            "./test",
            (matrix["compiler"], matrix["unwind"], matrix["symbols"], matrix["demangle"])
        )
    else:
        if matrix["compiler"] == "g++":
            return run_test(
                f".\\test.exe",
                (matrix["compiler"], matrix["unwind"], matrix["symbols"], matrix["demangle"])
            )
        else:
            return run_test(
                f".\\{matrix['target']}\\test.exe",
                (matrix["compiler"], matrix["unwind"], matrix["symbols"], matrix["demangle"])
            )

def test_full_or_auto(matrix):
    if platform.system() != "Windows":
        return run_test(
            "./test",
            (matrix["compiler"],)
        )
    else:
        if matrix["compiler"] == "g++":
            return run_test(
                f".\\test.exe",
                (matrix["compiler"],)
            )
        else:
            return run_test(
                f".\\{matrix['target']}\\test.exe",
                (matrix["compiler"],)
            )

def build_and_test(matrix):
    print(f"{Fore.BLUE}{Style.BRIGHT}{'=' * 10} Running build and test with config {', '.join(matrix.values())} {'=' * 10}{Style.RESET_ALL}")

    if os.path.exists("build"):
        shutil.rmtree("build", ignore_errors=True)

    if not os.path.exists("build"):
        os.mkdir("build")
    os.chdir("build")

    good = False
    if build(matrix):
        good = test(matrix)

    os.chdir("..")
    print()

    return good

def build_and_test_full_or_auto(matrix):
    print(f"{Fore.BLUE}{Style.BRIGHT}{'=' * 10} Running build and test with config {'<auto>' if matrix['config'] == '' else ', '.join(matrix.values())} {'=' * 10}{Style.RESET_ALL}")

    if os.path.exists("build"):
        shutil.rmtree("build", ignore_errors=True)

    if not os.path.exists("build"):
        os.mkdir("build")
    os.chdir("build")

    good = False
    if build_full_or_auto(matrix):
        good = test_full_or_auto(matrix)

    os.chdir("..")
    print()

    return good

def main():
    parser = argparse.ArgumentParser(
        prog="Build in all configs",
        description="Try building the library in all possible configurations for the current host"
    )
    parser.add_argument(
        "--clang",
        action="store_true"
    )
    parser.add_argument(
        "--gcc",
        action="store_true"
    )
    parser.add_argument(
        "--msvc",
        action="store_true"
    )
    parser.add_argument(
        "--all",
        action="store_true"
    )
    parser.add_argument(
        "--shared",
        action="store_true"
    )
    args = parser.parse_args()

    if platform.system() == "Linux":
        compilers = []
        if args.clang or args.all:
            compilers.append("clang++-14")
        if args.gcc or args.all:
            compilers.append("g++-10")
        matrix = {
            "compiler": compilers,
            "target": ["Debug"],
            "std": ["11", "20"],
            "unwind": [
                "CPPTRACE_UNWIND_WITH_EXECINFO",
                "CPPTRACE_UNWIND_WITH_UNWIND",
                "CPPTRACE_UNWIND_WITH_LIBUNWIND",
                #"CPPTRACE_UNWIND_WITH_NOTHING",
            ],
            "symbols": [
                # Disabled due to libbacktrace bug
                # "CPPTRACE_GET_SYMBOLS_WITH_LIBBACKTRACE",
                "CPPTRACE_GET_SYMBOLS_WITH_LIBDL",
                "CPPTRACE_GET_SYMBOLS_WITH_ADDR2LINE",
                "CPPTRACE_GET_SYMBOLS_WITH_LIBDWARF",
                #"CPPTRACE_GET_SYMBOLS_WITH_NOTHING",
            ],
            "demangle": [
                "CPPTRACE_DEMANGLE_WITH_CXXABI",
                #"CPPTRACE_DEMANGLE_WITH_NOTHING",
            ],
            "shared": ["On" if args.shared else "Off"]
        }
        exclude = []
        run_matrix(matrix, exclude, build_and_test)
        matrix = {
            "compiler": compilers,
            "target": ["Debug"],
            "std": ["11", "20"],
            "config": [""],
            "shared": ["On" if args.shared else "Off"]
        }
        exclude = []
        run_matrix(matrix, exclude, build_and_test_full_or_auto)
    if platform.system() == "Darwin":
        compilers = []
        if args.clang or args.all:
            compilers.append("clang++")
        if args.gcc or args.all:
            compilers.append("g++-12")
        matrix = {
            "compiler": compilers,
            "target": ["Debug"],
            "std": ["11", "20"],
            "unwind": [
                "CPPTRACE_UNWIND_WITH_EXECINFO",
                "CPPTRACE_UNWIND_WITH_UNWIND",
                #"CPPTRACE_UNWIND_WITH_NOTHING",
            ],
            "symbols": [
                #"CPPTRACE_GET_SYMBOLS_WITH_LIBBACKTRACE",
                "CPPTRACE_GET_SYMBOLS_WITH_LIBDL",
                "CPPTRACE_GET_SYMBOLS_WITH_ADDR2LINE",
                "CPPTRACE_GET_SYMBOLS_WITH_LIBDWARF",
                #"CPPTRACE_GET_SYMBOLS_WITH_NOTHING",
            ],
            "demangle": [
                "CPPTRACE_DEMANGLE_WITH_CXXABI",
                #"CPPTRACE_DEMANGLE_WITH_NOTHING",
            ],
            "shared": ["On" if args.shared else "Off"]
        }
        exclude = []
        run_matrix(matrix, exclude, build_and_test)
        matrix = {
            "compiler": compilers,
            "target": ["Debug"],
            "std": ["11", "20"],
            "config": [""],
            "shared": ["On" if args.shared else "Off"]
        }
        exclude = []
        run_matrix(matrix, exclude, build_and_test_full_or_auto)
    if platform.system() == "Windows":
        compilers = []
        if args.clang or args.all:
            compilers.append("clang++")
        if args.msvc or args.all:
            compilers.append("cl")
        if args.gcc or args.all:
            compilers.append("g++")
        matrix = {
            "compiler": compilers,
            "target": ["Debug"],
            "std": ["11", "20"],
            "unwind": [
                "CPPTRACE_UNWIND_WITH_WINAPI",
                "CPPTRACE_UNWIND_WITH_DBGHELP",
                "CPPTRACE_UNWIND_WITH_UNWIND", # Broken on github actions for some reason
                #"CPPTRACE_UNWIND_WITH_NOTHING",
            ],
            "symbols": [
                "CPPTRACE_GET_SYMBOLS_WITH_DBGHELP",
                "CPPTRACE_GET_SYMBOLS_WITH_ADDR2LINE",
                "CPPTRACE_GET_SYMBOLS_WITH_LIBDWARF",
                #"CPPTRACE_GET_SYMBOLS_WITH_NOTHING",
            ],
            "demangle": [
                "CPPTRACE_DEMANGLE_WITH_CXXABI",
                "CPPTRACE_DEMANGLE_WITH_NOTHING",
            ],
            "shared": ["On" if args.shared else "Off"]
        }
        exclude = [
            {
                "demangle": "CPPTRACE_DEMANGLE_WITH_CXXABI",
                "compiler": "cl"
            },
            {
                "unwind": "CPPTRACE_UNWIND_WITH_UNWIND",
                "compiler": "cl"
            },
            {
                "unwind": "CPPTRACE_UNWIND_WITH_UNWIND",
                "compiler": "clang++"
            },
            {
                "symbols": "CPPTRACE_GET_SYMBOLS_WITH_ADDR2LINE",
                "compiler": "cl"
            },
            {
                "symbols": "CPPTRACE_GET_SYMBOLS_WITH_ADDR2LINE",
                "compiler": "clang++"
            },
            {
                "symbols": "CPPTRACE_GET_SYMBOLS_WITH_LIBDWARF",
                "compiler": "cl"
            },
            {
                "symbols": "CPPTRACE_GET_SYMBOLS_WITH_LIBDWARF",
                "compiler": "clang++"
            },
            {
                "symbols": "CPPTRACE_GET_SYMBOLS_WITH_DBGHELP",
                "compiler": "g++"
            },
            {
                "symbols": "CPPTRACE_GET_SYMBOLS_WITH_DBGHELP",
                "demangle": "CPPTRACE_DEMANGLE_WITH_CXXABI"
            },
            {
                "symbols": "CPPTRACE_GET_SYMBOLS_WITH_LIBDWARF",
                "demangle": "CPPTRACE_DEMANGLE_WITH_NOTHING"
            },
            {
                "symbols": "CPPTRACE_GET_SYMBOLS_WITH_ADDR2LINE",
                "demangle": "CPPTRACE_DEMANGLE_WITH_NOTHING"
            }
        ]
        run_matrix(matrix, exclude, build_and_test)
        matrix = {
            "compiler": compilers,
            "target": ["Debug"],
            "std": ["11", "20"],
            "config": [""],
            "shared": ["On" if args.shared else "Off"]
        }
        exclude = []
        run_matrix(matrix, exclude, build_and_test_full_or_auto)

    global failed
    if failed:
        print("🔴 Some checks failed")
        sys.exit(1)

main()
