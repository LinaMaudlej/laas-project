#!/bin/bash

FILE="exp=8_tenants=1000_arrival=0.csv"
number_minus=$1
skip=$2
cur_skip=$skip
cur_num=$number_minus

while read line; do	
	echo $line | cut -d"," -f1 | tr -d $'\n'
	echo -n  ", "
	echo $line | cut -d"," -f2 | tr -d $'\n'
	echo -n  ","
	echo $line | cut -d"," -f3 | tr -d $'\n'
	echo -n  ","
	echo $line | cut -d"," -f4 | tr -d $'\n'
	echo -n  ","
	if (( cur_num !=  0  && cur_skip == 0 ))
	then
		echo $line | cut -d"," -f5 | awk '{print "0"}'
		cur_skip=$skip
		let cur_num-=1
	else
		echo $line | cut -d"," -f5 
		let cur_skip-=1
	fi
	
done < "$FILE"
