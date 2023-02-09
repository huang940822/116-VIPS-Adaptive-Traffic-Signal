# How to install RSU middleware service 
## Install step
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
## Installing service in RSU_Controller_master folder
* There are three service in RSU_Controller_master such as middleware.service,capacity_check_service and
network_check.service. You have to make sure path is correct.
1. You have to check your path of RSU_Controller_master folder
```bash=
pwd
```
* Example as follow:
```bash=
/home/oslab/RSU_Controller_master
```
2. Check the ExecStart and WorkingDirectory in all services is correct.
* middleware.service as follow.
    * ExecStart=/home/oslab/RSU_Controller_master/build/exec/middleware
    * WorkingDirectory=/home/oslab/RSU_Controller_master
3. Go to RSU_Controller_master folder
```bash=
cd RSU_Controller_master
```
4. Copy services in RSU_Controller_master folder to system
* I take middleware.service as example, other services are the same.
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
* Successful status of middleware.service as follow.
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


