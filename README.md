# EVRYTHNG Marvell SDK

An EVRYTHNG SDK for Marvell IoT SoC boards (based on WMSDK).

## Overview

The EVRYTHNG Marvell SDK contains an MQTT SDK to work with the EVRYTHNG cloud as well as sample applications and unit tests.

## EVRYTHNG API

You can find a programming guide in the following file:
```
lib/core/README.md
```

## Get the latest release

See [our release page](https://github.com/evrythng/evrythng-marvell-sdk/releases).


## Clone

Ensure you use the `--recursive` option as this project depends on the [EVRYTHNG C library](https://github.com/evrythng/evrythng-c-library):

`git clone git@github.com:evrythng/evrythng-marvell-sdk.git --recursive`

## Dependencies

1. In order to compile the library and run the tests you should install the following dependencies on your local machine:
`sudo apt-get install gcc-arm-none-eabi`
2. Download the latest [Marvell WMSDK](http://extranet.marvell.com). This SDK was tested with WMSDK version 3.2.12 but newer versions should be supported as well.

## Configuring

Copy the `config.mk.example` file to `config.mk` and configure it for your environment.

## Building

To build the library, demo application and tests just run:
```
make
```
To build the demo application only run:
```
make demo
```
To build the tests only run:
```
make tests
```
Additionally you can set NOISY to 1 to see more output (0 by default) 
```
make NOISY=1
```
To clean the build files run:
```
make clean
```
## Running and flashing the demo and tests applications

Additionally you can use targets ending with `_flashprog`, `_ramload`, v_footprint`.
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

When you are connected to your Marvell board via serial terminal press enter. You should then see the prompt `#`. Try `psm-dump` to see the current network and EVRYTHNG configurations.

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
The following commands configure the Marvell device to connect to EVRYTHNG:
```
psm-set evrythng url ssl://mqtt.evrythng.com:443
psm-set evrythng thng_id <<ThngId>>
psm-set evrythng api_key <<DeviceApiKey>>
```
Press the reboot button on the device.

### Understanding the demo application code

The source of the demo application is located in `apps/demo/main.c`. This app is a good example of using the EVRYTHNG Cloud API.

The key features of the application are:

1. Subscribing to the LED Action to control the LEDs when an [Action](https://dashboard.evrythng.com/documentation/api/actions) notification is sent via the EVRYTHNG Cloud.
2. Publishing [Properties](https://dashboard.evrythng.com/documentation/api/properties) when buttons are pressed.
3. Publish [Action](https://dashboard.evrythng.com/documentation/api/actions) when the buttons are pressed.

## Creating your own application

1. Go to the `apps` folder, copy and rename the demo application
2. Open the Makefile and copy-paste the `demo(_clean/_flashprog/_ramload/_footprint)` targets omitting the ones that are not relevant for your use case..
3. You can now start building/flashing and running your application by calling targets you've just created.
