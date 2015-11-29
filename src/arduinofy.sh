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

mkdir -p "${DEST}/examples/benchmark"
$COPY "${PWD}"/../examples/benchmark/benchmark.ino "${DEST}/examples/benchmark"
$COPY "${PWD}"/../examples/benchmark/benchmark.h "${DEST}/examples/benchmark"
