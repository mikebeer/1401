#!/bin/sh
# mkkit.sh N - make the installable kit build N: dist/IBM1401-kit-bN.zip
#   CMS/  every CMS file under its CMS name (FN.FT): the C sources, the
#         EXECs, the samples (and the TEXT decks if tools/mkcms.sh made them)
#   PC/   ibm1401 and autocoder for Linux x86-64 (static)
#   README-KIT.TXT  installation and first steps
set -e
N=${1:?usage: mkkit.sh build-number}
B=$(cd "$(dirname "$0")/.." && pwd)
grep -q "BUILD   \"$N\"" "$B/src/version.h" || {
  echo "mkkit: src/version.h says another build; set BUILD \"$N\" first"; exit 1; }
K=IBM1401-kit-b$N
W=$B/build/kit/$K
rm -rf "$B/build/kit"; mkdir -p "$W/CMS" "$W/PC" "$B/dist"
"$B/tools/amalgamate.sh" "$B/build/cms" >/dev/null
cp "$B/build/cms/IBM1401.C" "$B/build/cms/AUTOCODR.C" "$W/CMS/"
for t in IBM1401 AUTOCODR; do
  [ -f "$B/build/cms/$t.TEXT" ] && cp "$B/build/cms/$t.TEXT" "$W/CMS/"
done
cp "$B"/cms/*.EXEC "$W/CMS/"
for f in "$B"/samples/*.s "$B"/tests/src/*.s; do
  n=$(basename "$f" .s | tr a-z A-Z)
  cp "$f" "$W/CMS/$n.AUTOCODE"
done
cp "$B/tests/src/t05cards.dat" "$W/CMS/T05CARDS.DATA"
cp "$B/tests/src/t07cons.con" "$W/CMS/T07CONS.CONSOLE"
cc -std=c89 -O2 -static -o "$W/PC/ibm1401" "$B"/src/ibm1401.c "$B"/src/cpu.c "$B"/src/io.c "$B"/src/bcd.c
cc -std=c89 -O2 -static -o "$W/PC/autocoder" "$B"/src/autocoder.c "$B"/src/bcd.c
sed "s/@BUILD@/$N/g" "$B/tools/README-KIT.TXT" > "$W/README-KIT.TXT"
cp "$B/docs/MANUAL.md" "$W/"
( cd "$B/build/kit" && rm -f "$B/dist/$K.zip" && zip -qr "$B/dist/$K.zip" "$K" )
echo "mkkit: dist/$K.zip ($(ls "$W/CMS" | wc -l) CMS files)"
