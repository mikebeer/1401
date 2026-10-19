# IBM 1401 for VM/370plus

An IBM 1401 Data Processing System as a guest of VM/370plus: an emulator
(**IBM1401**), an Autocoder assembler (**AUTOCODR**) and a virtual machine,
user **IBM1401**, that runs the card decks spooled to its reader, as a 1401
in a card shop did.  IBM shipped 1401 compatibility for the System/360
Models 30 and 40 and an emulator program under DOS/360.  This is the same
idea for VM/370plus.

Both programs are portable C89.  They build on the PC (Linux, any C89
compiler) and on CMS (VM/370plus's native GCC 13 `CC` EXEC, or the M5f
cross toolchain).  The reference for every instruction is the *IBM 1401
Reference Manual* A24-1403-5 (April 1962).

## What is there

| Part | What it does |
|---|---|
| `src/cpu.c` | The processing unit: the full instruction set with chaining, word marks, the address registers after each operation, and editing. The special features are there too: multiply/divide, indexing and store address register, high-low-equal compare, bit test, expanded print edit, move record and modify address. Storage is 1,400 to 16,000 positions. |
| `src/io.c` | The 1402 card read-punch: the reader takes several files as one hopper, with last card and the LOAD key. The 1403 printer: carriage tape and skip/space, plain text or ASA carriage control. The 729 tapes 1–6: SimH `.tap` or text files with one record per line, move and load mode, tape mark, backspace and rewind. The 1407 console. |
| `src/autocoder.c` | Autocoder (the language of IBM's Tape Autocoder, C24-3319). The deck it writes is IBM's self-loading format: the clear-storage and bootstrap cards, then load cards (39 columns of data, LCA/SW/CW, `1040`), EX reloader cards and the end card. |
| `cms/` | CMS EXECs: RUN1401, SEND1401, SRV1401 (the guest), G1401PRO (its PROFILE), DIR1401, INST1401, MK1401 |
| `samples/` | HELLO and PRIMES (the 168 primes below 1000) |
| `tests/` | Feature programs t01–t07, with expected decks from Van Snyder's Autocoder and expected output from SimH i1401; a random-program fuzzer |

## On the PC

    make            # ibm1401 and autocoder
    make test       # the test suite: one summary line

    ./autocoder samples/primes.s          # -> primes.cd (deck), primes.lst
    ./ibm1401 primes.cd                   # printout on the terminal
    ./ibm1401 deck.cd -data cards.dat -print out.txt -punch out.cd -tape1 t1.tap

On the PC each option is written `-option`; on CMS options follow `(`.

## On CMS

    AUTOCODR PRIMES                  assemble PRIMES AUTOCODE -> PRIMES DECK A, PRIMES LISTING A
    RUN1401 PRIMES                   assemble if needed, run, printout in PRIMES LIST1403 A
    RUN1401 PRIMES ( TYPE            ... and type it
    RUN1401 PAYROLL ( DATA WEEK42 CARDS A  TAPE1 MASTER TAPE A  PRINT
    SEND1401 PRIMES                  run it on the 1401 guest; printout back in your reader

See [docs/MANUAL.md](docs/MANUAL.md) for the commands, the guest, the
Autocoder language and installation, and [docs/TESTING.md](docs/TESTING.md)
for how the programs were checked.

## How it was checked

- **Assembler:** for every test program and ROPE's seven examples, the deck
  is card-for-card identical to Van Snyder's Autocoder (`-b I`).
- **Emulator:** the printout, punched cards and tape records are identical
  to SimH i1401's. This covers the feature tests, ROPE's examples (Lincoln,
  pi to 2,000 digits, Mersenne, powers of 2) and 800 randomly generated
  programs.  The test suite also passes big-endian (s390x).
- **One known difference:** SimH stores a blank when a single quotient
  digit counts past 99. That is undefined behaviour in SimH (it indexes past
  a table). IBM1401 keeps the digit modulo 10, as the 1401's decimal adder
  does.

## Credits

Part of VM/370plus (Mike Beer).  Behaviour checked against Bob
Supnik's SimH i1401 and Van Snyder's Autocoder (in Ron Mak's and Luca
Severini's ROPE).  No code of theirs is included.  The loader card images
in the decks are IBM's, from the 1401 Autocoder (1401-AU-037).
