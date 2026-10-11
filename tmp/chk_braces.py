# -*- coding: utf-8 -*-
"""粗查 C 文件括号平衡（跳过注释与字符串）。"""
import sys


def count(path):
    s = open(path, encoding='utf-8', errors='replace').read()
    i = 0
    n = len(s)
    depth_p = depth_b = 0
    line = 1
    while i < n:
        c = s[i]
        if c == '\n':
            line += 1
            i += 1
            continue
        if c == '/' and i + 1 < n and s[i + 1] == '*':
            j = s.find('*/', i + 2)
            i = n if j < 0 else j + 2
            continue
        if c == '/' and i + 1 < n and s[i + 1] == '/':
            j = s.find('\n', i)
            i = n if j < 0 else j
            continue
        if c == '"' or c == "'":
            q = c
            i += 1
            while i < n and s[i] != q:
                if s[i] == '\\':
                    i += 1
                i += 1
            i += 1
            continue
        if c == '(':
            depth_p += 1
        elif c == ')':
            depth_p -= 1
            if depth_p < 0:
                print(f'  extra ) at line {line}')
                depth_p = 0
        elif c == '{':
            depth_b += 1
        elif c == '}':
            depth_b -= 1
            if depth_b < 0:
                print(f'  extra }} at line {line}')
                depth_b = 0
        i += 1
    print(f'{path}: paren_balance={depth_p} brace_balance={depth_b}')


for p in sys.argv[1:]:
    count(p)
