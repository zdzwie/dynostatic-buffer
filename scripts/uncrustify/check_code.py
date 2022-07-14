import subprocess
import os
import sys
import json

import difflib
from difflib import unified_diff

class console_colors:
    HEADER = '\033[95m'
    OK = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    CORRECT = '\033[94m'
    END = '\033[0m'


class uncrustify_tool:
    def __init__(self, path: str) :
        self.path_to_uncrustify = path

    def check_file(self, file: str) -> None:
        tmp = open('tmp.txt', 'w')
        tmp.close()
        p1 = subprocess.Popen([self.path_to_uncrustify,'-c','scripts/uncrustify/code_style.cfg', '-f', file, '-o', 'tmp.txt'], stderr=subprocess.PIPE)
        p1.communicate()
        uncrustify_flag = 0
        with open(file) as f, open('tmp.txt') as g:
            f_lines = f.readlines()
            g_lines = g.readlines()
            for line in difflib.unified_diff(f_lines, g_lines) :
                if line.find('+++') == -1 :
                    uncrustify_flag = 1
                print(line, end='')

        if uncrustify_flag == 0 :
            print(f'{console_colors.OK} {file} -> Code formatting check result: PASSED {console_colors.END}', end='\n')
        if uncrustify_flag == 1 :
            print(f'{console_colors.FAIL} {file} -> Code formatting check result: FAILED {console_colors.END}', end='\n')
            self.uncrustify_pass_flag = 1
        os.remove('tmp.txt')

    def check_code_style(self) -> None:
        with open ('scripts/uncrustify/files_to_check.json', 'r') as f_json:
            f_list = json.loads(f_json.read())
            for folder in f_list['folders']:
                for file in folder['list']:
                    for extension in file['exts']:
                        self.check_file(f"{folder['folder']}/{file['name']}.{extension}")



tool = uncrustify_tool(sys.argv[1])
print(f'{console_colors.CORRECT} ---> Start checking files with uncrustify tool. <--- {console_colors.END}')
tool.check_code_style()