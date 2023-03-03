#!/bin/bash

#available_usage=$(df | grep '/dev/sdb2' | awk '{ print $4 }') # get available usage of /dev/sda5

threshold=12000000 # the threshold of disk usage

while true
do
	
    	available_usage=$(df | grep '/dev/sdb2' | awk '{ print $4 }')
	while [ $available_usage -lt $threshold ]
	#while true
	do
    		cd /home/oslab/RSU_Controller_v3-master/log
		echo "delete oldest files"
		# delete the oldest file
    		ls -1t | tail -n -1 | xargs -d "\n" -I {} rm {}
    		available_usage=$(df | grep '/dev/sdb2' | awk '{ print $4 }')
		echo $available_usage
		
	done
	if [[ -f "empty" ]]
        then
                echo "empty file exist"
        else
                echo "create file named empty"
                touch empty
        fi
	sleep 1
done

