TEMPLATE = subdirs
CONFIG += ordered
SUBDIRS = virtmem/src \
          test \
    benchmark
test.depends = lib
