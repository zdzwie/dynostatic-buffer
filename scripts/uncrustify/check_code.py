import subprocess
import os
import sys

import difflib
from difflib import unified_diff
class uncrustify_tool:
    def __init__(self, path: str) :
        self.path_to_uncrustify = path

    def check_file(self, file: str) -> None:
        tmp = open("tmp.txt", "w")
        tmp.close()
        p1 = subprocess.Popen([self.path_to_uncrustify,'-c','scripts/uncrustify/code_style.cfg', '-f', file, '-o', 'tmp.txt'], stderr=subprocess.PIPE)
        p1.communicate()
        uncrustify_flag = 0
        with open(file) as f, open("tmp.txt") as g:
            f_lines = f.readlines()
            g_lines = g.readlines()
            for line in difflib.unified_diff(f_lines, g_lines) :
                if line.find("+++") == -1 :
                    uncrustify_flag = 1
                print(line, end="")

        if uncrustify_flag == 0 :
            print(file + " code formatting check result: PASSED", end="\n")
        if uncrustify_flag == 1 :
            print(file + " code formatting check result: FAILED", end="\n")
            self.uncrustify_pass_flag = 1
        os.remove("tmp.txt")


tool = uncrustify_tool(sys.argv[1])
tool.check_file("library/dynostatic-buffer.c")