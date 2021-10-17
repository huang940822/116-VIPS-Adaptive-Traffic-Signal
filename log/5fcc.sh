while true
do
	a=$(tac 2021-10-15\ 15.log|grep -i "5fcc" -B 5 -A 1 -m 1)
	echo "$a"

	sleep 1
done
