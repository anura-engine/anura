#!/usr/bin/env python3

#   Provide `os.walk(1)`.
import os

#   Provide `sys.exit(1)`.
import sys


STATUS_SUCCESS = 0


SOURCES_BASE = '../../src/'  #   TODO Stop relying on this.


def analyze(dir_, doc_name):
    doc_full_name = dir_ + '/' + doc_name
    with open(doc_full_name) as input_doc:
        input_doc_lines = input_doc.readlines()
        line_index = 1
        for input_doc_line in input_doc_lines:
            # print(input_doc_line)
            if (True
                    and 'FUNCTION_DEF' in input_doc_line
                    and 'FUNCTION_DEF_CTOR' not in input_doc_line
                    and 'FUNCTION_DEF_MEMBERS' not in input_doc_line
                    and 'FUNCTION_DEF_IMPL' not in input_doc_line
                    and 'END_FUNCTION_DEF' not in input_doc_line
                    and '//' not in input_doc_line
                    and '#define' not in input_doc_line
                    and True):
                # print('matched line ' + str(line_index) + " of file '" +
                #       doc_full_name + "':\n\t" + input_doc_line)
                input_doc_line_explode = input_doc_line.split('"')
                # print(input_doc_line_explode)
                #   TODO Add pluggable and parametrized output capability.
                simplified_input_doc_line_candidate = input_doc_line_explode[1]
                if len(simplified_input_doc_line_candidate) == 0:
                    #   The function is not documented. Pick the function name
                    # and signature, hold it as `code` and add a mark to
                    # request the documentation string to get fixed.
                    a = input_doc_line.split('(')
                    b = a[1]
                    c = b.split(',')
                    d = c[0]
                    simplified_input_doc_line = d
                else:
                    simplified_input_doc_line = (
                        simplified_input_doc_line_candidate)
                if simplified_input_doc_line[-1:] != '.':
                    simplified_input_doc_line = simplified_input_doc_line + '.'
                #   TODO Add the `code` ('`') marks where they must be.
                #
                #   FIXME Fix known bugs, such as for several text strings
                # appearing, like:
                #
                #     * convert rgb to hsv
                #
                #     * convert hsv to rgb
                #
                #   There are maybe half a dozen or more of those.
                #
                #   TODO Change every other debugging print into a logging call
                # leaving the meaningful `print(...)` calls alone.
                print('* ' + simplified_input_doc_line)
                print('')
            line_index += 1


def run():
    cur_path = []
    for dir_walk in os.walk(SOURCES_BASE):
        current_dir = dir_walk[0]
        # print('current_dir: ' + current_dir)
        cur_path.append(current_dir)
        # print('cur_path: ' + str(cur_path))
        dir_subdirectories = dir_walk[1]
        # print('dir_subdirectories: ' + str(dir_subdirectories))
        docs_in_cur_dir = dir_walk[2]
        # print('docs_in_cur_dir: ' + str(docs_in_cur_dir))
        for doc_name in docs_in_cur_dir:
            # print('doc_name: ' + doc_name)
            # print('cur_path[-1:][0]: ' + cur_path[-1:][0])
            analyze(cur_path[-1:][0], doc_name)
    sys.exit(STATUS_SUCCESS)


if __name__ == '__main__':
    run()
