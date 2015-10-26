# Evrythng Marvell SDK

An Evrythng SDK based on Marvell WMSDK.

## Overview

The Evrythng Marvell SDK contains Evrythng API to work with Evrythng cloud, sample application which shows how to use this API and unit tests.

## EVRYTHNG API

You can find a programming guide in the following file:
```
lib/core/README.md
```

## Clone

`git clone git@github.com:evrythng/evrythng-marvell-sdk.git --recursive`

## Requirements

1. In order to compile library and run tests you should install on your local machine:
`sudo apt-get install gcc-arm-none-eabi`
2. Go to **extranet.marvell.com** and download the latest WMSDK. This SDK was tested with WMSDK version 3.2.12

## Configuring

Copy file **config.mk.example** to **config.mk** and set all the variable values according to your needs following the instructions inside.

## Building

Building a library, demo application and tests is as easy as typing
```
make
```
To build demo application
```
make demo
```
To build tests application
```
make tests
```
Additionally you can set NOISY to 1 to see more output (0 by default) 
```
make NOISY=1
```
To clean the build files
```
make clean
```
## Running and flashing demo and tests applications

In addition you can use targets ending with **_flashprog**, **_ramload**, **_footprint**.
For example:
```
make demo_flashprog
```
to flash the demo app to the board,
```
make demo_ramload
```
to load the demo to the device RAM and run
```
make demo_footprint
```
to check the demo application footprint

## Demo Application

When you are connected to your marvell board via serial terminal press enter. You should see the prompt `#`.
Try `psm-dump` to see the current network and EVT configuration. 

### Network configuration
The following commands configure the network:
```
psm-set network ssid <<SSID>>
psm-set network passphrase <<Passphrase>
psm-set network security 4
psm-set network configured 1
```

After setting `psm-set network configured 1`, the network is configured. Press the reboot button on the device.

### Configure the data connection for the EVT cloud
The following commands configure the Marvell device to connect to the EVT:
```
psm-set evrythng url ssl://mqtt.evrythng.com:443
psm-set evrythng thng_id <<ThngId>>
psm-set evrythng api_key <<DeviceApiKey>>
```
Press the reboot button on the device

### How the demo application works

You can study the demo app source **apps/demo/main.c** and figure out what the application does.

The key features of the application are:

1. Subscribe to led actions and control leds on notifications from Evrythng Cloud
2. Publish properties when the buttons are pressed
3. Publish actions when the buttons are pressed

## Creating your own application

1. Go to apps folder and copy-rename demo application
2. Open Makefile and copy-paste demo(_clean/_flashprog/_ramload/_footprint) targets ommiting what is not relevant to you.
3. You can now start building/flashing and running your application calling targets you've just created.
