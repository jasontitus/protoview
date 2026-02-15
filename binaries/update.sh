#!/bin/sh
VERSION="1.4.3"
BINPATH="/Users/antirez/hack/flipper/official/build/f7-firmware-D/.extapps/tpms_reader.fap"
cp "$BINPATH" "tpms_reader_${VERSION}.fap"
git commit -a -m "Binary file updated to v${VERSION}."
