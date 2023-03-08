#!/bin/bash

path=$(cd "$(dirname "$0")"; pwd)
cd $path

echo $path

# capacity_check.service
echo "[Unit]
Description=capacity check and del old files

[Service]
User=oslab
ExecStart=$path/log_usage_check.sh
WorkingDirectory=$path/log
Restart=always
[Install]
WantedBy=multi-user.target" > capacity_check.service

# network_check.service
echo "[Unit]
Description=restart ipc daemon
after=network.target

[Service]
User=root
ExecStart=$path/ping_test.sh
WorkingDirectory=$path
Restart=always
RestartSec=10s
TimeoutSec=infinity

[Install]
WantedBy=multi-user.target" > network_check.service

# middleware.service
echo "[Unit]
Description=middleware daemon
after=network.target

[Service]
User=root
ExecStart=$path/start.sh
WorkingDirectory=$path
Restart=always
RestartSec=10s
TimeoutSec=infinity

[Install]
WantedBy=multi-user.target" > middleware.service

mv capacity_check.service /etc/systemd/system/capacity_check.service
chmod 644 /etc/systemd/system/capacity_check.service

mv network_check.service /etc/systemd/system/network_check.service
chmod 644 /etc/systemd/system/network_check.service

mv middleware.service /etc/systemd/system/middleware.service
chmod 644 /etc/systemd/system/middleware.service

systemctl daemon-reload
systemctl enable capacity_check.service
systemctl start capacity_check.service
systemctl enable network_check.service
systemctl start network_check.service
systemctl enable middleware.service
systemctl start middleware.service

rm install.sh