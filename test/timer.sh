#!/bin/sh

echo
echo "Performance timings: $(date)"
echo
git log -n 1
echo
echo ./timer 1000 ./jsnpgtest json/input/canada.json
{ ./timer 1000 ./jsnpgtest json/input/canada.json > /dev/null ; } 2>&1
echo
echo ./timer 1000 ./jsnpgtest json/input/citm_catalog.json
{ ./timer 1000 ./jsnpgtest json/input/citm_catalog.json > /dev/null ; } 2>&1
echo
echo ./timer 1000 ./jsnpgtest json/input/twitter.json
{ ./timer 1000 ./jsnpgtest json/input/twitter.json > /dev/null ; } 2>&1
echo
echo ./timer 1000 ./jsnpgtest json/input/pass01.json
{ ./timer 1000 ./jsnpgtest json/input/pass01.json > /dev/null ; } 2>&1

echo "====================================================================="
