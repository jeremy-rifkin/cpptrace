import argparse
import os
import platform
import shutil
import subprocess
import sys

from util import *

sys.stdout.reconfigure(encoding='utf-8') # for windows gh runner

failed = False

def run_command(*args: List[str]):
    print("[🔵 Running Command \"{}\"]".format(" ".join(args)))
    p = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = p.communicate()
    print("\033[0m", end="") # makefile in parallel sometimes messes up colors
    if p.returncode != 0:
        print("[🔴 Command `{}` failed]".format(" ".join(args)))
        print("stdout:")
        print(stdout.decode("utf-8"), end="")
        print("stderr:")
        print(stderr.decode("utf-8"), end="")
        global failed
        failed = True
        return False
    else:
        print("[🟢 Command `{}` succeeded]".format(" ".join(args)))
        return True

def run_test(test_binary, *driver_args: List[str]):
    print("[🔵 Running Command \"{} | {}\"]".format(test_binary, " ".join(driver_args)))
    test = subprocess.Popen([test_binary], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    driver = subprocess.Popen(driver_args, stdin=test.stdout, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    test.wait()
    test_stderr = test.stderr.read()
    driver_stdout, driver_stderr = driver.communicate()
    print("\033[0m", end="") # makefile in parallel sometimes messes up colors
    if test.returncode != 0 or driver.returncode != 0:
        print("[🔴 Command `{} |{}` failed]".format(test_binary, " ".join(driver_args)))
        print("test stderr:")
        print(test_stderr.decode("utf-8"), end="")
        print("stdout:")
        print(driver_stdout.decode("utf-8"), end="")
        print("stderr:")
        print(driver_stderr.decode("utf-8"), end="")
        global failed
        failed = True
        return False
    else:
        print("[🟢 Command `{} | {}` succeeded]".format(test_binary, " ".join(driver_args)))
        return True

def build(matrix):
    if platform.system() != "Windows":
        args = [
            "cmake",
            "..",
            f"-DCMAKE_BUILD_TYPE={matrix['target']}",
            f"-DCMAKE_CXX_COMPILER={matrix['compiler']}",
            f"-DCMAKE_CXX_STANDARD={matrix['std']}",
            f"-D{matrix['unwind']}=On",
            f"-D{matrix['symbols']}=On",
            f"-D{matrix['demangle']}=On",
            "-DCPPTRACE_BACKTRACE_PATH=/usr/lib/gcc/x86_64-linux-gnu/10/include/backtrace.h",
            "-DCPPTRACE_BUILD_TEST=On",
            "-DBUILD_SHARED_LIBS=On"
        ]
        if matrix['symbols'] == "CPPTRACE_GET_SYMBOLS_WITH_LIBDL":
           args.append("-DCPPTRACE_BUILD_TEST_RDYNAMIC=On")
        succeeded = run_command(*args)
        if succeeded:
            return run_command("make", "-j")
    else:
        succeeded = run_command(
            "cmake",
            "..",
            f"-DCMAKE_BUILD_TYPE={matrix['target']}",
            f"-DCMAKE_CXX_COMPILER={matrix['compiler']}",
            f"-DCMAKE_CXX_STANDARD={matrix['std']}",
            f"-D{matrix['unwind']}=On",
            f"-D{matrix['symbols']}=On",
            f"-D{matrix['demangle']}=On",
            "-DCPPTRACE_BUILD_TEST=On",
            "-DBUILD_SHARED_LIBS=On"
        )
        if succeeded:
            return run_command("msbuild", "cpptrace.sln")

def test(matrix):
    if platform.system() != "Windows":
        run_test(
            "./test",
            "python3",
            "../test/test.py",
            matrix["compiler"],
            matrix["unwind"],
            matrix["symbols"],
            matrix["demangle"]
        )
    else:
        run_test(
            f".\\{matrix['target']}\\test.exe",
            "python3",
            "../test/test.py",
            matrix["compiler"],
            matrix["unwind"],
            matrix["symbols"],
            matrix["demangle"]
        )

def build_and_test(matrix):
    print(matrix)

    if os.path.exists("build"):
        shutil.rmtree("build")

    os.mkdir("build")
    os.chdir("build")

    if build(matrix):
        test(matrix)

    os.chdir("..")
    print()

def build_full_or_auto(matrix):
    if platform.system() != "Windows":
        args = [
            "cmake",
            "..",
            f"-DCMAKE_BUILD_TYPE={matrix['target']}",
            f"-DCMAKE_CXX_COMPILER={matrix['compiler']}",
            f"-DCMAKE_CXX_STANDARD={matrix['std']}",
            f"-DCPPTRACE_BACKTRACE_PATH=/usr/lib/gcc/x86_64-linux-gnu/10/include/backtrace.h",
            "-DCPPTRACE_BUILD_TEST=On",
            "-DBUILD_SHARED_LIBS=On"
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
            f"-DCMAKE_CXX_STANDARD={matrix['std']}",
            "-DCPPTRACE_BUILD_TEST=On",
            "-DBUILD_SHARED_LIBS=On"
        ]
        if matrix["config"] != "":
            args.append(f"{matrix['config']}")
        print(args)
        succeeded = run_command(*args)
        if succeeded:
            return run_command("msbuild", "cpptrace.sln")

def test_full_or_auto(matrix):
    if platform.system() != "Windows":
        run_test(
            "./test",
            "python3",
            "../test/test.py",
            matrix["compiler"]
        )
    else:
        run_test(
            f".\\{matrix['target']}\\test.exe",
            "python3",
            "../test/test.py",
            matrix["compiler"]
        )

def build_and_test_full_or_auto(matrix):
    print(matrix)

    if os.path.exists("build"):
        shutil.rmtree("build")

    os.mkdir("build")
    os.chdir("build")

    if build_full_or_auto(matrix):
        test_full_or_auto(matrix)

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
                "CPPTRACE_UNWIND_WITH_EXECINFO",
                "CPPTRACE_UNWIND_WITH_UNWIND",
                #"CPPTRACE_UNWIND_WITH_NOTHING",
            ],
            "symbols": [
                "CPPTRACE_GET_SYMBOLS_WITH_LIBBACKTRACE",
                "CPPTRACE_GET_SYMBOLS_WITH_LIBDL",
                "CPPTRACE_GET_SYMBOLS_WITH_ADDR2LINE",
                #"CPPTRACE_GET_SYMBOLS_WITH_NOTHING",
            ],
            "demangle": [
                "CPPTRACE_DEMANGLE_WITH_CXXABI",
                #"CPPTRACE_DEMANGLE_WITH_NOTHING",
            ],
        }
        exclude = []
        run_matrix(matrix, exclude, build_and_test)
        matrix = {
            "compiler": ["g++-10", "clang++-14"],
            "target": ["Debug"],
            "std": ["11", "20"],
            "config": ["-DCPPTRACE_FULL_TRACE_WITH_LIBBACKTRACE=On", ""]
        }
        exclude = []
        run_matrix(matrix, exclude, build_and_test_full_or_auto)
    if platform.system() == "Darwin":
        matrix = {
            "compiler": ["g++-13", "clang++"],
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
                #"CPPTRACE_GET_SYMBOLS_WITH_ADDR2LINE",
                #"CPPTRACE_GET_SYMBOLS_WITH_NOTHING",
            ],
            "demangle": [
                "CPPTRACE_DEMANGLE_WITH_CXXABI",
                #"CPPTRACE_DEMANGLE_WITH_NOTHING",
            ]
        }
        exclude = []
        run_matrix(matrix, exclude, build_and_test)
        matrix = {
            "compiler": ["g++-13", "clang++"],
            "target": ["Debug"],
            "std": ["11", "20"],
            "config": [""]
        }
        exclude = []
        run_matrix(matrix, exclude, build_and_test_full_or_auto)
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
            "std": ["11", "20"],
            "unwind": [
                "CPPTRACE_UNWIND_WITH_WINAPI",
                #"CPPTRACE_UNWIND_WITH_NOTHING",
            ],
            "symbols": [
                "CPPTRACE_GET_SYMBOLS_WITH_DBGHELP",
                #"CPPTRACE_GET_SYMBOLS_WITH_NOTHING",
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
        run_matrix(matrix, exclude, build_and_test)
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
        run_matrix(matrix, exclude, build_and_test_full_or_auto)

    global failed
    if failed:
        print("🔴 Some checks failed")
        sys.exit(1)

main()
