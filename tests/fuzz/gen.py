#!/usr/bin/env python3
"""gen.py SEED N OUTDIR - random 1401 Autocoder programs for differential
testing: random arithmetic, move, edit, compare/branch and word mark
operations on random fields, then every field is printed.  The same
program must give the same printout on every correct 1401."""
import random, sys, os

def card(label, op, operand):
    return ('     ' + label.ljust(10) + op.ljust(5) + operand)[:72]

def gen(seed):
    r = random.Random(seed)
    nf = 16
    fields = []
    for i in range(nf):
        n = r.randint(1, 10)
        kind = r.random()
        if kind < 0.7:
            digits = ''.join(r.choice('0123456789') for _ in range(n))
            sign = r.choice(['', '', '-', '&'])
            val = (sign + digits) if sign else digits
            fields.append(('N', n, val))
        else:
            txt = ''.join(r.choice('ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 .,*$-/') for _ in range(n))
            fields.append(('C', n, '@' + txt + '@'))
    L = []
    L.append(card('', 'JOB', 'FUZZ %d' % seed))
    L.append(card('', 'ORG', '87'))
    L.append(card('X1', 'DSA', '0'))
    L.append(card('', 'ORG', '92'))
    L.append(card('X2', 'DSA', '0'))
    L.append(card('', 'ORG', '400'))
    L.append(card('START', 'CS', '332'))
    L.append(card('', 'CS', ''))
    lab = [0]
    def newlab():
        lab[0] += 1
        return 'L%d' % lab[0]
    ops2 = ['A', 'S', 'ZA', 'ZS', 'MCW', 'MN', 'MZ', 'MCS', 'C', 'LCA']
    for k in range(r.randint(10, 40)):
        x = r.random()
        a = 'F%d' % r.randrange(nf)
        b = 'F%d' % r.randrange(nf)
        if x < 0.55:
            op = r.choice(ops2)
            if op == 'LCA':
                # keep the field structure: copy into a work field
                L.append(card('', 'LCA', '%s,WORK' % a))
            else:
                L.append(card('', op, '%s,%s' % (a, b)))
        elif x < 0.65:
            # multiply: product field PROD, multiplier at its high order
            ma = r.randrange(nf)
            if fields[ma][0] != 'N':
                continue
            L.append(card('', 'ZA', 'F%d,MPLR' % ma))
            L.append(card('', 'ZA', 'ZERO20,PROD'))
            L.append(card('', 'MCW', 'MPLR,PROD-11'))
            mb = r.randrange(nf)
            if fields[mb][0] == 'N':
                L.append(card('', 'M', 'F%d,PROD' % mb))
                L.append(card('', 'MCW', 'PROD,RES%d' % r.randrange(4)))
        elif x < 0.72:
            da = r.randrange(nf)
            if fields[da][0] != 'N':
                continue
            L.append(card('', 'ZA', 'F%d,DVD' % da))
            L.append(card('', 'ZA', 'F%d,DVR' % r.randrange(nf)))
            L.append(card('', 'D', 'DVR,DVD-9'))
            L.append(card('', 'MCW', 'DVD,RES%d' % r.randrange(4)))
        elif x < 0.82:
            l1 = newlab(); l2 = newlab()
            br = r.choice(['BH', 'BL', 'BE', 'BU'])
            L.append(card('', 'C', '%s,%s' % (a, b)))
            L.append(card('', br, l1))
            L.append(card('', 'MCW', '@N@,TRACE'))
            L.append(card('', 'B', l2))
            L.append(card(l1, 'MCW', '@Y@,TRACE'))
            L.append(card(l2, 'A', '@1@,STEPS'))
        elif x < 0.88:
            e = r.randrange(4)
            L.append(card('', 'LCA', 'ED%d,EDW' % e))
            L.append(card('', 'MCE', '%s,EDW' % a))
            L.append(card('', 'MCW', 'EDW,RES%d' % r.randrange(4)))
        elif x < 0.92:
            l1 = newlab()
            L.append(card('', 'BWZ', '%s,%s,%s' % (l1, a, r.choice(['1', '2', 'K', 'B', 'S']))))
            L.append(card('', 'A', '@1@,STEPS'))
            L.append(card(l1, 'NOP', ''))
        elif x < 0.96:
            l1 = newlab()
            L.append(card('', 'BBE', '%s,%s,%s' % (l1, a, r.choice(['1', '2', '4', '8', 'A', 'B', '9']))))
            L.append(card('', 'A', '@1@,STEPS'))
            L.append(card(l1, 'NOP', ''))
        else:
            L.append(card('', 'SBR', 'X1,%d' % r.randrange(0, 5)))
            L.append(card('', 'MCW', '%s&X1,WORK' % a))
        if r.random() < 0.1:
            L.append(card('', 'BAV', newlab()))
            L.append(card(lab_name(lab[0]), 'NOP', ''))
    # print every field, the results and the trace
    names = ['F%d' % i for i in range(nf)] + ['RES0', 'RES1', 'RES2', 'RES3',
             'TRACE', 'STEPS', 'WORK', 'PROD', 'DVD']
    col = 201
    for nm in names:
        L.append(card('', 'MCW', '%s,%d' % (nm, min(col + 11, 332))))
        col += 13
        if col > 320:
            L.append(card('', 'W', ''))
            L.append(card('', 'CS', '332'))
            L.append(card('', 'CS', ''))
            col = 201
    L.append(card('', 'W', ''))
    L.append(card('', 'H', 'START'))
    for i, (k, n, v) in enumerate(fields):
        L.append(card('F%d' % i, 'DCW', v))
    for i in range(4):
        L.append(card('RES%d' % i, 'DCW', '@' + ' ' * 12 + '@'))
    L.append(card('TRACE', 'DCW', '@-@'))
    L.append(card('STEPS', 'DCW', '000'))
    L.append(card('WORK', 'DCW', '@' + ' ' * 12 + '@'))
    L.append(card('MPLR', 'DCW', '0000000000'))
    L.append(card('PROD', 'DCW', '000000000000000000000'))
    L.append(card('ZERO20', 'DCW', '000000000000000000000'))
    L.append(card('DVR', 'DCW', '0000'))
    L.append(card('DVD', 'DCW', '0000000000000000'))
    L.append(card('EDW', 'DCW', '@' + ' ' * 16 + '@'))
    eds = ['@ ,   ,  0.  @', '@$ ,   ,  0.  &CR@', '@*  ,  0.  -@', '@   0@']
    for i, e in enumerate(eds):
        L.append(card('ED%d' % i, 'DCW', e))
    L.append(card('', 'END', 'START'))
    return '\n'.join(L) + '\n'

def lab_name(n):
    return 'L%d' % n

if __name__ == '__main__':
    seed, n, out = int(sys.argv[1]), int(sys.argv[2]), sys.argv[3]
    os.makedirs(out, exist_ok=True)
    for i in range(n):
        with open(os.path.join(out, 'fz%04d.s' % (seed + i)), 'w') as f:
            f.write(gen(seed + i))
