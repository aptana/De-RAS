#!/usr/bin/env bash
# Assembles the Windows distribution folder from a MinGW cross build and builds the NSIS installer.
#   tools/package/make_windows_dist.sh <build-win dir> <qt6-win prefix> <out dir>
set -euo pipefail
SRC="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="$1"; QT="$2"; OUT="$3"
VERSION=$(grep -oP 'DERAS_VERSION "\K[^"]+' "$BUILD/generated/Version.h")
LABEL=$(grep -oP 'DERAS_RELEASE_LABEL "\K[^"]+' "$BUILD/generated/Version.h")
STAGE="$OUT/De-RAS-$VERSION-win64"
rm -rf "$STAGE"; mkdir -p "$STAGE"/{platforms,styles,licenses,docs}
STRIP=x86_64-w64-mingw32-strip
cp "$BUILD/ui/De-RAS.exe" "$BUILD/deras-steady.exe" "$BUILD/deras-roundtrip.exe" "$STAGE/"
cp "$QT"/bin/Qt6{Core,Gui,Widgets}.dll "$STAGE/"
cp "$QT/plugins/platforms/qwindows.dll" "$STAGE/platforms/"
cp "$QT/plugins/styles/qwindowsvistastyle.dll" "$STAGE/styles/"
GCCDIR=$(dirname "$(x86_64-w64-mingw32-g++-posix -print-libgcc-file-name)")
for d in libstdc++-6.dll libgcc_s_seh-1.dll; do
  f=$(find "$GCCDIR" /usr/lib/gcc/x86_64-w64-mingw32 -name "$d" -path '*posix*' | head -1); cp "$f" "$STAGE/"; done
cp /usr/x86_64-w64-mingw32/lib/libwinpthread-1.dll "$STAGE/"
$STRIP --strip-unneeded "$STAGE"/*.exe "$STAGE"/*.dll "$STAGE"/platforms/*.dll "$STAGE"/styles/*.dll
cp "$SRC/branding/icons/De-RAS.ico" "$STAGE/"
cp "$SRC/THIRD_PARTY_NOTICES.txt" "$SRC/installer/NOTICE.txt" "$STAGE/"
cp "$SRC"/third_party/qt/* "$STAGE/licenses/"
cp "$SRC"/resources/fonts/LICENSE-*.txt "$STAGE/licenses/"
for f in "$SRC"/docs/manual/*; do [ -f "$f" ] && cp "$f" "$STAGE/docs/"; done
# Reference documents keep their packaged names (FEATURE_MATRIX.md, ...). The
# full working tree has <NAME>.md; the published repository has <NAME>_EN.md
# only, so fall back to that (and warn if neither exists).
for n in FEATURE_MATRIX TESTING FILE_FORMATS; do
  if [ -f "$SRC/docs/$n.md" ]; then cp "$SRC/docs/$n.md" "$STAGE/docs/$n.md"
  elif [ -f "$SRC/docs/${n}_EN.md" ]; then cp "$SRC/docs/${n}_EN.md" "$STAGE/docs/$n.md"
  else echo "warning: docs/$n.md and docs/${n}_EN.md not found; not packaged" >&2; fi
done
# qt.conf keeps Qt from looking for plugins anywhere but next to the exe
printf '[Paths]\nPrefix = .\nPlugins = .\n' > "$STAGE/qt.conf"
makensis -V2 -DVERSION="$VERSION" -DLABEL="$LABEL" -DSTAGE="$STAGE" -DOUTDIR="$OUT" "$SRC/installer/De-RAS.nsi"
( cd "$OUT" && rm -f "De-RAS-$VERSION-win64-portable.zip" && zip -qr "De-RAS-$VERSION-win64-portable.zip" "$(basename "$STAGE")" )
ls -la "$OUT"
