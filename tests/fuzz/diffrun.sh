#!/bin/sh
# diffrun.sh DIR: assemble every DIR/*.s with AUTOCODR and with Van
# Snyder's Autocoder (VSAC), run each deck on IBM1401 and SimH (SIMH),
# and compare decks and printouts.  One summary line at the end.
# VSAC and SIMH name the reference programs.
D=$1; B=$(cd "$(dirname "$0")/../.." && pwd)
VSAC=${VSAC:-vsac}; SIMH=${SIMH:-i1401}
n=0; deckok=0; prtok=0; bad=""
for s in "$D"/*.s; do
  f=${s%.s}; n=$((n+1))
  rm -f $f.vcd
  $VSAC -b Ix -o $f.vcd $s >/dev/null 2>&1
  $B/autocoder $s -deck $f.cd -nolist >/dev/null 2>&1
  if cmp -s $f.cd $f.vcd; then deckok=$((deckok+1)); else bad="$bad deck:$(basename $f)"; fi
  printf 'attach cdr %s.cd\nattach -n lpt %s.simh\nboot cdr\nquit\n' $f $f > $f.sim
  timeout -s KILL 20 $SIMH $f.sim > $f.simlog 2>&1; simrc=$?
  $B/ibm1401 $f.cd -print $f.mine -limit 20000000 > $f.mylog 2>&1; myrc=$?
  if cmp -s $f.mine $f.simh; then prtok=$((prtok+1))
  elif [ $simrc = 137 ] && [ $myrc = 8 ]; then prtok=$((prtok+1)); loops="$loops $(basename $f)"
  else bad="$bad print:$(basename $f)"; fi
done
[ -n "$loops" ] && echo "ENDLESS LOOPS ON BOTH (COUNTED AS SAME):$loops"
echo "PROGRAMS $n  DECKS IDENTICAL $deckok  PRINTOUTS IDENTICAL $prtok"
[ -n "$bad" ] && echo "DIFFERENT:$bad"
[ $deckok = $n ] && [ $prtok = $n ]
