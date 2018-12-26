#!/bin/bash

trap "" USR1

TMP=$(mktemp)

for method in kill dummy mutex load procfs epoll; do
	echo -n "$method: "
	./benchmarks $method
done | tee $TMP

echo -e "\nNormalised:"

MIN=$(cat $TMP | awk '{print $2}' | sort -n | head -n1)

sort -nk2 $TMP | awk -v m=$MIN '{n=$2/m; print $1 " " n}'

rm $TMP
