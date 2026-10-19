#!/usr/bin/env python3
"""wrap80.py FILE... - break C source lines longer than 80 columns at a
comma, an operator or before a trailing comment, outside string and
character literals (CMS files and XEDIT like 80 columns)."""
import sys, re

def literal_mask(line):
    """True for positions inside a string/char literal or a comment."""
    m = [False] * len(line)
    i = 0; q = None
    while i < len(line):
        c = line[i]
        if q:
            m[i] = True
            if c == '\\':
                if i + 1 < len(line): m[i + 1] = True
                i += 2; continue
            if c == q: q = None
        elif line.startswith('/*', i):
            j = line.find('*/', i + 2)
            j = len(line) if j < 0 else j + 2
            for k in range(i, j): m[k] = True
            i = j; continue
        elif c in '"\'':
            q = c; m[i] = True
        i += 1
    return m

def wrap(line, limit=80):
    if len(line) <= limit:
        return [line]
    indent = len(line) - len(line.lstrip())
    # a trailing comment goes on its own line above
    cm = re.search(r'\s+(/\*.*\*/)\s*$', line)
    if cm and cm.start() > indent:
        code = line[:cm.start()].rstrip()
        com = ' ' * indent + cm.group(1)
        if len(com) <= limit:
            return wrap(com, limit) + wrap(code, limit)
    m = literal_mask(line)
    best = -1
    for i in range(min(len(line), limit) - 1, indent + 8, -1):
        if m[i]: continue
        if line[i] == ',' and i + 1 < len(line) and line[i + 1] == ' ':
            best = i + 1; break
        if line[i] == ' ' and line[i+1:i+3] in ('&&', '||') :
            best = i; break
        if line[i] == ' ' and i + 2 < len(line) and line[i+1] in '+-|?:' and line[i+2] == ' ':
            best = i; break
    if best < 0:
        return [line]
    head = line[:best].rstrip()
    tail = ' ' * (indent + 4) + line[best:].lstrip()
    return [head] + wrap(tail, limit)

for fn in sys.argv[1:]:
    src = open(fn).read().split('\n')
    out = []
    for l in src:
        out.extend(wrap(l))
    open(fn, 'w').write('\n'.join(out))
    bad = [i + 1 for i, l in enumerate(out) if len(l) > 80]
    if bad:
        print(fn, 'still long:', bad[:20])
