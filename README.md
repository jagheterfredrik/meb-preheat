# MEB Preheat
This repository contains information on how to build a cable harness that can inject preheating messages to MEB cars, allowing for battery preheating while driving. Tested on ID.4 but should look similar on ID.3, Skoda Enyaq, ID.Buzz, Cupra Born, and others.

## Step 1
Buy [a cable](https://www.aliexpress.com/item/1005008006846323.html) and a [USB2CANFDv1](https://www.aliexpress.com/item/1005007126451299.html). You will also need a ST-Link V2 (or [a clone](https://www.aliexpress.com/item/33010611392.html)) and a 1.5m USB-C to USB-C cable.

## Step 2
Flash the custom firmware on the USB2CANFDv1, start by [completely emptying the flash](https://github.com/WeActStudio/WeActStudio.USB2CANFDV1?tab=readme-ov-file#how-to-completely-empty-flash), then build the code or flash the hex file in the Github releases (I'm using STM32CubeProgrammer, if you're using a clone ST-Link you might have to set "Shared" to "Enabled" and press refresh next to the serial number, then connect, open the hex file, followed by Download). Flip the CAN termination switch on the USB2CANFDv1 (marked 120Ω) to OFF.

## Step 3
Cut the following strands of the aliexpress cable and plug them in accordingly on the USB2CANFDv1 screw terminal. There are pin# marking on the connector making them easy to identify.

 - Pin 15: CAN-L
 - Pin 16: CAN-H
 - Pin 31: GND

The end product should look something like

 ![IMG_6316](https://github.com/user-attachments/assets/7fa45861-95a9-4cbc-8cf9-8b60646e177c)

## Step 4
Plug it into the car. The cable harness is attached between the original 40-pin gateway cable and its connector. They are located behind the glove compartment, while one could do it without removing any trim, I opt to remove the four T20 screws holding the flappy textile noise dampener to get better access. Attach the USB-C cable to the USB2CANFDv1 and guide it towards the center console.

![IMG_6314](https://github.com/user-attachments/assets/5db893a3-dfbd-468c-a423-8d09f005f737)

## Step 5
You can now enable the battery heater by plugging the USB-C cable into one of the USB-C outlets in the center console. The heater will only heat the battery as long as the battery management system indicates that the battery is too cold for optimal charging performance.
