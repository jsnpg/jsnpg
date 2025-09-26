#!/bin/bash

while read line; do
        echo "$line"
        ./number_parsers "$1" "$line"
done < "$2"
