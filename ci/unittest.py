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

def get_c_compiler_counterpart(compiler: str) -> str:
    return compiler.replace("clang++", "clang").replace("g++", "gcc")

def build(runner: MatrixRunner):
    if platform.system() == "Linux":
        matrix = runner.current_config()
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
            "-DCPPTRACE_STD_FORMAT=Off",
            "-DCPPTRACE_BUILD_TESTING=On",
            f"-DCPPTRACE_SANITIZER_BUILD={matrix['sanitizers']}",
            f"-DCPPTRACE_BUILD_TESTING_SPLIT_DWARF={matrix['split_dwarf']}",
            f"-DCPPTRACE_BUILD_TESTING_SPLIT_DWARF={matrix['dwarf_version']}",
            f"-DCPPTRACE_USE_EXTERNAL_LIBDWARF=On",
            f"-DCPPTRACE_USE_EXTERNAL_ZSTD=On",
            f"-DCPPTRACE_USE_EXTERNAL_GTEST=On",
            f"-DCMAKE_INSTALL_PREFIX=~/tmp/foo",
        ]
        return runner.run_command(*args) and runner.run_command("ninja")
    elif platform.system() == "Darwin":
        matrix = runner.current_config()
        args = [
            "cmake",
            "..",
            "-GNinja",
            f"-DCMAKE_CXX_COMPILER={matrix['compiler']}",
            f"-DCMAKE_C_COMPILER={get_c_compiler_counterpart(matrix['compiler'])}",
            f"-DCMAKE_BUILD_TYPE={matrix['build_type']}",
            f"-DBUILD_SHARED_LIBS={matrix['shared']}",
            "-DCPPTRACE_WERROR_BUILD=On",
            "-DCPPTRACE_STD_FORMAT=Off",
            "-DCPPTRACE_BUILD_TESTING=On",
            f"-DCPPTRACE_SANITIZER_BUILD={matrix['sanitizers']}",
            # f"-DCPPTRACE_BUILD_TESTING_SPLIT_DWARF={matrix['split_dwarf']}",
            # f"-DCPPTRACE_BUILD_TESTING_SPLIT_DWARF={matrix['dwarf_version']}",
            f"-DCPPTRACE_USE_EXTERNAL_LIBDWARF=On",
            f"-DCPPTRACE_USE_EXTERNAL_ZSTD=On",
            f"-DCPPTRACE_USE_EXTERNAL_GTEST=On",
            f"-DCMAKE_PREFIX_PATH={'/tmp/gtest_install_gcc' if 'g++' in matrix['compiler'] else '/tmp/gtest_install'}",
        ]
        return runner.run_command(*args) and runner.run_command("ninja")
    else:
        raise ValueError()

def test(runner: MatrixRunner):
    if platform.system() != "Windows":
        return runner.run_command("./unittest") and runner.run_command("bash", "-c", "exec -a u ./unittest")
    else:
        raise ValueError()

def build_and_test(runner: MatrixRunner):
    # the build directory has to be purged on compiler or shared change
    last = runner.last_config()
    current = runner.current_config()
    if last is None or last["compiler"] != current["compiler"] or last["shared"] != current["shared"]:
        if os.path.exists("build"):
            shutil.rmtree("build", ignore_errors=True)

    if not os.path.exists("build"):
        os.mkdir("build")
    os.chdir("build")

    good = False
    if build(runner):
        good = test(runner)

    os.chdir("..")
    print(flush=True)

    return good

def run_linux_matrix():
    MatrixRunner(
        matrix = {
            "compiler": ["g++-10", "clang++-14"],
            "shared": ["OFF", "ON"],
            "build_type": ["Debug", "RelWithDebInfo"],
            "sanitizers": ["OFF", "ON"],
            "has_dl_find_object": ["OFF", "ON"],
            "split_dwarf": ["OFF", "ON"],
            "dwarf_version": ["4", "5"],
        },
        exclude = []
    ).run(build_and_test)

def run_macos_matrix():
    MatrixRunner(
        matrix = {
            "compiler": ["g++-12", "clang++"],
            "shared": ["OFF", "ON"],
            "build_type": ["Debug", "RelWithDebInfo"],
            "sanitizers": ["OFF", "ON"],
            # "split_dwarf": ["OFF", "ON"],
            # "dwarf_version": ["4", "5"],
        },
        exclude = [
            {
                "compiler": "g++-12",
                "sanitizers": "ON",
            }
        ]
    ).run(build_and_test)

def main():
    if platform.system() == "Linux":
        run_linux_matrix()
    if platform.system() == "Darwin":
        run_macos_matrix()
    if platform.system() == "Windows":
        raise ValueError() # run_windows_matrix()

main()
