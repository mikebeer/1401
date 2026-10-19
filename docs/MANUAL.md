# IBM 1401 for VM/370plus — manual

Build 1.  Version 1.0 of IBM1401 and AUTOCODR.

1. [Installing on VM/370plus](#1-installing-on-vm370plus)
2. [RUN1401 — run a program in your own virtual machine](#2-run1401)
3. [SEND1401 and the 1401 guest](#3-the-1401-guest)
4. [AUTOCODR — the assembler](#4-autocodr)
5. [IBM1401 — the emulator](#5-ibm1401)
6. [The Autocoder language](#6-the-autocoder-language)
7. [Character codes](#7-character-codes)
8. [What is not there](#8-what-is-not-there)

## 1. Installing on VM/370plus

The kit (`IBM1401-kit-bN.zip`) has a `CMS/` folder with every file under
its CMS name (`RUN1401.EXEC` is RUN1401 EXEC):

| File | Purpose |
|---|---|
| IBM1401 C, AUTOCODR C | the two programs, one C89 source each |
| IBM1401 TEXT, AUTOCODR TEXT | the same, cross-built (only in kits built with the toolchain) |
| RUN1401, SEND1401 EXEC | for users |
| SRV1401, G1401PRO EXEC | the 1401 guest and its PROFILE |
| DIR1401, INST1401, MK1401 EXEC | installation (MAINT) |
| HELLO, PRIMES, T01ARITH ... T07CONS AUTOCODE | samples; T05CARDS DATA, T07CONS CONSOLE |

**1. Send the files to MAINT.** Use vmspool from the PC
(`python3 vmspool.py CMS/RUN1401.EXEC MAINT RUN1401 EXEC`, then READCARD or
RDRLIST on CMS), or IND$FILE PUT from the 3279 terminal, ASCII CRLF.  Every
line is 80 columns or less, so no file goes as a stream.

**2. Build the modules** (only if the kit has no TEXT decks), in a machine
with 64M (`DEFINE STORAGE 64M`, `IPL CMS`) and the C toolchain accessed:

    MK1401                      CC IBM1401, CC AUTOCODR -> two MODULEs

**3. Install** as MAINT:

    INST1401                    modules and EXECs to 19E; the IBM1401 user
    INST1401 ( AUTOLOG          ... and AUTOLOG1 starts the guest at IPL
    INST1401 ( NOGUEST          only the 19E part

INST1401 LOADs and GENMODs TEXT decks if it finds no MODULE.  It copies to
19E with file mode 2.  DIR1401 then adds the user IBM1401: OPTION ECMODE
(the modules run AMODE 31), 32M, IPL CMS, and its own 191 on VM50U1
cylinders 101–110 (`DIR1401 cyl vol` puts it elsewhere).  INST1401 formats
the 191 if it is new, writes the PROFILE EXEC and starts the guest with
AUTOLOG.

On the PC, `tools/mkcms.sh` cross-builds the TEXT decks with the
VM/370plus M5f toolchain (cmscc and elf_to_cms, as for IND$FILE), and
`tools/mkkit.sh N` makes kit build N.

## 2. RUN1401

    RUN1401 fn [ft [fm]] [( options]

If fn AUTOCODE exists (or ft is not DECK), it is assembled first.  Then
the deck fn DECK is loaded with the LOAD key and run.  Files:

| File | Contents |
|---|---|
| fn DECK A | the object deck |
| fn LISTING A | the assembly listing |
| fn LIST1403 A | the 1403 printout, ASA carriage control (`PRINT ... (CC`) |
| fn PUN1403 A | punched cards, only if the program punched any |

| Option | |
|---|---|
| `DATA fn ft [fm]` | cards behind the deck (may be repeated) |
| `PRINT` | also print the printout on your virtual printer |
| `TYPE` | also type it on the terminal |
| `TAPEn fn ft [fm]` | mount a tape on 729 unit n, 1–6 (fm A if omitted, so a new tape can be written) |
| `SWITCH BCDEFG` | sense switches on |
| `HALT GO` / `HALT ASK` | a halt continues / asks at the terminal (default: the run ends) |
| `LIMIT n` | stop after n instructions |
| `SIZE n` | core storage, 1400 to 16000 (default 16000) |
| `ENCODING A\|H\|S` | character encoding, see 7 |
| `LOUD` | show the IBM1401 command |

The return code is IBM1401's: 0 halt, 4 the program read past the last
card, 8 instruction limit, 12 machine check (invalid op code, address or
length; the message says which and where), 20 file error.

## 3. The 1401 guest

The virtual machine IBM1401 is a 1401 in a card shop.  A deck spooled to
its reader is read in with the LOAD key and run.  The printout goes back to
the sender's reader (RDRLIST shows it), punched cards go to the sender's
reader too, and the sender gets one message:

    IBM1401: job 0042 ended with a halt, printed 18 lines

**Submitting a job:**

    SEND1401 fn [ft [fm]] [( options]
      DATA fn ft [fm]     data cards behind the deck (fm A if omitted)
      TAPEn name          the guest's tape "name" on unit n
      SWITCH x  HALT GO  LIMIT n
      TO userid           another 1401 guest

SEND1401 assembles fn AUTOCODE if needed.  It punches the deck and its
data as one file, with NOHEADER, to the guest's reader.  Options become a
first card that the guest reads and does not give to the 1401:

    *1401 TAPE1 PAYROLL SWITCH BC HALT GO

A tape `name` is the file `name TAPE1401 A` on the guest's 191.  It is
created when first written and kept, so one job can write a tape that the
next reads.

**The service machine** runs SRV1401 from its PROFILE EXEC when it was
started disconnected (AUTOLOG).  Logged on at a terminal, it only tells
you how to start it:

    SRV1401 [( LOUD ONCE INTERVAL n]

It waits for reader files with WAKEUP (RDR.  SRV1401 CONFIG A
(optional) sets:

    PRINT SENDER          printout to the sender (default)
    PRINT CLASS P         printout to the system printer of class P (the PC's VMPRINT)
    LIMIT 50000000        default instruction limit, so no job loops for ever
    INTERVAL 60

`SRV1401 STOP A` on the guest's 191 ends the loop after the current job.
Console (1407) output of a job is printed as a second file.  Console input
is not available to guest jobs.

## 4. AUTOCODR

    AUTOCODR fn [ft [fm]] [( options]       ft default AUTOCODE

| Option | |
|---|---|
| `DECK fn ft fm` / `NODECK` | the object deck (default fn DECK A) |
| `LIST fn ft fm` / `LIST *` / `NOLIST` | the listing (default fn LISTING A; `*` = the terminal) |
| `ENCODING A\|H\|S` | character encoding of source and deck |
| `BOOT I\|N` | IBM loader cards in front of the deck (default), or none |
| `SIZE n` | core size the clear-storage cards clear (default 16000) |

Return codes: 0 clean, 8 errors (each is listed under its line and shown
on the terminal), 12 a file cannot be opened.  The listing shows, for every
line, its count, location and instruction, and the deck card it went to.
The symbol table follows.

**The deck.**  It runs on any 1401, real or simulated (SimH, ROPE), as
IBM's Autocoder decks did:

| Card | Contents |
|---|---|
| 1–2 | Clear storage, up to SIZE. |
| 3 | The bootstrap: word marks in 1, 40, 47, 54, 61, 68, 72. |
| Load cards | Columns 1–39: up to 39 characters of program. 40–46: `Lnnnaaa`, a load of nnn characters ending at aaa (the word mark comes with them). 47–67: up to six more word marks, `,aaabbb`, or `)aaabbb` clearing one. 68–71: `1040`, read the next card and branch to 040. 72–75: the sequence number. 76–80: the JOB card's columns 76–80. |
| EX | `B` to the start address in 68–71, followed by two reloader cards. The segment returns to the loader with `R 40` (read a card, branch to 040). |
| End card | `/sss080`: clear 000–080, branch to the start address. |

## 5. IBM1401

    IBM1401 fn [ft [fm]] [( options]          ft default DECK
    ibm1401 deck [deck ...] [-option ...]     (PC)

The first card is read with the LOAD key: 001–080 is cleared, the card is
read, a word mark is set in 001, and the program starts at 001.

| Option | |
|---|---|
| `DATA file` | more cards behind the deck, read as one hopper |
| `PRINT file` / `PRINT *` | 1403 output (default: the terminal) |
| `ASA` | ASA carriage control in column 1 (` ` `0` `-` `+` `1`) instead of newlines and form feeds |
| `PUNCH file` | 1402 punch (default: the cards are dropped) |
| `TAPEn file`, `RTAPEn file` | a tape on unit n, read-write / read-only. `name.tap` is a SimH tape; anything else is text with one record per line (a tape mark is a line holding only the tape-mark character). Tapes are written back when the run ends. |
| `CONSOLE file`, `CONOUT file` | 1407 input and output (default: the terminal) |
| `SWITCH letters` | sense switches B–G |
| `HALT STOP\|GO\|ASK` | what the halt instruction does |
| `LIMIT n` | stop after n instructions |
| `SIZE n` | 1400, 2000, 4000, 8000, 12000 or 16000 |
| `ENCODING A\|H\|S` | |
| `TRACE [file]` | one line per instruction: address, instruction, A and B registers |
| `DUMP from to` | a storage print at the end, word marks as `1` below |
| `QUIET` | no end-of-run message |

The end-of-run message reads:

    IBM1401: HALT AT 551, 65618 INSTRUCTIONS, 14 CARDS READ, 18 LINES PRINTED, 0 CARDS PUNCHED

## 6. The Autocoder language

Columns: 6–11 label, 16–20 operation, 21–72 operands.  The operands end
at the first blank outside a literal; a remark may follow.  `*` in column
6 marks a comment.  A blank operation repeats the previous declarative, or
gives a DA subfield.

**Declaratives.**  A label on DCW, DC, DS or DSA names the field's
low-order (units) position.  A label on an instruction names its op code.

| | |
|---|---|
| `DCW const` | constant with a word mark. The constant can be `00123`, `-5` (sign over the units digit), `@ANY TEXT@`, `#20` (20 blanks), `SYMBOL` / `*` (an address), `+@TEXT@` (the address of a literal) or `%U1` (a device). |
| `DC const` | the same without a word mark |
| `DS n` | reserve n positions |
| `DSA addr` | a 3-character address with a word mark |
| `EQU value` | `X EQU Y+5`, `X EQU 100`, `X EQU *` (the last position assigned), `X EQU 89&X1` (indexed), `TAPE EQU %U1` |
| `DA nXm[,G][,C][,|][,X1]` | n records of m positions: a group mark after them, cleared to blanks, a record mark after each record, indexed by X1. Lines below with a blank operation define fields: `NAME 1,8` (positions 1–8: a word mark at 1 in every record, NAME addresses 8), or `FLAG 9` (one position, no word mark). |

**Processor control.**

| | |
|---|---|
| `ORG n` / `ORG SYMBOL+n` / `ORG *&X00` | the location counter; X00 rounds up to a hundred |
| `LTORG addr` | place the literals collected so far there |
| `EX addr` | literals, then load and execute this segment; loading resumes when it executes `R 40` |
| `XFR addr` | the same without the literals |
| `END addr` | literals, the end card; addr is the start address |
| `JOB text` | the listing heading; columns 76–80 go to every card of the deck |
| `CHAIN n` | n more copies of the previous op code (chained instructions) |
| `SFX c` | suffix c on 5-character names |
| `CTL`, `PRINT`, `PUNCH`, `ALTER`, `INCLD`, `CALL` | accepted and ignored |

**Operands.**

| | |
|---|---|
| Symbols, actual addresses (`280`), `*` | `*` is the instruction's last position |
| Address adjustment | `NAME+5`, `NAME-3`, `*-7` |
| Indexing | `NAME&X1`, `0&X2`, `TABLE+3&X3`. A DA with `X1` indexes its fields by itself. |
| Literals | `@TEXT@` (character); `+5`, `-12` (numeric, signed); `+NAME`, `-NAME+3` (address constant); `NAME#5` (an area of 5 blanks named NAME) |
| Devices | `%U1` (tape 1), `%T0` (1407), `%F1` (disk) |
| The d-character | last operand of one character, e.g. `BCE LOOP,FIELD,A`. A trailing comma is a blank d (`BCE X,Y,`). |

**Mnemonics.**  All of the 1401's and its features':

- Arithmetic: `A S M D ZA ZS`
- Moves: `MCW MLC LCA MLCWA MCS MCE MN MLNS MZ MLZS MCM MRCM MIZ MBC MBD`
- Branches: `B BCE BWZ BW BM BBE BIN BSS BE BH BL BU BAV BC9 BCV BLC BEF BER BPB BPCB`
- Other: `C SW CW CS SAR SBR MA NOP H`
- Cards and printer: `R W WR P RP WP WRP RCB PCB RF WRF SRF SPF SS SSB CC CCB WM`
- Tapes: `RT RTW RTB WT WTW WTB WTM BSP SKP RWD RWU CU MU LU`
- Disks: `RD RDW SD WD WDW ...`

An actual op code may be written in column 19 with the d-character in
column 20.

**Rules worth knowing.**

- Fields are processed right to left, up to a word mark.  MCW stops at a
  word mark in A *or* B.  LCA stops at A and moves the word marks.
- `C A,B` sets *high* when the B-field is higher.  The fields must have
  the same length: a B-field longer than the A-field is always high.
- `D DIVSOR,DVDEND-n`: the B-address is the dividend's high-order
  position.  The field needs as many zeros to its left as the divisor has
  digits, plus one.  The quotient is then left of the remainder.
- `M MCAND,PROD`: PROD is (multiplicand + multiplier + 1) long, with the
  multiplier in its high-order positions.
- Results carry their sign in the units zone: +7 is `G`, −7 is `P`.  Use
  `MZ @0@,FIELD` before comparing or printing digits.

## 7. Character codes

The 1401 has 64 characters; the encodings say which character of the PC or
CMS stands for each.  They are those of Van Snyder's Autocoder:

| | `&` | rec. mark | grp. mark | `#` | `@` | `%` | tape mark |
|---|---|---|---|---|---|---|---|
| A (default) | `&` | `\|` | `}` | `#` | `@` | `%` | `{` |
| H | `+` | `\|` | `}` | `=` | `'` | `(` | `{` |
| S (SimH old, ROPE) | `&` | `'` | `"` | `#` | `@` | `%` | `(` |

In source programs `+` and `&` are the same character in every encoding.
On CMS the files are EBCDIC (IBM-1047, as vmspool and the C runtime use).
A deck made on CMS runs on CMS.  A deck made on the PC in encoding A and
sent with vmspool arrives with the same characters.

## 8. What is not there

- The 1405/1311 disk instructions stop with "device not installed".
- Column binary (RCB/PCB) reads and punches as ordinary BCD.
- Stacker selection is accepted, but all cards go to one output.
- No macros (IOCS, `MA` macro library).  Only location counter 0.
- Timing is not simulated: the emulator runs as fast as it can.
