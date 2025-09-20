#!/bin/sh

echo
echo "Performance statistics: $(date)"
echo

perf stat -d ./testutil -t 1 json/input/canada.json 2>&1
echo
perf stat -d ./testutil -t 1 json/input/citm_catalog.json 2>&1
echo
perf stat -d ./testutil -t 1 json/input/twitter.json 2>&1
echo
perf stat -d ./testutil -t 1 json/input/pass01.json 2>&1

echo "====================================================================="
