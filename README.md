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
