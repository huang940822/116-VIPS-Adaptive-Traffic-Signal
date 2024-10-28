#!/bin/bash
output=$(echo | ssh -o StrictHostKeyChecking=no root@192.168.100.3 '/usr/bin/gnss-status-check.sh' 2>/dev/null)
if echo "$output" | grep -q "GNSS is working well"; then
        echo "GNSS is working well"
        output=$(echo | ssh -o StrictHostKeyChecking=no root@192.168.100.3 '/usr/bin/gnss_health' 2>/dev/null)
        output2=$(echo "$output" | grep "fix mode")
        echo "$output2"
        if echo "$output2" | grep -q "FIX_MODE_3D"; then
                echo "fix mode is working well"
        else
                echo "fix mode is not working well"
                echo | ssh -o StrictHostKeyChecking=no root@192.168.100.3 '/usr/bin/gnss-reset.sh'
        fi
else
        echo "GNSS is not working well"
        echo | ssh -o StrictHostKeyChecking=no root@192.168.100.3 '/usr/bin/gnss-reset.sh'
fi