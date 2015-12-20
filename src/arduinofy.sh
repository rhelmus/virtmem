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

mkdir -p "${DEST}/examples/serial_simple"
$COPY "${PWD}"/../examples/serial_simple/serial_simple.ino "${DEST}/examples/serial_simple"

mkdir -p "${DEST}/examples/serial_input"
$COPY "${PWD}"/../examples/serial_input/serial_input.ino "${DEST}/examples/serial_input"

mkdir -p "${DEST}/examples/spiram_simple"
$COPY "${PWD}"/../examples/spiram_simple/spiram_simple.ino "${DEST}/examples/spiram_simple"

mkdir -p "${DEST}/examples/multispiram_simple"
$COPY "${PWD}"/../examples/multispiram_simple/multispiram_simple.ino "${DEST}/examples/multispiram_simple"

mkdir -p "${DEST}/examples/sd_simple"
$COPY "${PWD}"/../examples/sd_simple/sd_simple.ino "${DEST}/examples/sd_simple"

mkdir -p "${DEST}/examples/alloc_properties"
$COPY "${PWD}"/../examples/alloc_properties/alloc_properties.ino "${DEST}/examples/alloc_properties"
