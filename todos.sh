#!/bin/bash

printf "parcel binary size:      %s  (%s)\n" "$(du -hs ./build/parcel)"  "$(cat ./build/parcel | wc -c)"
printf "parceld binary size:     %s  (%s)\n" "$(du -hs ./build/parceld)" "$(cat ./build/parceld | wc -c)"
printf "sloc:    %s\n" "$(cat src/*.* src/parcel/* src/parceld/* | egrep -v '^$' | egrep -v '^\w*\/\/' | wc -l)"
printf "TODOs:         %d\n\n" "$(grep -rHni todo src/ | wc -l)"
bash -c "git grep -n TODO"
bash -c "grep -rHni todo src/ "
