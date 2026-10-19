#!/bin/sh
# mkcms.sh - cross-build IBM1401 TEXT and AUTOCODR TEXT for VM/370plus CMS
#
# Uses the VM/370plus M5f toolchain, as for IND$FILE: cmscc (GCC 13 -m31,
# newlib, cmsrt) and elf_to_cms.  On CMS then:
#     LOAD IBM1401 (CLEAR NOMAP      GENMOD IBM1401
#     LOAD AUTOCODR (CLEAR NOMAP     GENMOD AUTOCODR
# (INST1401 EXEC does this when it finds the TEXT decks.)
#
#   VMP   the vm370plus checkout   (default ../vm370plus)
#   E2C   elf_to_cms                (default /home/claude/m5f/elf_to_cms)
set -e
B=$(cd "$(dirname "$0")/.." && pwd)
VMP=${VMP:-$B/../vm370plus}
CMSCC=$VMP/arch/31bit/m5f/cmscc
E2C=${E2C:-/home/claude/m5f/elf_to_cms}
O=$B/build/cms
[ -x "$CMSCC" ] || { echo "mkcms: no cmscc at $CMSCC (set VMP)"; exit 1; }
[ -x "$E2C" ] || { echo "mkcms: no elf_to_cms at $E2C (set E2C)"; exit 1; }
"$B/tools/amalgamate.sh" "$O"
for m in IBM1401 AUTOCODR; do
  l=$(echo $m | tr A-Z a-z)
  cp "$O/$m.C" "$O/$l.c"
  "$CMSCC" -O2 -o "$O/$l.elf" "$O/$l.c"
  "$E2C" "$O/$l.elf" "$O/$m.TEXT"
  echo "mkcms: $O/$m.TEXT"
done
