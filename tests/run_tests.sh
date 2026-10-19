#!/bin/sh
# run_tests.sh - the IBM1401 / AUTOCODR test suite (on the PC)
#
# For every line "name encoding" of tests.txt: assemble src/name.s with
# AUTOCODR, compare the deck with expect/name.enc.cd (made by Van
# Snyder's Autocoder), run it on IBM1401 (with src/name.dat behind the
# deck, if there is one, and the options in src/name.opt, %W standing
# for the work directory) and compare the printout, punch, tape and disk
# with expect/ (made by SimH i1401, or checked by hand where SimH has no
# such feature; see docs/TESTING.md).  Prints one summary of all tests.
cd "$(dirname "$0")" || exit 1
T=$(pwd); B=$(cd .. && pwd)
W=${TMPDIR:-/tmp}/t1401.$$; mkdir -p "$W"
pass=0; fail=0; failed=""
check() {   # check label result
  if [ "$2" = 0 ]; then pass=$((pass+1)); else fail=$((fail+1)); failed="$failed $1"; fi
}
grep -v '^#' tests.txt | while read n e; do echo "$n $e"; done > "$W/list"
while read n e; do
  [ -z "$n" ] && continue
  src=src/$n.s; [ -f $src ] || src=../samples/$n.s
  $B/autocoder $src -enc $e -deck $W/$n.cd -list $W/$n.lst >/dev/null 2>&1
  cmp -s $W/$n.cd expect/$n.$e.cd; check "$n.$e.deck" $?
  in=$W/$n.cd; [ -f src/$n.dat ] && in="$W/$n.cd src/$n.dat"
  rm -f $W/$n.tap
  con=""; [ -f src/$n.con ] && con="-console src/$n.con -conout $W/$n.con"
  opt=""; [ -f src/$n.opt ] && opt=$(sed "s|%W|$W|g" src/$n.opt)
  rm -f $W/$n.dsk $W/$n.rdk
  $B/ibm1401 $in -enc $e -print $W/$n.prt -punch $W/$n.pun -tape1 $W/$n.tap $con $opt -quiet -limit 10000000 </dev/null
  cmp -s $W/$n.prt expect/$n.$e.prt; check "$n.$e.print" $?
  if [ -f expect/$n.$e.pun ]; then cmp -s $W/$n.pun expect/$n.$e.pun; check "$n.$e.punch" $?; fi
  if [ -f expect/$n.$e.con ]; then cmp -s $W/$n.con expect/$n.$e.con; check "$n.$e.console" $?; fi
  for x in dsk rdk; do
    if [ -f expect/$n.$e.$x ]; then cmp -s $W/$n.$x expect/$n.$e.$x; check "$n.$e.$x" $?; fi
  done
  if [ -f expect/$n.$e.tap ]; then python3 -I tapcmp.py $W/$n.tap expect/$n.$e.tap; check "$n.$e.tape" $?; fi
done < "$W/list"
# ASA carriage control output (VM spool / CMS PRINT ( CC)
$B/ibm1401 expect/t05cards.A.cd src/t05cards.dat -asa -print $W/asa -quiet </dev/null
cmp -s $W/asa expect/t05cards.A.asa; check "t05cards.asa" $?
# return codes: halt 0, reader empty 4, limit 8
$B/ibm1401 expect/t01arith.A.cd -print /dev/null -quiet </dev/null; check "rc.halt" $?
$B/ibm1401 expect/t01arith.A.cd -print /dev/null -quiet -limit 100 </dev/null; [ $? = 8 ]; check "rc.limit" $?
echo "TESTS $((pass+fail))  OK $pass  DIFFERENT $fail"
[ -n "$failed" ] && echo "DIFFERENT:$failed"
rm -rf "$W"
[ $fail = 0 ]
