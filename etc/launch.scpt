#!/usr/bin/osascript

on run argv
    tell application "Terminal"
        do script "cd ~/Documents/Github/parcel/build && ./parcel -a localhost -u " & (item 1 of argv)
    end tell
end run