#!/bin/bash
EXE=""
OS="$(uname)"
if [[ $OS == "Windows_NT" ]]; then
	EXE='.exe'
fi

printf "parcel binary size:      %s   (%s)\n" "$(du -hs ./build/parcel${EXE})" "$(cat ./build/parcel${EXE} | wc -c)"
printf "parceld binary size:     %s  (%s)\n" "$(du -hs ./build/parceld${EXE})" "$(cat ./build/parceld${EXE} | wc -c)"
printf "source lines of code:    %d\n" "$(cat src/*.* src/parcel/* src/parceld/* | egrep -v '^$' | egrep -v '^\w*\/\/' | wc -l)"
printf "number of todos:         %d\n" "$(grep -rHni todo src/ | wc -l)"
bash -c "grep -rHni todo src"
