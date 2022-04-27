#!/bin/bash

for i in {1..3}; do
	./launch.scpt "u${i}"
	sleep 1
done
