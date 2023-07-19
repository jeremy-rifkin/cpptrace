import subprocess
import sys
import itertools
from typing import List

def do_exclude(matrix_config, exclude):
    return all(map(lambda k: matrix_config[k] == exclude[k], exclude.keys()))

def run_matrix(matrix, exclude, fn):
    #print(matrix.values())
    for config in itertools.product(*matrix.values()):
        #print(config)
        matrix_config = {}
        for k, v in zip(matrix.keys(), config):
            matrix_config[k] = v
        #print(matrix_config)
        if any(map(lambda ex: do_exclude(matrix_config, ex), exclude)):
            continue
        else:
            fn(matrix_config)
