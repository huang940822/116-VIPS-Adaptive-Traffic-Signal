#!/bin/bash
filename='111_iplist'
echo "input  filepath/filename:"
read middleware_path
while read line; do
	# reading each line
	# [ need space between '[]' ]
	if [[ $line == "#"* ]]; then
		continue  
	fi #end if
	if [ -z $line ]; then
		echo "new line"
		continue  
	fi #end if
	echo "$line"
	
	#scp  -o 'ProxyJump nckucsieos@223.200.250.100 -p 10122' $middleware_path oslab@$line:/home/oslab/RSU_Controller_v3-master
	
	#update ipc network check restart
	#echo "oslab"|ssh -J nckucsieos@223.200.250.100:10122 oslab@$line -tt  'sudo cp /home/oslab/RSU_Controller_v3-master/network_check.service /etc/systemd/system;sudo systemctl enable network_check.service;
	#sudo systemctl restart  network_check.service;sudo systemctl status network_check.service'
	
	#scp  -o 'ProxyJump nckucsieos@223.200.250.100 -p 10122' ping_test.sh oslab@$line:/home/oslab/RSU_Controller_v3-master/
	#echo "oslab"|ssh -J nckucsieos@223.200.250.100:10122 oslab@$line -tt  'sudo systemctl enable network_check.service;sudo systemctl restart  network_check.service;sudo systemctl status network_check.service'
	#echo "oslab"|ssh -J nckucsieos@223.200.250.100:10122 oslab@$line -tt  'sudo systemctl enable capacity_check.service;sudo systemctl restart  capacity_check.service;sudo systemctl status capacity_check.service'
	#echo "oslab"|ssh -J nckucsieos@223.200.250.100:10122 oslab@$line -tt  'sudo systemctl restart  network_check.service;sudo systemctl status network_check.service'


	#disable all middleware service
	#echo "oslab"|ssh -J nckucsieos@223.200.250.100:10122 oslab@$line -tt  'sudo systemctl restart middleware.service'
	#echo "oslab"|ssh -J nckucsieos@223.200.250.100:10122 oslab@$line -tt  'sudo systemctl stop middleware.service'
	#scp  -o 'ProxyJump nckucsieos@223.200.250.100 -p 10122' $middleware_path oslab@$line:/home/oslab/RSU_Controller_v3-master/build/exec/
	#scp  -o 'ProxyJump nckucsieos@223.200.250.100 -p 10122' config.txt oslab@$line:/home/oslab/RSU_Controller_v3-master/application/TSP/config/
	
	#scp  -o 'ProxyJump nckucsieos@223.200.250.100 -p 10122' $middleware_path oslab@$line:/home/oslab/RSU_Controller_v3-master/

	echo "oslabTainanBUS"|ssh -J nckucsieos@223.200.250.100:10122 -p 10003 oslab@$line -tt 'sudo systemctl status middleware.service'
	#echo "oslab"|ssh -J nckucsieos@223.200.250.100:10122 oslab@$line -tt  'sudo systemctl restart middleware.service;sudo systemctl status middleware.service'
	#echo "oslab"|ssh -J nckucsieos@223.200.250.100:10122 oslab@$line -tt  'sudo systemctl restart middleware.service;sudo systemctl status middleware.service'

	#scp  -o 'ProxyJump nckucsieos@223.200.250.100 -p 10122'  $middleware_path -p 10003 oslab@$line:/home/oslab/RSU_Controller/
	#echo "oslabTainanBUS"|ssh -J nckucsieos@223.200.250.100:10122 -p 10003 oslab@$line -tt 'sudo systemctl restart middleware.service'
	#scp  $middleware_path oslab@$line:/home/oslab/RSU_Controller_v3-master/build/exec/


	#for dsrc oslink update
	#ssh -J nckucsieos@223.200.250.100:10122,oslab@$line root@192.168.100.3  'pkill oslink' </dev/null
	#echo $?
	#scp -J nckucsieos@223.200.250.100:10122,oslab@$line $middleware_path root@192.168.100.3:/home/root/ext-fs/home/unex/oslink/build/exec/oslink
	#echo $?	
	#ssh -J nckucsieos@223.200.250.100:10122,oslab@$line root@192.168.100.3  'cd /home/root/ext-fs/home/unex/oslink/build/exec;./oslink --host_ip 192.168.100.3 --host_port 10000 --peer_ip 192.168.100.2 --peer_port 10001&' < /dev/null
	#echo $?

done < $filename

