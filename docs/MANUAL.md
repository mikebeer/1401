# IBM 1401 for VM/370plus — manual

Build 2.  Version 1.0 of IBM1401 and AUTOCODR.

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
| HELLO, PRIMES, T01ARITH ... T13MACRO AUTOCODE | samples; T05CARDS DATA, T07CONS CONSOLE, T10COLBN DATA |
| PRCON, ADD2, TWICE, PRS, TBL MACRO | the sample library macros that T13MACRO uses |

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
| `PACKn fn ft [fm]` | mount a 1311 disk pack on drive n, 0–4 (fm A if omitted) |
| `DISK fn ft [fm]` | the 1405 disk storage is this file (fm A if omitted) |
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
      PACKn name          the guest's 1311 pack "name" on drive n
      DISK name           the guest's 1405 disk storage "name"
      SWITCH x  HALT GO  LIMIT n
      TO userid           another 1401 guest

SEND1401 assembles fn AUTOCODE if needed.  It punches the deck and its
data as one file, with NOHEADER, to the guest's reader.  Options become a
first card that the guest reads and does not give to the 1401:

    *1401 TAPE1 PAYROLL SWITCH BC HALT GO

A tape `name` is the file `name TAPE1401 A` on the guest's 191.  It is
created when first written and kept, so one job can write a tape that the
next reads.  Disks are kept the same way: a 1311 pack `name` is
`name PACK1311 A`, a 1405 disk storage `name DISK1405 A`.  A full 1311
pack takes 2.1 MB; a 1405 file holds only the records ever written (204
bytes each), so mind the size of the guest's 191.

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
| `MACLIB dir` | (PC) a directory of library macros; may be given up to 8 times. The source's own directory is searched first. On CMS a macro is the file `NAME MACRO` on any accessed disk. |

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
| `PACKn file` | a 1311 disk pack on drive n, 0–4, in SimH's `.dsk` format; created if new, written back when the run ends |
| `DISK file` | the 1405 disk storage, in this file; created if new, written back when the run ends. A 1401 has a 1311 or a 1405, so `DISK` and `PACKn` exclude each other. |
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

**Column binary.**  `RCB` (`1` with d-character C) reads a card of 160
characters, two per column: the first holds rows 12, 11, 0, 1, 2, 3 and the
second rows 4–9, as the bits B A 8 4 2 1 of a 1401 character (SimH's
format; in encoding S the 80 first halves come first, then the 80 second
halves).  They go to 401–480 and 501–580, and 001–080 gets the card's
characters, or the rows ORed together where a column's punches are no
character.  The B-register is 481 afterwards.  On CMS a card file with
column-binary cards needs records of 160 (RECFM V, or F 160).  `PCB` (`4` with C) punches
401–480 and 501–580 in the same format.  `MBD` (`M` with d-character A, or
D as Van Snyder's Autocoder writes it) puts 401 501 402 502 ... into a tape
area ending at the B-address; `MBC` (`M` with B) does the reverse.  Both
stop after the 400-area position with a word mark (manual pp. 90–93).

**1311 disk drive** (`PACKn`).  It behaves as SimH's: 100 cylinders of 10
tracks of 20 sectors; a sector is a 6-character address and 100 characters
(move mode) or 90 characters with word marks (load mode, `L`).  The
B-address is the disk control field: the drive (0 2 4 6 8 for drives 0–4),
a 6-digit sector address and a 3-digit sector count, with the data right
behind it and a group mark with a word mark at the end.  `%F0` seeks (a
`#`-flagged field is a direct seek), `%F1` reads or writes sectors, `%F3`
write-checks the last operation, `%F5` reads or writes with the sector
count overlaid, `%F6` reads or writes whole tracks with their addresses.
An address of 000000 on the pack stands for the sector's own number.

**1405 disk storage** (`DISK`), as in the manual (pp. 63–68): a Model 2,
100,000 records of 200 characters, five records to a track.  The B-address
is an 8-character record address (access arm 0 or 1, a 6-digit record
number, a constant 0), then a group mark with a word mark, then the data,
then another group mark with a word mark.  `%F0` seeks, `%F1` reads or
writes one record (200 characters; 176 with word marks under `L`), `%F2` a
whole track starting at the addressed record (1000; 880), `%F3`
write-checks the last operation.  A read or write needs the arm on the
record's track; otherwise the unequal-address indicator turns on and
nothing moves.  A record read in the other mode than it was written in
turns on the parity indicator.  Afterwards A is B+1, and B is B+210, B+1010,
B+186 or B+890.

**Disk indicators** (both units; `BIN` tests them): V parity or write check
error, W wrong-length record, X unequal address, Y any disk error, N access
inoperable (a drive not mounted, an arm not there).  Errors set the
indicators and the program goes on; a 1405 record address that is not a
valid number turns on N.  A 1311 disk control field with an invalid drive,
sector address or count stops the run with "invalid disk operation"
(return code 12), as SimH does.

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
| `LC n[,R[,X00+k]]`, `LC n,X,LABEL+k` | location counter n, 0–9 (see below) |
| `CALL NAME,A,B` | `B NAME`, `DCW A`, `DCW B`; the file named by NAME's first 3 letters is included before the next LTORG, EX or END |
| `INCLD NAME` | only the inclusion |
| `CTL`, `PRINT`, `PUNCH`, `ALTER` | accepted and ignored |

**Location counters** (as in Van Snyder's Autocoder).  `LC n` switches to
counter n; each counter goes on where it left off.  Counter 0 starts at
333, the others at 0, and `ORG` sets the current one.  `LC n,R` makes a
relocatable counter, which may not hold an absolute ORG.  AUTOCODR has no
linker, so it places relocatable counters itself, in counter order:
`LC n,R` right after the highest absolute address, `LC n,R,X00+k` at the
next multiple of 100 plus k, `LC n,X,LABEL+k` at LABEL+k.  Labels in them
get their final addresses, and the deck is absolute.

**Macros** (Van Snyder's format).  A statement whose operation is no
Autocoder operation calls the library macro of that name (3 to 5
letters).  The macro file's first line may be a `HEADR` line; the other
lines are model statements, in which:

| Code | |
|---|---|
| `)00` in the label field | the macro statement's label |
| `)01` ... `)99` | the operands 1–99 (in the label or operation field the operand fills the field) |
| `)0A` ... `)0I` (12 zone) | operands 1–9, but the statement is dropped if the operand is missing |
| `)0J` ... `)0R` (11 zone) | an internal label, `)0J` followed by the macro statement's 3-digit serial number |
| at the end of the remarks: `)0/` ... `)0Z` (0 zone) | drop the statement if operand 1–9 is there |
| at the end of the remarks: `)0A` ... `)0I` | drop the statement if operand 1–9 is missing |

Operands are separated by commas; `@...@` may hold commas and blanks.  A
comma followed by blanks continues the operands in column 21 of the next
line (up to 5 lines).  Macros may call macros.  Generated lines are marked
`+` in the listing.  The sample macros PRCON, ADD2 and TWICE and the
T13MACRO program show each code.

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
- Disks: `SD RD RDW RDT RDTW WD WDW WDT WDTW WDC WDCW`.  With one operand
  (`RD AREA`) the function digit is the 1405's: seek 0, record 1, track 2,
  check 3, giving `M %F1 AREA R`.  With two (`RD 6,DCF`) the first is the
  digit, for every 1311 function.  Van Snyder's Autocoder writes `%D` here;
  the manual and AUTOCODR write `%F`.
- `MBD` assembles with d-character A, as in the manual (Van Snyder's
  Autocoder writes D; IBM1401 takes both).

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

- Stacker selection is accepted, but all cards go to one output.
- No IOCS macro library: the macro processor is there, but no IBM macros
  come with it.  MA is always the machine's Modify Address.
- No linker and no relocatable output: relocatable location counters are
  placed by AUTOCODR in the one assembly.
- The 1311's write-address key is always on (track writes with addresses
  are allowed); the 1405 is always a Model 2 with both access arms.
- Disk and seek timing are not simulated, so the disk-busy indicator never
  turns on.  The emulator runs as fast as it can.
