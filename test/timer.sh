#!/bin/sh

echo
echo "Performance timings: $(date)"
echo
git log -n 1
echo
echo ./testutil -t 1000 json/input/canada.json
{ ./testutil -t 1000 json/input/canada.json ; } 2>&1
echo
echo ./testutil -t 1000 json/input/citm_catalog.json
{ ./testutil -t 1000 json/input/citm_catalog.json ; } 2>&1
echo
echo ./testutil -t 1000 json/input/twitter.json
{ ./testutil -t 1000 json/input/twitter.json ; } 2>&1
echo
echo ./testutil -t 1000 json/input/pass01.json
{ ./testutil -t 1000 json/input/pass01.json ; } 2>&1

echo "====================================================================="
