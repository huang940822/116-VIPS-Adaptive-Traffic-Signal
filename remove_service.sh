#!/bin/bash

sudo systemctl stop middleware.service
sudo systemctl disable middleware.service

sudo systemctl stop capacity_check.service
sudo systemctl disable capacity_check.service

sudo systemctl stop network_check.service
sudo systemctl disable network_check.service

sudo systemctl stop gnss_status_check.service
sudo systemctl disable gnss_status_check.service
