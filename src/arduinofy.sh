#!/bin/sh

COPY="cp -v"

if [ "$1" = "-s" ]; then
    COPY="ln -fvs" shift
fi

DEST="$1"
echo "destination: $DEST"

mkdir -p "${DEST}"

$COPY "${PWD}"/alloc.h "${DEST}/"
$COPY "${PWD}"/base_alloc.h "${DEST}/"
$COPY "${PWD}"/base_alloc.cpp "${DEST}/"
$COPY "${PWD}"/base_wrapper.h "${DEST}/"
$COPY "${PWD}"/config.h "${DEST}/"
$COPY "${PWD}"/sdfatlib_alloc.h "${DEST}/"
$COPY "${PWD}"/serram_alloc.h "${DEST}/"
$COPY "${PWD}"/serram_utils.hpp "${DEST}/"
$COPY "${PWD}"/serram_utils.h "${DEST}/"
$COPY "${PWD}"/spiram_alloc.h "${DEST}/"
$COPY "${PWD}"/static_alloc.h "${DEST}/"
$COPY "${PWD}"/utils.cpp "${DEST}/"
$COPY "${PWD}"/utils.h "${DEST}/"
$COPY "${PWD}"/utils.hpp "${DEST}/"
$COPY "${PWD}"/virtmem.h "${DEST}/"
$COPY "${PWD}"/wrapper.cpp "${DEST}/"
$COPY "${PWD}"/wrapper.h "${DEST}/"
$COPY "${PWD}"/wrapper_utils.h "${DEST}/"
