import argparse
import os
import platform
import shutil
import subprocess
import sys

from util import *

sys.stdout.reconfigure(encoding='utf-8') # for windows gh runner

def run_command(*args: List[str]):
    print("[🔵 Running Command \"{}\"]".format(" ".join(args)))
    p = subprocess.Popen(args)
    p.wait()
    print("\033[0m") # makefile in parallel sometimes messes up colors
    if p.returncode != 0:
        print("[🔴 Command \"{}\" failed]".format(" ".join(args)))
    else:
        print("[🟢 Command \"{}\" succeeded]".format(" ".join(args)))

def build(matrix):
    print(matrix)

    if os.path.exists("build"):
        shutil.rmtree("build")

    os.mkdir("build")
    os.chdir("build")

    run_command(
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

    if not (platform.system() == "Windows"):
        run_command("make", "-j")
    else:
        run_command("msbuild", "cpptrace.sln")

    os.chdir("..")

def build_full_or_auto(matrix):
    print(matrix)

    run_command(
        "cmake",
        "..",
        f"-DCMAKE_BUILD_TYPE={matrix['target']}",
        f"-DCMAKE_CXX_COMPILER={matrix['compiler']}",
        f"-DCMAKE_CXX_STANDARD={matrix['std']}",
        f"-DCPPTRACE_BACKTRACE_PATH=/usr/lib/gcc/x86_64-linux-gnu/10/include/backtrace.h",
        f"{matrix['config']}"
    )

    if not (platform.system() == "Windows"):
        run_command("make", "-j")
    else:
        run_command("msbuild", "cpptrace.sln")

    os.chdir("..")

def main():
    parser = argparse.ArgumentParser(
        prog="Build in all configs",
        description="Try building the library in all possible configurations for the current host"
    )

    if platform.system() == "Linux":
        matrix = {
            "compiler": ["g++-10", "clang++-14"],
            "target": ["Debug"],
            "std": [11, 20],
            "unwind": [
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
            "std": [11, 20],
            "config": ["-DCPPTRACE_FULL_TRACE_WITH_LIBBACKTRACE=On", ""]
        }
        exclude = []
        run_matrix(matrix, exclude, build_full_or_auto)
    if platform.system() == "Darwin":
        matrix = {
            "compiler": ["g++-13", "clang++"],
            "target": ["Debug"],
            "std": [11, 20],
            "unwind": [
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
            "std": [11, 20],
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
        args = parser.parse_args()

        compilers = ["cl", "clang++"]
        if args.clang_only:
            compilers = ["clang++"]
        if args.msvc_only:
            compilers = ["cl"]

        matrix = {
            "compiler": compilers,
            "target": ["Debug"],
            "std": [11, 20],
            "unwind": [
                "CPPTRACE_UNWIND_WITH_WINAPI",
                "CPPTRACE_UNWIND_WITH_NOTHING",
            ],
            "symbols": [
                "CPPTRACE_GET_SYMBOLS_WITH_DBGHELP",
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
            }
        ]
        run_matrix(matrix, exclude, build)
        matrix = {
            "compiler": compilers,
            "target": ["Debug"],
            "std": [11, 20],
            "config": [""]
        }
        exclude = [
            {
                "config": "-DCPPTRACE_FULL_TRACE_WITH_LIBBACKTRACE=On"
            }
        ]
        run_matrix(matrix, exclude, build_full_or_auto)

main()
