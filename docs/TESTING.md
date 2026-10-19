# How IBM1401 and AUTOCODR were checked

## The test suite (`make test`)

`tests/run_tests.sh` assembles each program listed in `tests/tests.txt`
(`tests/src/t01`–`t07`, `samples/hello`, `samples/primes`) and compares:

| Check | Expected result made by |
|---|---|
| the object deck | Van Snyder's Autocoder 4.8, `-b I` |
| the printout, punched cards, tape records | SimH i1401 (V4.0, as in ROPE) |
| console (1407) output | SimH, for the printout; IBM1401 for the console file |
| ASA printout, return codes | IBM1401 (checked by hand) |

It prints one line: `TESTS 30  OK 30  DIFFERENT 0`.

| Test | Covers |
|---|---|
| t01arith | add/subtract with signs, recomplement, overflow, ZA/ZS, multiply, divide, compare (high/low/equal/unequal), MCE with `$ * , . CR`, MCS, MZ/MN |
| t02da | DA with subfields, record marks, group mark, C option; index registers; address and area-defining literals; MCM; SAR/SBR; MA |
| t03ex | EX segment and reloader, LTORG, CHAIN, forward EQU, DSA *, ORG *&X00, SFX, actual op code in column 19 |
| t04tape | WT, WTW, WTM, RWD, RT, RTW, BSP, BEF (end of file) |
| t05cards | data cards, last-card branch, punch, select stacker, carriage control, write word marks |
| t06misc | BWZ, BW, BBE, BCE, BM, CW, NOP, chained MCW/MN, CS and branch |
| t07cons | 1407 write and read |

Tests t01, t03 and t06 also run in encoding S (SimH old conversions).

The same suite passed with:

- the single-file CMS sources from `tools/amalgamate.sh`;
- a big-endian s390x build, run under qemu.

## ROPE's examples

Lincoln, BiggerPi (pi to 2,000 digits), mersenne, powers2, sqrt1k,
list8080 and errors.  The decks are identical to Van Snyder's, including
the deliberately faulty `errors` and `sqrt1k`.  The printouts are identical
to SimH's, including where they stop.  They are GPL, so they are not in
this repository.

## Random programs (`tests/fuzz`)

`gen.py SEED N DIR` writes N random Autocoder programs.  Each one does
random arithmetic, moves, edits, multiplies, divides, compare-and-branch,
BWZ, BBE and indexed moves on random fields, then prints every field.
`diffrun.sh DIR` assembles each program with both assemblers and runs it on
both emulators (`VSAC=...` and `SIMH=...` name the reference programs).

| Batch | Programs | Identical decks | Identical printouts |
|---|---|---|---|
| seed 1 | 300 | 300 | 299 |
| seed 1001 | 500 | 500 | 500 |

The one different printout (seed 1, fz0106) is a SimH defect.  A quotient
digit counted past 99 because the program's quotient field overlapped its
divisor.  SimH then indexes past the end of its 100-entry `sum_table`
(undefined behaviour) and stored a blank.  IBM1401 stores the count modulo
10, as the 1401's decimal adder does.

## Not yet tested

The CMS EXECs and the CMS build had a separate code review, and every
EXEC parses with Regina.  None of it has run on VM/370plus yet.
