#!/bin/bash
tmp=`ps -aux | grep middleware | cut -d ' ' -f 2-6,25-40`     

prev=0
for i in $tmp
do
    if [[ "$i" == *"build/exec/middleware"* ]]
    then
        echo "$i is executing with pid $prev"
        exit 
    fi
    prev=$i
done

echo "Start middleware"
./build/exec/middleware