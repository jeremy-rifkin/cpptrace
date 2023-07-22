import argparse
import os
import platform
import shutil
import subprocess
import sys
from colorama import Fore, Back, Style

from util import *

sys.stdout.reconfigure(encoding='utf-8') # for windows gh runner

failed = False

def run_command(*args: List[str]):
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
        global failed
        failed = True
        return False
    else:
        print(f"{Fore.GREEN}{Style.BRIGHT}Command succeeded{Style.RESET_ALL}")
        return True

def build(matrix):
    print(f"{Fore.BLUE}{Style.BRIGHT}{'=' * 10} Running build with config {', '.join(matrix.values())} {'=' * 10}{Style.RESET_ALL}")

    if os.path.exists("build"):
        shutil.rmtree("build")

    os.mkdir("build")
    os.chdir("build")

    if platform.system() != "Windows":
        succeeded = run_command(
            "cmake",
            "..",
            f"-DCMAKE_BUILD_TYPE={matrix['target']}",
            f"-DCMAKE_CXX_COMPILER={matrix['compiler']}",
            f"-DCMAKE_CXX_STANDARD={matrix['std']}",
            f"-D{matrix['unwind']}=On",
            f"-D{matrix['symbols']}=On",
            f"-D{matrix['demangle']}=On",
            "-DCPPTRACE_BACKTRACE_PATH=/usr/lib/gcc/x86_64-linux-gnu/10/include/backtrace.h"
        )
        if succeeded:
            run_command("make", "-j")
    else:
        args = [
            "cmake",
            "..",
            f"-DCMAKE_BUILD_TYPE={matrix['target']}",
            f"-DCMAKE_CXX_COMPILER={matrix['compiler']}",
            f"-DCMAKE_CXX_STANDARD={matrix['std']}",
            f"-D{matrix['unwind']}=On",
            f"-D{matrix['symbols']}=On",
            f"-D{matrix['demangle']}=On"
        ]
        if matrix["compiler"] == "g++":
            args.append("-GUnix Makefiles")
        succeeded = run_command(*args)
        if succeeded:
            if matrix["compiler"] == "g++":
                run_command("make", "-j")
            else:
                run_command("msbuild", "cpptrace.sln")

    os.chdir("..")
    print()

def build_full_or_auto(matrix):
    print(f"{Fore.BLUE}{Style.BRIGHT}{'=' * 10} Running build with config {'<auto>' if matrix['config'] == '' else ', '.join(matrix.values())} {'=' * 10}{Style.RESET_ALL}")

    if os.path.exists("build"):
        shutil.rmtree("build")

    os.mkdir("build")
    os.chdir("build")

    if platform.system() != "Windows":
        args = [
            "cmake",
            "..",
            f"-DCMAKE_BUILD_TYPE={matrix['target']}",
            f"-DCMAKE_CXX_COMPILER={matrix['compiler']}",
            f"-DCMAKE_CXX_STANDARD={matrix['std']}",
            f"-DCPPTRACE_BACKTRACE_PATH=/usr/lib/gcc/x86_64-linux-gnu/10/include/backtrace.h",
        ]
        if matrix["config"] != "":
            args.append(f"{matrix['config']}")
        succeeded = run_command(*args)
        if succeeded:
            run_command("make", "-j")
    else:
        args = [
            "cmake",
            "..",
            f"-DCMAKE_BUILD_TYPE={matrix['target']}",
            f"-DCMAKE_CXX_COMPILER={matrix['compiler']}",
            f"-DCMAKE_CXX_STANDARD={matrix['std']}"
        ]
        if matrix["config"] != "":
            args.append(f"{matrix['config']}")
        if matrix["compiler"] == "g++":
            args.append("-GUnix Makefiles")
        succeeded = run_command(*args)
        if succeeded:
            if matrix["compiler"] == "g++":
                run_command("make", "-j")
            else:
                run_command("msbuild", "cpptrace.sln")

    os.chdir("..")
    print()

def main():
    parser = argparse.ArgumentParser(
        prog="Build in all configs",
        description="Try building the library in all possible configurations for the current host"
    )

    if platform.system() == "Linux":
        matrix = {
            "compiler": ["g++-10", "clang++-14"],
            "target": ["Debug"],
            "std": ["11", "20"],
            "unwind": [
                "CPPTRACE_UNWIND_WITH_UNWIND",
                "CPPTRACE_UNWIND_WITH_EXECINFO",
                "CPPTRACE_UNWIND_WITH_NOTHING",
            ],
            "symbols": [
                "CPPTRACE_GET_SYMBOLS_WITH_LIBBACKTRACE",
                "CPPTRACE_GET_SYMBOLS_WITH_LIBDL",
                "CPPTRACE_GET_SYMBOLS_WITH_ADDR2LINE",
                "CPPTRACE_GET_SYMBOLS_WITH_NOTHING",
            ],
            "demangle": [
                "CPPTRACE_DEMANGLE_WITH_CXXABI",
                "CPPTRACE_DEMANGLE_WITH_NOTHING",
            ],
        }
        exclude = []
        run_matrix(matrix, exclude, build)
        matrix = {
            "compiler": ["g++-10", "clang++-14"],
            "target": ["Debug"],
            "std": ["11", "20"],
            "config": ["-DCPPTRACE_FULL_TRACE_WITH_LIBBACKTRACE=On", ""]
        }
        exclude = []
        run_matrix(matrix, exclude, build_full_or_auto)
    if platform.system() == "Darwin":
        matrix = {
            "compiler": ["g++-13", "clang++"],
            "target": ["Debug"],
            "std": ["11", "20"],
            "unwind": [
                "CPPTRACE_UNWIND_WITH_UNWIND",
                "CPPTRACE_UNWIND_WITH_EXECINFO",
                "CPPTRACE_UNWIND_WITH_NOTHING",
            ],
            "symbols": [
                #"CPPTRACE_GET_SYMBOLS_WITH_LIBBACKTRACE",
                "CPPTRACE_GET_SYMBOLS_WITH_LIBDL",
                "CPPTRACE_GET_SYMBOLS_WITH_ADDR2LINE",
                "CPPTRACE_GET_SYMBOLS_WITH_NOTHING",
            ],
            "demangle": [
                "CPPTRACE_DEMANGLE_WITH_CXXABI",
                "CPPTRACE_DEMANGLE_WITH_NOTHING",
            ]
        }
        exclude = []
        run_matrix(matrix, exclude, build)
        matrix = {
            "compiler": ["g++-13", "clang++"],
            "target": ["Debug"],
            "std": ["11", "20"],
            "config": [""]
        }
        exclude = []
        run_matrix(matrix, exclude, build_full_or_auto)
    if platform.system() == "Windows":
        parser.add_argument(
            "--clang-only",
            action="store_true"
        )
        parser.add_argument(
            "--msvc-only",
            action="store_true"
        )
        parser.add_argument(
            "--mingw-only",
            action="store_true"
        )
        args = parser.parse_args()

        compilers = ["cl", "clang++", "g++"]
        if args.clang_only:
            compilers = ["clang++"]
        if args.msvc_only:
            compilers = ["cl"]
        if args.mingw_only:
            compilers = ["g++"]

        matrix = {
            "compiler": compilers,
            "target": ["Debug"],
            "std": ["11", "20"],
            "unwind": [
                "CPPTRACE_UNWIND_WITH_WINAPI",
                "CPPTRACE_UNWIND_WITH_UNWIND",
                "CPPTRACE_UNWIND_WITH_NOTHING",
            ],
            "symbols": [
                "CPPTRACE_GET_SYMBOLS_WITH_DBGHELP",
                "CPPTRACE_GET_SYMBOLS_WITH_ADDR2LINE",
                "CPPTRACE_GET_SYMBOLS_WITH_NOTHING",
            ],
            "demangle": [
                #"CPPTRACE_DEMANGLE_WITH_CXXABI",
                "CPPTRACE_DEMANGLE_WITH_NOTHING",
            ]
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
                "symbols": "CPPTRACE_GET_SYMBOLS_WITH_DBGHELP",
                "compiler": "g++"
            },
        ]
        run_matrix(matrix, exclude, build)
        matrix = {
            "compiler": compilers,
            "target": ["Debug"],
            "std": ["11", "20"],
            "config": [""]
        }
        exclude = [
            {
                "config": "-DCPPTRACE_FULL_TRACE_WITH_LIBBACKTRACE=On"
            }
        ]
        run_matrix(matrix, exclude, build_full_or_auto)

    global failed
    if failed:
        print("🔴 Some checks failed")
        sys.exit(1)

main()
