#!/bin/bash

printf "parcel binary size:      %s   (%s)\n" "$(du -hs ./build/parcel)"  "$(cat ./build/parcel | wc -c)"
printf "parceld binary size:     %s  (%s)\n" "$(du -hs ./build/parceld)" "$(cat ./build/parceld | wc -c)"
printf "source lines of code:    %d\n" "$(cat src/*.* src/parcel/* src/parceld/* | egrep -v '^$' | egrep -v '^\w*\/\/' | wc -l)"
printf "number of TODOs:         %d\n" "$(grep -rHni todo src/ | wc -l)"
bash -c "grep -rHni todo src"
