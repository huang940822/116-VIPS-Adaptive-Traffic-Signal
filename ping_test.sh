#!/bin/bash

reboot_midnight(){
    	while true
    	do	
		currenttime=$(date +%H:%M:%S)
		echo "$currenttime"
   		if [[ "$currenttime" > "00:00:01" ]] && [[ "$currenttime" < "00:00:15" ]]; then
     			#reboot
			echo "time to reboot"
			echo "system reboot at midnight">>/home/oslab/RSU_Controller_v3-master/log/system_reboot_$NOW.log
			reboot
   		fi

		sleep 1
	done
}



network_detect(){
	
	count=0;
	while true
	do
		cat /dev/null > /var/log/syslog
		cat /dev/null > /var/adm/sylog
		cat /dev/null > /var/log/wtmp
		cat /dev/null > /var/log/maillog
		cat /dev/null > /var/log/messages
		cat /dev/null > /var/log/openwebmail.log
		cat /dev/null > /var/log/maillog
		cat /dev/null > /var/log/secure
		cat /dev/null > /var/log/httpd/error_log
		cat /dev/null > /var/log/httpd/ssl_error_log
		cat /dev/null > /var/log/httpd/ssl_request_log
		cat /dev/null > /var/log/httpd/ssl_access_log
		ping -c 3 8.8.8.8

		if [ "$?" -ne 0 ]
		then	
			((count++))
	
			if [ "$count" -eq 3 ]
			then
				count=0
				NOW=$( date '+%F_%H:%M:%S' )
				echo "network fails"
				echo "network fails">>/home/oslab/RSU_Controller_v3-master/log/networkfail_$NOW.log
				reboot
			fi
	
		fi
	


		sleep 300 #10mins
	done
}


reboot_midnight &
network_detect &
wait
