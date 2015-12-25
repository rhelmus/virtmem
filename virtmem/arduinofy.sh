#!/bin/sh

COPY="cp -v"

if [ "$1" = "-s" ]; then
    COPY="ln -fvs" shift
fi

DEST="$1"
echo "destination: $DEST"

copyExample()
{
    mkdir -p "${DEST}/examples/$1"
    $COPY "$PWD/../examples/$1/$1.ino" "$DEST/examples/$1"

    if [ -f "${PWD}/../examples/$1/$1.h" ]; then
        $COPY "${PWD}/../examples/$1/$1.ino" "${DEST}/examples/$1"
    fi
}

mkdir -p "${DEST}"
$COPY "${PWD}"/alloc.h "${DEST}/"
$COPY "${PWD}"/base_alloc.h "${DEST}/"
$COPY "${PWD}"/base_alloc.cpp "${DEST}/"
$COPY "${PWD}"/base_vptr.h "${DEST}/"
$COPY "${PWD}"/config.h "${DEST}/"
$COPY "${PWD}"/sd_alloc.h "${DEST}/"
$COPY "${PWD}"/serial_alloc.h "${DEST}/"
$COPY "${PWD}"/serial_utils.hpp "${DEST}/"
$COPY "${PWD}"/serial_utils.h "${DEST}/"
$COPY "${PWD}"/spiram_alloc.h "${DEST}/"
$COPY "${PWD}"/static_alloc.h "${DEST}/"
$COPY "${PWD}"/utils.cpp "${DEST}/"
$COPY "${PWD}"/utils.h "${DEST}/"
$COPY "${PWD}"/virtmem.h "${DEST}/"
$COPY "${PWD}"/vptr.h "${DEST}/"
$COPY "${PWD}"/vptr_utils.h "${DEST}/"
$COPY "${PWD}"/vptr_utils.hpp "${DEST}/"

copyExample benchmark
copyExample serial_simple
copyExample serial_input
copyExample spiram_simple
copyExample multispiram_simple
copyExample sd_simple
copyExample alloc_properties
copyExample locking
