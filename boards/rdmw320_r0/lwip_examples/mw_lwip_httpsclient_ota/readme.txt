Overview
========

The lwip_httpsclient_ota demo application demonstrates OTA update for mcufw through HTTPS client set up on lwIP TCP/IP and the MbedTLS stack with FreeRTOS. The user should place the new version of firmware on HTTPS server and provide the URL for device to update it.


Toolchain supported
===================
- IAR embedded Workbench  8.50.9
- GCC ARM Embedded  10.2.1

Hardware requirements
=====================
- Mini USB cable
- RDMW320-R0 board
- J-Link Debug Probe
- Personal Computer

Board settings
==============

Prepare the Demo
================
-------- Prepare the server on PC -----------
1. Install apache2 http server and openssl
	# sudo apt-get install apache2
	# sudo apt-get install openssl

2. Stop the Apache2 http server
	# sudo systemctl stop apache2

3. Modify Apache2 confifgurations
	A. Modify ServerName in /etc/apache2/apache2.conf (ServerName is IP(for e.g 192.168.1.2) of the WLAN
	   network interface where on the Ubuntu 16 Host)

		ServerName 192.168.1.2

	B. Modify file "/etc/apache2/sites-available/default-ssl.conf" with following parameters

		ServerAdmin webmaster@192.168.1.2
		ServerName 192.168.1.2
		DocumentRoot /var/www/html

	C. Modify file "/etc/apache2/sites-available/000-default.conf" with following parameters

		ServerAdmin webmaster@192.168.1.2
		ServerName 192.168.1.2
		DocumentRoot /var/www/html
		Redirect "/" "https://192.168.1.2/"

4. Create certificates for https server
	# sudo openssl req -new > new.ssl.csr
	NOTE: Here it will ask for details for the certificates, make sure the common name should be your IP address
	which is assigned in apache2 config file. And it will ask for password to create for this certificate you should
	remember this for further use.

	# sudo openssl rsa -in privkey.pem -out new.cert.key
	NOTE: Here you have to submit the password which given in above step

	# sudo openssl x509 -in new.ssl.csr -out new.cert.cert -req -signkey new.cert.key -days 365

5. Copy the new generated certificate and key
	# sudo cp new.cert.cert /etc/ssl/certs/server.crt
	# sudo cp new.cert.key /etc/ssl/private/server.key

6. Modify the Apache2 configuration parameters for secure connection in file "/etc/apache2/sites-available/default-ssl.conf"

	SSLCertificateFile      /etc/ssl/certs/server.crt
	SSLCertificateKeyFile   /etc/ssl/private/server.key

7. Adjust the Firewall

	A. Check the available profiles
	# sudo ufw app list

	B. Check current running settings
	# sudo ufw status

	C. Update following profiles
	# sudo ufw allow 'Apache Full'
	# sudo ufw delete allow 'Apache'

8. Enable the changes in Apache2

	A. Enable Apache SSL module and mod_headers
	# sudo a2enmod ssl
	# sudo a2enmod headers

	B. Enable SSL Virtual Host
	# sudo a2ensite default-ssl

	C. Enable SSL parameters
	# sudo a2enconf ssl-params

9. Check no syntax errors in the files
	# sudo apache2ctl configtest

10. Restart Apache2 with updated configurations
	# sudo systemctl restart apache2

11. To verify connection
	Access https://192.168.1.2 in web browser it will prompt for security and authority.

-------- Prepare the client on RDMW320-R0 board -----------
User needs to modify the certificates in the application file <mw_lwip_httpsclient_ota/write_firmware.h> for
SRV_CRT_RSA_SHA256_PEM macro.

The example requires M4 core to load WIFI firmware from flash. So the partition table layout.bin and
the WIFI firmware xxx.fw.bin need to be prepared in the flash before running the example.
Use following steps to get the flash contents ready:
1.  Connect a USB cable between the host PC and Mini USB port on the target board.
2.  Connect j-link probe between the host PC and JTAG port on the target board.
3.  Open jlink.exe, execute following commands
    J-Link>connect
      Device>88MW320
      TIF>s
      Speed>
    J-Link>loadbin <sdk_path>\tools\boot2\layout.bin 0x1F004000
    J-Link>loadbin <sdk_path>\boards\rdmw320_r0\wifi_examples\common\mw32x_uapsta_W14.88.36.p173.fw.bin 0x1F150000

    To create your own layout.bin and wifi firmware bin for flash partition, please use the tool located at
    <sdk_path>\tools\mw_img_conv\mw320 to convert the layout configuration file and WIFI firmware to the images
    suitable for flash partition. For example,
    # python mw_img_conv.py layout layout.txt layout.bin
    # python mw_img_conv.py wififw mw32x_uapsta_W14.88.36.p173.bin mw32x_uapsta_W14.88.36.p173.fw.bin
    Please note the wifi firmware binary should not be compressed.

Now use the general way to debug the example
1.  Connect a USB cable between the host PC and Mini USB port on the target board.
2.  Open a serial terminal with the following settings:
    - 115200 baud rate
    - 8 data bits
    - No parity
    - One stop bit
    - No flow control
3.  Download the program to the target board.
4.  Launch the debugger in your IDE to begin running the demo.

To make the demo bootable on board power on, additional steps are needed as follows (for IAR and armgcc only):
1.  Because the debugger downloads the application without considering XIP flash offset setting in FLASHC, we creates
    a special linker file with manual offset to make the wifi example debuggable (without conflicting the partition
    table area in flash). To make it bootable, we need to change the linker file to the one in devices, e.g.
    <sdk_path>\devices\88MW320\iar\88MW320_xx_xxxx_flash.icf and build the application again.
2.  Create bootable MCU firmware
    Run <sdk_path>\tools\mw_img_conv\mw320\mkimg.sh with the built binary file, for example,
    # ./mkimg.sh mw_lwip_httpsclient_ota.bin
    Then mw_lwip_httpsclient_ota.fw.bin will be created.
3.  Write needed components to flash partition in jlink.exe according to partition table (layout.txt), you may skip
    the component which has not been changed on flash.
    J-Link>connect
      Device>88MW320
      TIF>s
      Speed>
    J-Link>exec SetFlashDLNoRMWThreshold = 0xFFFF        // SET RMW threshold to 64kB, so size < 64KB will be WMW.
    J-Link>loadbin <sdk_path>\tools\boot2\boot2.bin 0x1F000000
    J-Link>loadbin <sdk_path>\tools\boot2\layout.bin 0x1F004000
    J-Link>loadbin mw_lwip_httpsclient_ota.fw.bin 0x1F010000
    J-Link>loadbin <sdk_path>\boards\rdmw320_r0\wifi_examples\common\mw32x_uapsta_W14.88.36.p173.fw.bin 0x1F150000
4.  Reset your board and then the application is running.
Running the demo
================
1. When the demo runs successfully, the terminal will display the following:

		----------------------------------------
		[client mcufw OTA demo]
		----------------------------------------
		Build Time: Mar 17 2021--19:32:45 
		----------------------------------------
		Initialize WLAN Driver
		----------------------------------------
		MAC Address: 00:50:43:24:37:10 
		[net] Initialized TCP/IP networking stack
		----------------------------------------
		app_cb: WLAN: received event 10
		----------------------------------------

2. Connect your board to Access Point
	# wlan-add 0 ssid <ssid> wpa2 <passphrase>
	Note: For open securiry, don't use wpa2 option

	# wlan-connect 0
	Note: Once the connection is done successfully, following events can be observed on terminal
		
		----------------------------------------
		app_cb: WLAN: received event 0
		----------------------------------------
		app_cb: WLAN: connected to network
		Connected to following BSS:
		SSID = [ssid], IP = [192.168.1.163]

3. Now all set to update the mcufw, execute fw_update command with firmware URL of your server
	# fw_update <url>
4. Wait for a minute, it will update firmware and reboot automatically else prompt an error
5. Once the updated firmware executes, the terminal shows:

		----------------------------------------
		[client mcufw OTA demo]
		----------------------------------------
		Build Time: Mar 17 2021--19:34:44 
		----------------------------------------
		Initialize WLAN Driver
		----------------------------------------
		MAC Address: 00:50:43:24:37:10 
		[net] Initialized TCP/IP networking stack
		----------------------------------------
		app_cb: WLAN: received event 10
		----------------------------------------

