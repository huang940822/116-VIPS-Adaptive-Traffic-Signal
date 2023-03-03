# RSU_Controller_master
## Compiling Environment

1. Installing Make and GCC9
```bash=
sudo apt-get update
sudo apt-get upgrade
sudo apt install make
sudo apt install gcc
```
2. Access Serial Port
```bash=
sudo adduser oslab dialout
```
3. RS232 is plugged into **COM1**
## Setup and Start Program

1. Go to home directory
```bash=
cd ~
```
2. Clone the RSU repository
```bash=
git clone https://github.com/oslab-csie-ncku/RSU_Controller_master.git
```
3. Go to RSU_Controller_master folder
```bash=
cd RSU_Controller_master
```
4. Execute command `make` to build code
```bash=
make
```

5. You must check *config* file in RSU_Controller_master directory
    * *config* file is placed in `RSU_Controller_master/config` directory
    * *config* file for EVSP application is placed in `RSU_Controller_master/application/EVSP/config` directory
    * *_touching_area.txt* is placed in `RSU_Controller_master/application/EVSP/config` directory
    * *config* file for TSP application is placed in `RSU_Controller_master/application/TSP/config` directory   
    * *Supermatrix* file is placed in `RSU_Controller_master/application/TSP/config/RSU_supermatrix` directory

## Execute at systemd
* There are three service in RSU_Controller_master as folloe
    1. *middleware.service*
    2. *capacity_check_service*
    3. *network_check.service*

You have to make sure path is correct.

1. Check your path of RSU_Controller_master folder
```bash=
pwd
```
* Example as follow:
```bash=
/home/oslab/RSU_Controller_master
```
2. Check the `ExecStart` and `WorkingDirectory` in all services is correct.
* *middleware.service*
    * ExecStart=/home/oslab/RSU_Controller_master/build/exec/middleware
    * WorkingDirectory=/home/oslab/RSU_Controller_master
* *capacity_check_service*
    * ExecStart=/home/oslab/RSU_Controller_master/log_usage_check.sh
    * WorkingDirectory=/home/oslab/RSU_Controller_master/log
* *network_check.service*
    * ExecStart=/home/oslab/RSU_Controller_master/ping_test.sh
    * WorkingDirectory=/home/oslab/RSU_Controller_master
3. Go to RSU_Controller_master folder
```bash=
cd RSU_Controller_master
```
4. Copy services in RSU_Controller_master folder to system
* Take middleware.service as example, other services are the same.
```bash=
sudo cp middleware.service /etc/systemd/system/
```
5. Change permissions of services
```bash=
sudo chmod 644 /etc/systemd/system/middleware.service
```
6. Reload systemd configuration
```bash=
sudo systemctl daemon-reload
```
7. You can use `systemctl` command to start your service.
```bash=
sudo systemctl start middleware.service
```
8. You should let service can automatically start after booting.
```bash=
sudo systemctl enable middleware.service
```
9. Now, you can check the service status
```bash=
sudo systemctl status middleware.service
```
* Successful status of *middleware.service* as follow.
<img width="893" alt="截圖 2023-02-08 上午11 31 02" src="https://user-images.githubusercontent.com/46049179/217422489-22d2aead-0f89-440f-be8e-ce1917d2e041.png">

# RSU_CPS development journal
## Application layer
* Add a new application CPS.
    * Modify application_object, that adding new callback function **on_camera_packet_rx**.
    * CPS pseudocode:
![](https://i.imgur.com/CA0yAkw.png)
## Middleware
* Add a new package handler, **Smart_AVI_packet_rx_event_handler**, to handle packages from Smart AVI.
* Modify Heartbeat package handler, which fixes the bug causing com_send()'s error.
## Com_layer
* Add a new device type, **FROM_SMART_AVI**, to identify the package from Smart_AVI.
    * Packages which sent from port number **12345** will be classified in **FROM_SMART_AVI**.
* Make use of Heartbeat package to build the RSU's connection with OSlink.
    * Packages which sent from port number **10001** will be regarded as Heartbeat packages.


