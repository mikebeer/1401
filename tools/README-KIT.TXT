IBM 1401 for VM/370plus - kit build @BUILD@
=========================================

CMS/   the CMS files, named FN.FT (RUN1401.EXEC is RUN1401 EXEC)
PC/    ibm1401 and autocoder for Linux x86-64
MANUAL.md  the manual

Install on VM/370plus (as MAINT)
--------------------------------
1. Send the CMS files to MAINT, e.g. with vmspool from the PC:
       for f in CMS/*; do b=$(basename $f); \
         python3 vmspool.py $f MAINT ${b%.*} ${b#*.}; done
   then on CMS, for each file in the reader:  READCARD fn ft A
   (or RDRLIST).  IND$FILE PUT, ASCII CRLF, works as well.
   All lines are 80 columns or less: no UNSTREAM is needed.

2. Only if the kit has no IBM1401 TEXT / AUTOCODR TEXT: build the
   modules with the native C toolchain, in a 64M machine:
       DEFINE STORAGE 64M   (then IPL CMS)
       MK1401

3. Install:
       INST1401 ( AUTOLOG
   - IBM1401 MODULE, AUTOCODR MODULE, RUN1401/SEND1401/SRV1401/MK1401
     EXEC and the samples go to 19E (mode 2);
   - DIR1401 adds the user IBM1401 (ECMODE, 32M, 191 = VM50U1 cyl
     101-110) and runs DIRECT; its 191 is formatted, gets PROFILE EXEC,
     and the guest is started (AUTOLOG IBM1401 IBM1401);
   - ( AUTOLOG: AUTOLOG1 starts the guest at every IPL.
   Without the guest: INST1401 ( NOGUEST

First steps (any CMS user)
--------------------------
   RUN1401 HELLO ( TYPE          assemble and run, type the printout
   RUN1401 PRIMES ( TYPE         the 168 primes below 1000
   RUN1401 T05CARDS ( DATA T05CARDS DATA *    with data cards
   SEND1401 PRIMES               on the 1401 guest: the printout comes
                                 back to your reader, with a message
   RUN1401 T08PACK ( PACK0 T08 PACK1311 A TYPE     a 1311 disk pack
   RUN1401 T09RAMAC ( DISK T09 DISK1405 A TYPE     the 1405 RAMAC
   RUN1401 T10COLBN ( DATA T10COLBN DATA * TYPE    column binary
   RUN1401 T13MACRO ( TYPE       library macros (PRCON MACRO etc.)

New in build 2: the 1311 and 1405 disks (PACKn, DISK), column binary
(RCB, PCB, MBC, MBD), Autocoder location counters LC 0-9, library
macros (fn MACRO on any disk), CALL and INCLD.  See MANUAL.md.

On the PC
---------
   PC/autocoder CMS/PRIMES.AUTOCODE -deck primes.cd -list primes.lst
   PC/ibm1401 primes.cd
