TEMPLATE = subdirs
CONFIG += ordered
SUBDIRS = src \
          test \
    benchmark
test.depends = lib
