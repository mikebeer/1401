# How IBM1401 and AUTOCODR were checked

## The test suite (`make test`)

`tests/run_tests.sh` assembles each program listed in `tests/tests.txt`
(`tests/src/t01`–`t13`, `samples/hello`, `samples/primes`) and compares:

| Check | Expected result made by |
|---|---|
| the object deck | Van Snyder's Autocoder 4.8, `-b I` |
| the printout, punched cards, tape records, 1311 pack | SimH i1401 (V4.0, as in ROPE) |
| what SimH and Van Snyder's Autocoder do not have | IBM1401 / AUTOCODR, checked by hand (below) |
| console (1407) output | SimH, for the printout; IBM1401 for the console file |
| ASA printout, return codes | IBM1401 (checked by hand) |

It prints one line: `TESTS 45  OK 45  DIFFERENT 0`.

| Test | Covers |
|---|---|
| t01arith | add/subtract with signs, recomplement, overflow, ZA/ZS, multiply, divide, compare (high/low/equal/unequal), MCE with `$ * , . CR`, MCS, MZ/MN |
| t02da | DA with subfields, record marks, group mark, C option; index registers; address and area-defining literals; MCM; SAR/SBR; MA |
| t03ex | EX segment and reloader, LTORG, CHAIN, forward EQU, DSA *, ORG *&X00, SFX, actual op code in column 19 |
| t04tape | WT, WTW, WTM, RWD, RT, RTW, BSP, BEF (end of file) |
| t05cards | data cards, last-card branch, punch, select stacker, carriage control, write word marks |
| t06misc | BWZ, BW, BBE, BCE, BM, CW, NOP, chained MCW/MN, CS and branch |
| t07cons | 1407 write and read |
| t08pack | 1311: seek, write, write check (good and bad), read, sector count, wrong length, load mode, track with addresses, unequal address, drive not mounted; the pack file |
| t09ramac | 1405: seek, record write/check/read, the B-register after, a record on another track, full track from the middle of a track, load mode, mode mismatch (parity), wrong length, no such arm; the disk file |
| t10colbn | column binary: RCB with a binary column, the 400/500 areas, MBD, MBC, PCB, a normal card behind |
| t11lc | location counters 0–2, absolute |
| t12reloc | relocatable location counters: R, R,X00+6, X,label+20 |
| t13macro | library macros (operands, label, literal with comma, internal labels, conditional statements, continued operands, nested macros), CALL, INCLD |

**Where the expected results come from for t08–t13:**

| Test | Deck | Output |
|---|---|---|
| t08pack | AUTOCODR (`%F`; Van Snyder's writes `%D`) | SimH: the pack file is byte-identical, and the printout is identical except two lines. SimH reads a never-formatted sector address (000000 on the pack) as the sector's number, but converts the digits with the wrong table (`bcd_to_bin` for `bin_to_bcd`), so its zeros come out as blanks: `  1  1` for `001001`.  IBM1401 gives `001001`. |
| t09ramac | AUTOCODR (one-operand disk ops; Van Snyder's needs two) | SimH has no 1405: checked by hand against manual pp. 63–68 (registers B+210, B+1010, B+186; indicators; the order of a track read from record 122) |
| t10colbn | AUTOCODR (MBD with A; Van Snyder's writes D, otherwise identical) | SimH punches the same card.  It differs where it departs from the manual: B is 81 after RCB (manual: 481), and a lone 0 punch reads as the A bit, not `0`.  SimH has no MBC/MBD (it moves as MCW). |
| t11lc | identical to Van Snyder's | SimH |
| t12reloc | AUTOCODR: Van Snyder's makes relocatable counters only for its linker (with `-b` it does not finish) | SimH |
| t13macro | identical to Van Snyder's with the operand continuation removed.  Van Snyder's reads a macro call's continuation line from the macro file, not the source (it opens the macro first), so that one statement fails there. | SimH |

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
| seed 7 (build 2) | 150 | 150 | 149 |

The different printouts (seed 1 and seed 7, each fz0106) are this SimH defect.  A quotient
digit counted past 99 because the program's quotient field overlapped its
divisor.  SimH then indexes past the end of its 100-entry `sum_table`
(undefined behaviour) and stored a blank.  IBM1401 stores the count modulo
10, as the 1401's decimal adder does.

## Not yet tested

The CMS EXECs and the CMS build had a separate code review, and every
EXEC parses with Regina.  None of it has run on VM/370plus yet.
