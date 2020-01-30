Refer to http://backreference.org/2010/03/26/tuntap-interface-tutorial/ 

-----

dameon, a tunneling program using udp socket supporting NB-IoT Device or Management Host  
	for NB-IoT Device uses -c argument  
	for Management Host uses -s argument  
switch_server, a proxy udp server, redirecting two sockets between NB-IoT devices and Management Host  

    |--------------|        |-----------|      |--------|
    |  NB-IoT Rpi  |========|   proxy   |======|  Host  |
    |--------------|        |-----------|      |--------|

-----

To compile two programs  
$ gcc daemon_transparentMODE_slip.c slip.c -o daemon  
$ gcc switch_server_transparent_slip_nomap.c slip.c -o switch  

-----

virtual interface setting( sudo for root authority)  

$ sudo openvpn --mktun --dev tun2  
$ sudo ip link set tun2 up  
$ sudo ip addr add 192.168.0.2/24 dev tun2		//set virtual ip address  
$ sudo ip link set dev tun2 mtu 1200  			//set tun interface mtu to 1200 bytes  
$ ifconfig 										//check all config is right  

--------

Usage:   

for daemon:  
	$ sudo ./daemon -i <ifacename> [-s|-c <proxyIP>] [-p <port>] [-d]  
	
-i <ifacename>: Name of interface to use (mandatory)  
-s -c <proxyIP>: run in Management Host (-s), or NB-IoT Device (-c) and specify proxyIP address (mandatory)  
-p <port>: specify proxy's open port (mandatory)  
-d: outputs debug information while running  

for switch:  
	$ ./switch <Port for NB-IoT Device> <Port for Management Host>  
	
--------