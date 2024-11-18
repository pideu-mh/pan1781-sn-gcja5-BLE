# Table of Contents <!-- omit in toc -->

- [Introduction](#introduction)
- [Hardware Setup](#hardware-setup)
- [Software Setup](#software-setup)
- [Usage](#usage)
- [Sensor Data Format](#sensor-data-format)

# Introduction

The goal of this project is to show the usage of the Panasonic SN-GCJA5 sensor in conjunction with the PAN1781 EVB.

The Panasonic [SN-GCJA5](https://na.industrial.panasonic.com/products/sensors/air-quality-gas-flow-sensors/lineup/laser-type-pm-sensor/series/123557/model/123559) is a laser particulate matter (PM) sensor that can be used for measuring air quality. It can be accessed via I2C interface. 

The Panasonic [PAN1781 EVB](https://pideu.panasonic.de/development-hub/pan1781/evaluation_board/user_guide/) is the evaluation board for the [PAN1781 module](https://industry.panasonic.eu/products/devices/wireless-connectivity/bluetooth-low-energy-modules/pan1781-nrf52820) which is based on the nRF52820 Bluetooth chipset by Nordic Semiconductor.

The sensor data of the SN-GCJA5 is collected via I2C and is forwarded as cleartext to a remote device via Bluetooth using the [Nordic UART Service (NUS)](https://docs.nordicsemi.com/bundle/ncs-latest/page/nrf/libraries/bluetooth_services/services/nus.html).

You can use this project with any recent version of the [nRF Connect SDK](https://docs.nordicsemi.com/bundle/ncs-latest/page/nrf/index.html) by Nordic Semiconductor.

# Hardware Setup

You have to attach the SN-GCJA5 PM sensor to the PAN1781 EVB as shown in the following picture.

> ⚠️ Make sure to set the jumper on P17 as shown so that the 5V power supply pin is enabled correctly. Otherwise the sensor will not be powered!

![image info](./images/hardware_setup.png)

# Software Setup

You have to follow the [Getting Started](https://pideu.panasonic.de/development-hub/pan1781/software/getting_started/) guide in the [Panasonic Wireless Connectivity Development Hub](https://pideu.panasonic.de/development-hub/) to setup your development environment.

But instead of setting up a new project, you have to clone this project and use the "Open an existing application" action from the "Welcome" section of the "nRF Connect" extension in Visual Studio Code instead.

Afterwards you can configure, build and program the application as explained in the guide.

# Usage

After you flashed the application you have to make sure to enable the "RTT Output" to see the debug and diagnostic output of the application.

You have to run a suitable application on a mobile device to show the sensor data, for example the for example the "UART" util service of the [nRF Toolbox](https://play.google.com/store/apps/details?id=no.nordicsemi.android.nrftoolbox&hl=de) app for Android (1).

![image info](./images/usage_1.png)

Choose and click the right peripheral device from the list of devices (2).

![image info](./images/usage_2.png)

After the connection has been established the sensor data are shown in the "Output" window.

![image info](./images/usage_3.png)

# Sensor Data Format

The sensor data is read from the sensor as-is and transferred in cleartext format. Different members of the sensor data are separated by a semicolon, while identifier and value are separated by a colon.

```
ST:41;PM1.0:0;PM2.5:181;PM10:203;PC0.5:0;PC1.0:1;PC2.5:2;PC5.0:0;PC7.5:0;P10.0:0
```

"ST" represents the status, "PM" the mass-density value and "PC" the particle count data.

For further information refer to the communication specification of the sensor.
