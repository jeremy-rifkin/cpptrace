import subprocess
import sys
import itertools
from typing import List, Dict
from colorama import Fore, Back, Style
import re
import time

# https://stackoverflow.com/a/14693789/15675011
ansi_escape = re.compile(r'''
    \x1B  # ESC
    (?:   # 7-bit C1 Fe (except CSI)
        [@-Z\\-_]
    |     # or [ for CSI, followed by a control sequence
        \[
        [0-?]*  # Parameter bytes
        [ -/]*  # Intermediate bytes
        [@-~]   # Final byte
    )
''', re.VERBOSE)

class MatrixRunner:
    def __init__(self, matrix, exclude, strata=None):
        self.matrix = matrix
        self.exclude = exclude
        self.strata = strata or []
        self.slices = self.parse_slices()
        strata_keys = list(self.strata[0].keys()) if self.strata else []
        self.keys = list(matrix.keys()) + strata_keys
        self.values = (
            [list(v) for v in matrix.values()]
            + [list(dict.fromkeys(v for s in self.strata for v in s[k])) for k in strata_keys]
        )
        self.results = {} # insertion-ordered
        self.failed = False
        self.work = self.get_work()

        self.last_matrix_config = None
        self.current_matrix_config = None

    def parse_slices(self) -> Dict[str, List[str]]:
        slices: Dict[str, List[str]] = dict()
        for arg in sys.argv:
            if arg.startswith("--slice="):
                rest = arg[len("--slice="):]
                key, value = rest.split(":")
                if key not in slices:
                    slices[key] = []
                slices[key].append(value)
        return slices

    def run_command(self, *args: List[str], always_output=False, output_matcher=None) -> bool:
        self.log(f"{Fore.CYAN}{Style.BRIGHT}Running Command \"{' '.join(args)}\"{Style.RESET_ALL}")
        start_time = time.time()
        p = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        stdout, stderr = p.communicate()
        runtime = time.time() - start_time
        self.log(Style.RESET_ALL, end="") # makefile in parallel sometimes messes up colors
        if p.returncode != 0:
            self.log(f"{Fore.RED}{Style.BRIGHT}Command failed{Style.RESET_ALL} {Fore.MAGENTA}(time: {runtime:.2f}s){Style.RESET_ALL}")
            self.log("stdout:")
            self.log(stdout.decode("utf-8"), end="")
            self.log("stderr:")
            self.log(stderr.decode("utf-8"), end="")
            self.failed = True
            return False
        else:
            self.log(f"{Fore.GREEN}{Style.BRIGHT}Command succeeded{Style.RESET_ALL} {Fore.MAGENTA}(time: {runtime:.2f}s){Style.RESET_ALL}")
            if always_output:
                self.log("stdout:")
                self.log(stdout.decode("utf-8"), end="")
                self.log("stderr:")
                self.log(stderr.decode("utf-8"), end="")
            elif len(stderr) != 0:
                self.log("stderr:")
                self.log(stderr.decode("utf-8"), end="")
            if output_matcher is not None:
                if not output_matcher(stdout.decode("utf-8")):
                    self.failed = True
                    return False
            return True

    def set_fail(self):
        self.failed = True

    def current_config(self):
        return self.current_matrix_config

    def last_config(self):
        return self.last_matrix_config

    def log(self, *args, **kwargs):
        print(*args, **kwargs, flush=True)

    def do_exclude(self, config, exclude):
        return all(config[k] == exclude[k] for k in exclude)

    def do_slice(self, config, slices):
        return not slices or all(config[k] in slices[k] for k in slices)

    def assignment_to_matrix_config(self, assignment):
        return dict(zip(self.keys, assignment))

    def get_work(self):
        work = []
        if self.strata:
            seen = set()
            strata_configs = []
            strata_keys = list(self.strata[0].keys())
            for stratum in self.strata:
                for vals in itertools.product(*[stratum[k] for k in strata_keys]):
                    if vals not in seen:
                        seen.add(vals)
                        strata_configs.append(vals)
        else:
            strata_configs = [()]
        for base_vals in itertools.product(*self.matrix.values()):
            for strata_vals in strata_configs:
                assignment = base_vals + strata_vals
                config = self.assignment_to_matrix_config(assignment)
                if any(self.do_exclude(config, ex) for ex in self.exclude):
                    continue
                if not self.do_slice(config, self.slices):
                    continue
                work.append(assignment)
        return work

    def run(self, fn):
        for i, assignment in enumerate(self.work):
            matrix_config = self.assignment_to_matrix_config(assignment)
            config_tuple = tuple(self.values[j].index(p) for j, p in enumerate(assignment))
            config_str = ', '.join(str(v) for v in matrix_config.values())
            if config_str == "":
                self.log(f"{Fore.BLUE}{Style.BRIGHT}{'=' * 10} [{i + 1}/{len(self.work)}] Running with blank config {'=' * 10}{Style.RESET_ALL}")
            else:
                self.log(f"{Fore.BLUE}{Style.BRIGHT}{'=' * 10} [{i + 1}/{len(self.work)}] Running with config {config_str} {'=' * 10}{Style.RESET_ALL}")
            self.last_matrix_config = self.current_matrix_config
            self.current_matrix_config = matrix_config
            self.results[config_tuple] = fn(self)
        self.print_results()
        if self.failed:
            self.log("🔴 Some checks failed")
            sys.exit(1)
        else:
            self.log("🟢 All checks passed")

    def adj_width(self, text):
        return len(text) - len(ansi_escape.sub("", text))

    def print_table(self, table):
        columns = len(table[0])
        column_widths = [1 for _ in range(columns)]
        for row in table:
            for i, cell in enumerate(row):
                column_widths[i] = max(column_widths[i], len(ansi_escape.sub("", cell)))
        for j, cell in enumerate(table[0]):
            self.log("| {cell:{width}} ".format(cell=cell, width=column_widths[j] + self.adj_width(cell)), end="")
        self.log("|")
        for i, row in enumerate(table[1:]):
            for j, cell in enumerate(row):
                self.log("| {cell:{width}} ".format(cell=cell, width=column_widths[j] + self.adj_width(cell)), end="")
            self.log("|")

    def print_results(self):
        self.log("Results:")
        table = [self.keys]
        for result in self.results:
            table.append([
                f"{Fore.GREEN if self.results[result] else Fore.RED}{Style.BRIGHT}{self.values[i][v]}{Style.RESET_ALL}"
                    for i, v in enumerate(result)
            ])
        self.print_table(table)
