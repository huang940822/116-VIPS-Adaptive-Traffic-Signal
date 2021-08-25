while true
do
	a=$(tac 2021-08-24\ 11.log|grep -i "5fcc" -B 5 -A 1 -m 1)
	echo "$a"

	sleep 0.5
done
