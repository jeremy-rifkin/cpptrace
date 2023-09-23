import subprocess
import sys
import itertools
from typing import List
from colorama import Fore, Back, Style
import re

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

def adj_width(text):
    return len(text) - len(ansi_escape.sub("", text))

def do_exclude(matrix_config, exclude):
    return all(map(lambda k: matrix_config[k] == exclude[k], exclude.keys()))

def print_table(table):
    columns = len(table[0])
    column_widths = [1 for _ in range(columns)]
    for row in table:
        for i, cell in enumerate(row):
            column_widths[i] = max(column_widths[i], len(ansi_escape.sub("", cell)))
    for j, cell in enumerate(table[0]):
        print("| {cell:{width}} ".format(cell=cell, width=column_widths[j] + adj_width(cell)), end="")
    print("|")
    for i, row in enumerate(table[1:]):
        for j, cell in enumerate(row):
            print("| {cell:{width}} ".format(cell=cell, width=column_widths[j] + adj_width(cell)), end="")
        print("|")

def run_matrix(matrix, exclude, fn):
    keys = [*matrix.keys()]
    values = [*matrix.values()]
    #print("Values:", values)
    results = {} # insertion-ordered
    for config in itertools.product(*matrix.values()):
        #print(config)
        matrix_config = {}
        for k, v in zip(matrix.keys(), config):
            matrix_config[k] = v
        #print(matrix_config)
        if any(map(lambda ex: do_exclude(matrix_config, ex), exclude)):
            continue
        else:
            config_tuple = tuple(values[i].index(p) for i, p in enumerate(config))
            results[config_tuple] = fn(matrix_config)
            # Fudged data for testing
            #print(config_tuple)
            #if "symbols" not in matrix_config:
            #    results[config_tuple] = matrix_config["compiler"] != "g++-10"
            #else:
            #    results[config_tuple] = not (matrix_config["compiler"] == "clang++-14" and matrix_config["symbols"] == "CPPTRACE_GET_SYMBOLS_WITH_ADDR2LINE")
    # I had an idea for printing 2d slices of the n-dimentional matrix, but it didn't pan out as much as I'd hoped
    dimensions = len(values)
    # # Output diagnostic tables
    # print("Results:", results)
    # if dimensions >= 2:
    #     for iteraxes in itertools.combinations(range(dimensions), dimensions - 2):
    #         # iteraxes are the axes we iterate over to slice, these fixed axes are the axes of the table
    #         # just the complement of axes, these are the two fixed axes
    #         fixed = [x for x in range(dimensions) if x not in iteraxes]
    #         assert(len(fixed) == 2)
    #         if any([len(values[i]) == 1 for i in fixed]):
    #             continue
    #         print("Fixed:", fixed)
    #         for iteraxesvalues in itertools.product(
    #             *[range(len(values[i])) if i in iteraxes else [-1] for i in range(dimensions)]
    #         ):
    #             print(">>", iteraxesvalues)
    #             # Now that we have our iteraxes values we have a unique plane
    #             table = [
    #                 ["", *[value for value in values[fixed[0]]]]
    #             ]
    #             #print(values[fixed[1]])
    #             for row_i, row_value in enumerate(values[fixed[1]]):
    #                 row = [row_value]
    #                 for col_i in range(len(values[fixed[0]])):
    #                     iteraxesvaluescopy = [x for x in iteraxesvalues]
    #                     iteraxesvaluescopy[fixed[1]] = row_i
    #                     iteraxesvaluescopy[fixed[0]] = col_i
    #                     #print("----->", iteraxesvaluescopy)
    #                     row.append(
    #                         f"{Fore.GREEN}{Style.BRIGHT}Good{Style.RESET_ALL}"
    #                             if results[tuple(iteraxesvaluescopy)]
    #                                 else f"{Fore.RED}{Style.BRIGHT}Bad{Style.RESET_ALL}"
    #                             if tuple(iteraxesvaluescopy) in results else ""
    #                     )
    #                 table.append(row)
    #             print_table(table)

    # Better idea would be looking for m<n tuples that are consistently failing and reporting on those
    #for fixed_axes in itertools.product(range(dimensions), 2):
    #    pass

    print("Results:")
    table = [keys]
    for result in results:
        table.append([
            f"{Fore.GREEN if results[result] else Fore.RED}{Style.BRIGHT}{values[i][v]}{Style.RESET_ALL}"
                for i, v in enumerate(result)
        ])
    print_table(table)
