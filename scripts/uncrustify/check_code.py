#!/usr/bin/env python3

import subprocess
import os
import sys
import difflib
import os.path
import json
import argparse
from typing import List

class Colors:
    __header = '\033[95m'
    __ok = '\033[92m'
    __warn = '\033[93m'
    __fail = '\033[91m'
    __correct = '\033[94m'
    __end = '\033[0m'

    @staticmethod
    def fail(msg: str) -> str:
        return f'{Colors.__fail}{msg}{Colors.__end}'

    @staticmethod
    def warn(msg: str) -> str:
        return f'{Colors.__warn}{msg}{Colors.__end}'

    @staticmethod
    def info(msg: str) -> str:
        return f'{Colors.__ok}{msg}{Colors.__end}'

    @staticmethod
    def debug(msg: str) -> str:
        return f'{Colors.__correct}{msg}{Colors.__end}'


class CodeStyleChecker:
    def __init__(self, config: str, files: str, replace: bool, verbose: bool, propose: bool, files_to_check: List[str], files_to_exclude: List[str]) -> None:
        """ Initialize Checker object.
            :param config: Path to uncrustify config file.
                :type config: str
            :param files: Path to json file with files to check.
                :type files: str
            :param replace: Replace files with formatted code.
                :type replace: bool
            :param verbose: Verbose mode.
                :type verbose: bool
            :param files_to_check: List of files to check.
                :type files_to_check: List[str]
            :param files_to_exclude: List of files to exclude.
                :type files_to_exclude: List[str]
        """
        # Params given by user
        self._cfg = config
        self._f_list = files
        self._replace = replace
        self._verbose = verbose
        self._propose = propose

        # Additional params
        self._files_to_check = []
        self._files_to_exclude = []
        self._unique_files = set()

        # Do postprocessing
        self._check_config()
        self._check_files_list()
        self._process_files_to_check_from_cmd_line(files_to_check)
        self._process_files_to_exclude_from_cmd_line(files_to_exclude)

        self._load_file_list()
        self._remove_excludes_from_list()
        self._remove_duplicates()
        self._sort_files_list()


    def __str__(self) -> str:
        """ Return string representation of Checker object. """
        return f'Code style checker:\n\tconfig={self._cfg}\n\files={self._f_list}\n\replace={self._replace}\n\verbose={self._verbose}'

    # Function used in constructor

    def _check_config(self) -> bool:
        """ Check if file with uncrusitfy configuration exists"""
        if not os.path.exists(self._cfg):
            print(Colors.fail(f'Config file {self._cfg} does not exist.'))
            return False
        return True

    def _check_files_list(self) -> bool:
        """ Check if file list exists
            :return: True if file list exists, False otherwise
                :rtype: bool
        """
        if not os.path.exists(self._f_list):
            print(Colors.fail(f'File with files to check {self._f_list} does not exist.'))
            return False
        return True

    def _process_files_to_check_from_cmd_line(self, files: List[str]) -> bool:
        """ Process files to check from command line.
            :param files: List of files to check
                :type files: List[str]
            :return: True if files were processed successfully, False otherwise
                :rtype: bool
        """
        if files is None:
            return True
        for file in files:
            if file.endswith('.*'): # Check that given is file name without extension
                base_name = file[:-2]
                for ext in os.listdir('.'):
                    if ext.startswith(base_name):
                        self._files_to_check.append(ext)
            elif file.endswith('/*'): # Check that given is folder name
                folder_name = file[:-2]
                if os.path.isdir(folder_name):
                    for root, _, files in os.walk(folder_name):
                        for f in files:
                            self._files_to_check.append(os.path.join(root, f))
                else:
                    print(Colors.warn(f'Folder {folder_name} does not exist.'))
                    return False
            else: # Check that given is file name with extension
                self._files_to_check.append(file)
        return True

    def _process_files_to_exclude_from_cmd_line(self, files: List[str]) -> bool:
        """ Process files to exclude from command line.
            :param files: List of files to exclude
                :type files: List[str]
            :return: True if files were processed successfully, False otherwise
                :rtype: bool
        """
        if files is None:
            return True
        for file in files:
            if file.endswith('.*'): # Check that given is file name without extension
                base_name = file[:-2]
                for ext in os.listdir('.'):
                    if ext.startswith(base_name):
                        self._files_to_exclude.append(ext)
            elif file.endswith('/*'): # Check that given is folder name
                folder_name = file[:-2]
                if os.path.isdir(folder_name):
                    for root, _, files in os.walk(folder_name):
                        for f in files:
                            self._files_to_exclude.append(os.path.join(root, f))
                else:
                    print(Colors.warn(f'Folder {folder_name} does not exist.'))
                    return False
            else: # Check that given is file name with extension
                self._files_to_exclude.append(file)
        return True

    def _load_file_list(self) -> bool:
        """ Load files to check from json file.
            :return: True if files were loaded successfully, False otherwise
                :rtype: bool
        """
        if not os.path.exists(self._f_list):
            print(Colors.fail(f'File with files to check {self._f_list} does not exist.'))
            return False
        with open (self._f_list, 'r') as f_json:
            f_list = json.loads(f_json.read())
            for folder in f_list['folders']:
                if not 'folder' in folder:
                    print(Colors.fail(f'Folder description does not contain folder.'))
                    return False
                folder_name = folder['folder']
                for file in folder['files']:
                    if not self._load_files_to_check(folder_name, file):
                        return False
                if 'exclude' in folder:
                    for exclude in folder['exclude']:
                        if not self._remove_excludes_from_file(folder_name, exclude):
                            return False
        return True

    def _load_files_to_check(self, folder_name: str, file_desc: dict) -> bool:
        """ Load files to check from json file.
            :param folder_name: Name of the folder with files to check
                :type folder_name: str
            :param file_desc: Description of the file to check
                :type file_desc: dict
            :return: True if files were loaded successfully, False otherwise
                :rtype: bool
        """
        if 'name' not in file_desc:
            print(Colors.fail('File description does not contain name.'))
            return False

        name = file_desc['name']
        if name != '*':
            if 'exts' not in file_desc:
                print(Colors.fail('File description does not contain exts.'))
                return False
            for ext in file_desc['exts']:
                file_path = f'{folder_name}/{name}.{ext}'
                if file_path not in self._unique_files:
                    self._unique_files.add(file_path)
                    self._files_to_check.append(file_path)
        else:
            if 'exts' not in file_desc:
                print(Colors.fail('File description does not contain exts.'))
                return False
            for file in os.listdir(folder_name):
                file_path = f'{folder_name}/{file}'
                if any(file.endswith(ext) for ext in file_desc['exts']) and file_path not in self._unique_files:
                    self._unique_files.add(file_path)
                    self._files_to_check.append(file_path)

            if file_desc.get('recursive', False):
                for root, _, files in os.walk(folder_name):
                    for file in files:
                        file_path = f'{root}/{file}'
                        if any(file.endswith(ext) for ext in file_desc['exts']) and file_path not in self._unique_files:
                            self._unique_files.add(file_path)
                            self._files_to_check.append(file_path)
        return True

    def _remove_excludes_from_file(self, folder_name: str, exclusion_desc: dict) -> bool:
        """ Remove files declared as excluded in configuration from files to check list.
            :param folder_name: Name of the folder with files to check
                :type folder_name: str
            :param exclusion_desc: Description of the files to exclude
                :type exclusion_desc: dict
            :return: True if files were excluded successfully, False otherwise
                :rtype: bool
        """
        if 'name' not in exclusion_desc:
            print(Colors.fail(f'Exclusion description does not contain name. Exclusion = {exclusion_desc}'))
            return False

        is_folder = 'folder' in exclusion_desc and exclusion_desc['folder']
        if is_folder:
            if not os.path.exists(f'{folder_name}/{exclusion_desc["name"]}'):
                print(Colors.fail(f'Folder to exclude {folder_name}/{exclusion_desc["name"]} does not exist.'))
                return False
            for root, _, files in os.walk(f'{folder_name}/{exclusion_desc["name"]}'):
                for file in files:
                    file_path = os.path.join(root, file)
                    if file_path in self._files_to_check:
                        self._files_to_check.remove(file_path)
        else:
            f_name = f'{folder_name}/{exclusion_desc["name"]}'
            if not os.path.exists(f_name):
                print(Colors.fail(f'File {f_name} does not exist.'))
                return False
            if f_name in self._files_to_check:
                self._files_to_check.remove(f_name)
            else:
                print(Colors.warn(f'File {f_name} is not in files to check list.'))
        return True

    def _remove_excludes_from_list(self) -> bool:
        """ Remove files declared as excluded in configuration from files to check list.
            :return: True if files were excluded successfully, False otherwise
                :rtype: bool
        """
        if self._files_to_exclude is None:
            return True
        for file_to_exclude in self._files_to_exclude:
            if file_to_exclude in self._files_to_check:
                self._files_to_check.remove(file_to_exclude)
            else:
                print(Colors.warn(f'File {file_to_exclude} is not in files to check list.'))
                return False
        return True

    def _remove_duplicates(self) -> None:
        """ Remove duplicates from files to check list. """
        self._files_to_check = list(set(self._files_to_check))

    def _sort_files_list(self) -> None:
        """ Sort files to check list. """
        self._files_to_check.sort()

    # Function used in check

    def _uncrustify_check_file(self, file) -> int:

        if self._verbose:
            print(f'Checking file:{file}')

        p1 = subprocess.Popen(['uncrustify', '-c', self._cfg, '-f', file, '-o', 'tmp.unc'],
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        stdout, stderr = p1.communicate()

        if p1.returncode != 0:
            raise Exception(f'Error during uncrustify formatting: {stderr}')
        if self._verbose:
            print(stdout, end='')

        uncrustify_flag = 0
        with open(file) as f, open("tmp.unc") as g:
            try:
                f_lines = f.readlines()
            except Exception as e:
                print(Colors.fail(f'{file} -> Error during parsing source file {str(e)}'), end='\n')
                return 1
            try:
                g_lines = g.readlines()
            except Exception as e:
                print(Colors.fail(f'{file} -> Error during parsing formatted file {str(e)}'), end='\n')
                return 1

            for i, (current, expected) in enumerate(zip(f_lines, g_lines), start=1):
                if current != expected:
                    self._show_line_diff(file, i, current, expected)
                    uncrustify_flag = 1

        os.remove("tmp.unc")
        if uncrustify_flag == 0 :
            if self._verbose:
                print(Colors.info(f'{file} -> Code formatting check result: PASSED'), end='\n')
            return 0
        if uncrustify_flag == 1 :
            print(Colors.fail(f'{file} -> Code formatting check result: FAILED'), end='\n')
            return 1

    def _show_line_diff(self, file: str, line_number: int, current: str, expected: str) -> None:
        print(Colors.fail(f'{file}:{line_number} Invalid formatting'), end='\n')
        if (current.rstrip('\n') == expected.rstrip('\n')):
            print(Colors.fail(f'\t-> No NL after {current.strip()}'), end='\n')
            return
        if (current.strip() == ''):
            print(Colors.fail(f'\t-> Unnecessary NL before {expected.strip()}'), end='\n')
            return
        print(Colors.fail(f'\t-> Current : {current.strip()}'), end='\n')
        print(Colors.fail(f'\t-> Expected: {expected.strip()}'), end='\n')
        if (self._propose):
            print(Colors.warn(f'\t-> Proposed changes:'), end='\n')
            for i,s in enumerate(difflib.ndiff(current, expected)):
                if s[0]==' ': continue
                elif s[0]=='-':
                    print(Colors.warn('\t\t' + u'Delete "{}" from position {}'.format(s[-1],i)))
                elif s[0]=='+':
                    print(Colors.warn('\t\t' + u'Add "{}" to position {}'.format(s[-1],i)))

    def _uncrustify_format_file(self, file) -> int:
        if self._verbose:
            print(f'Fixing formatting of file: {file}')

        p1 = subprocess.Popen(['uncrustify', '-c', self._cfg, '-f', file, '-o', file, '--no-backup'],
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        stdout, stderr = p1.communicate()

        if p1.returncode != 0:
            raise Exception(f'Error during uncrustify formatting applying: {stderr}')
        if self._verbose:
            print(stdout, end='')
        return 0


    def _check_code_style(self) -> int:
        ret = 0
        for file_to_check in self._files_to_check:
            if self._replace:
                self._uncrustify_format_file(file_to_check)
            else:
                if self._uncrustify_check_file(file_to_check) == 1:
                    ret = 1
        return ret

    def check(self) -> int:
        """ Check code style with uncrustify tool. """
        if not self._check_config():
            return 1
        print(Colors.debug('---> Start checking files with uncrustify tool. <---'))
        ret_val = self._check_code_style()
        if ret_val == 0:
            print(Colors.info('Code formatting check result: PASSED'), end='\n')
            return 0
        else:
            print(Colors.fail('Code formatting check result: FAILED'), end='\n')
            return 1


if __name__ == "__main__":

    parser = argparse.ArgumentParser(description='Check code style with uncrustify tool.')

    parser.add_argument('-s', '--style', help='Path to uncrustify style config file.', default='scripts/uncrustify/code_style.cfg')
    parser.add_argument('-l', '--list', help='Path to json file with files to check.', default='scripts/uncrustify/files_to_check.json')
    parser.add_argument('-f', '--files', help='List of files to check. Giving inf format {file_name}.* files with given name and all extension. Giving in format {folder}/* add all directory to check.', nargs='+')
    parser.add_argument('-e', '--exclude', help='List of files to exclude. Exclusion will be done after loading file list in .json file. Giving inf format {file_name}.* files with given name and all extension to exclude. Giving in format {folder}/* exclude all directory to check.', nargs='+')
    parser.add_argument('-r', '--replace', help='Replace files with formatted code.', action='store_true')
    parser.add_argument('-v', '--verbose', help='Verbose mode.', action='store_true')
    parser.add_argument('-p', '--propose', help='Propose changes to files.', action='store_true')

    args = parser.parse_args()
    checker = CodeStyleChecker(args.style, args.list, args.replace, args.verbose, args.propose, args.files, args.exclude)
    try:
        ret_val = checker.check()
    except Exception as e:
        print(Colors.fail(f'Error during checking code style: {str(e)}'))
        ret_val = 1
    sys.exit(ret_val)
