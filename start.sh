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

ABSPATH=$(readlink -f "$0") # /home/user/bin/foo.sh
SCRIPTPATH=$(dirname "$ABSPATH") # /home/user/bin
echo "Start middleware"
$SCRIPTPATH/build/exec/middleware