#!/bin/bash
output=$(echo | ssh -o StrictHostKeyChecking=no root@192.168.100.3 '/usr/bin/gnss-status-check.sh' 2>/dev/null)
if echo "$output" | grep -q "GNSS is working well"; then
        echo "GNSS is working well"
else
        echo "GNSS is not working well"
        echo | ssh -o StrictHostKeyChecking=no root@192.168.100.3 '/usr/bin/gnss-reset.sh'
fi