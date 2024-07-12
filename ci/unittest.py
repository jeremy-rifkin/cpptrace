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

def run_command(*args: List[str], always_output=False):
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
        if always_output:
            print("stdout:")
            print(stdout.decode("utf-8"), end="")
            print("stderr:")
            print(stderr.decode("utf-8"), end="")
        return True

def run_test(test_binary):
    global failed
    print(f"{Fore.CYAN}{Style.BRIGHT}Running test{Style.RESET_ALL}")
    test = subprocess.Popen([test_binary], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    test_stdout, test_stderr = test.communicate()
    print(Style.RESET_ALL, end="") # makefile in parallel sometimes messes up colors

    if test.returncode != 0:
        print(f"[🔴 Test command failed with code {test.returncode}]")
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
        print(f"{Fore.GREEN}{Style.BRIGHT}Test succeeded{Style.RESET_ALL}")
        return True

def build(matrix):
    if platform.system() != "Windows":
        args = [
            "cmake",
            "..",
            "-GNinja",
            f"-DCMAKE_CXX_COMPILER={matrix['compiler']}",
            f"-DCMAKE_C_COMPILER={get_c_compiler_counterpart(matrix['compiler'])}",
            f"-DCMAKE_BUILD_TYPE={matrix['build_type']}",
            f"-DBUILD_SHARED_LIBS={matrix['shared']}",
            f"-DHAS_DL_FIND_OBJECT={matrix['has_dl_find_object']}",
            "-DCPPTRACE_WERROR_BUILD=On",
            "-DCPPTRACE_BUILD_TESTING=On",
            f"-DCPPTRACE_SANITIZER_BUILD={matrix['sanitizers']}",
            f"-DCPPTRACE_BUILD_TESTING_SPLIT_DWARF={matrix['split_dwarf']}",
            f"-DCPPTRACE_BUILD_TESTING_SPLIT_DWARF={matrix['dwarf_version']}",
            f"-DCPPTRACE_USE_EXTERNAL_LIBDWARF=On",
            f"-DCPPTRACE_USE_EXTERNAL_ZSTD=On",
        ]
        succeeded = run_command(*args)
        if succeeded:
            return run_command("ninja")
    else:
        # args = [
        #     "cmake",
        #     "..",
        #     f"-DCMAKE_BUILD_TYPE={matrix['target']}",
        #     f"-DCMAKE_CXX_COMPILER={matrix['compiler']}",
        #     f"-DCMAKE_C_COMPILER={get_c_compiler_counterpart(matrix['compiler'])}",
        #     f"-DCMAKE_CXX_STANDARD={matrix['std']}",
        #     f"-DCPPTRACE_USE_EXTERNAL_LIBDWARF=On",
        #     f"-DCPPTRACE_USE_EXTERNAL_ZSTD=On",
        #     f"-DCPPTRACE_WERROR_BUILD=On",
        #     f"-D{matrix['unwind']}=On",
        #     f"-D{matrix['symbols']}=On",
        #     f"-D{matrix['demangle']}=On",
        #     "-DCPPTRACE_BUILD_TESTING=On",
        #     "-DCPPTRACE_SKIP_UNIT=On",
        #     f"-DBUILD_SHARED_LIBS={matrix['shared']}"
        # ]
        # if matrix["compiler"] == "g++":
        #     args.append("-GUnix Makefiles")
        # succeeded = run_command(*args)
        # if succeeded:
        #     if matrix["compiler"] == "g++":
        #         return run_command("make", "-j")
        #     else:
        #         return run_command("msbuild", "cpptrace.sln")
        raise ValueError()
    return False

def test(matrix):
    if platform.system() != "Windows":
        return run_test(
            "./unittest",
        ) and run_command("bash", "-c", "exec -a u ./unittest")
    else:
        raise ValueError()
        # if matrix["compiler"] == "g++":
        #     return run_test(
        #         f".\\integration.exe",
        #         (matrix["compiler"], matrix["unwind"], matrix["symbols"], matrix["demangle"])
        #     )
        # else:
        #     return run_test(
        #         f".\\{matrix['target']}\\integration.exe",
        #         (matrix["compiler"], matrix["unwind"], matrix["symbols"], matrix["demangle"])
        #     )

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

def run_linux_matrix():
    matrix = {
        "compiler": ["g++-10", "clang++-14"],
        "shared": ["OFF", "ON"],
        "build_type": ["Debug", "RelWithDebInfo"],
        "has_dl_find_object": ["OFF", "ON"],
        "sanitizers": ["OFF", "ON"],
        "split_dwarf": ["OFF", "ON"],
        "dwarf_version": ["4", "5"],
    }
    exclude = []
    run_matrix(matrix, exclude, build_and_test)

def main():
    if platform.system() == "Linux":
        run_linux_matrix()
    if platform.system() == "Darwin":
        raise ValueError() # run_macos_matrix()
    if platform.system() == "Windows":
        raise ValueError() # run_windows_matrix()

    global failed
    if failed:
        print("🔴 Some checks failed")
        sys.exit(1)

main()
