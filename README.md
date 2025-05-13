# meb-preheat
Investigating battery preheating on older MEB cars such as the ID.4. This is currently a scratchpad for my research and ideas. It does look like the J840 method using UDS output test is viable!

Lots of general information available from [NHTSA](https://static.nhtsa.gov/odi/tsbs/2021/MC-10186407-0001.pdf).

## The J840 (BMS/BMCe) method
I have been looking into if the J840 exposes functionality for turning on preheating by talking directly to it on the EV-CAN bus, this would require a harness between the gateway and the rest of the network (located behind the glove box).

I got hold of the J840 firwmare (FL_0Z1915184J_1041_V001_S.frf). Inside is an ODX container with the AES encrypted firmware but I haven't managed to find the key.

I also got a hold of the EV_BMCeVWBSMEB ODX decription of the unit but could only find the output test used by OBD11 (see below). By bypassing the firewall by connecting after the gateway, the UDS output test can be activated over the CAN-EV bus! The harness was graciously custom made by FICI Electronic Connector store on Aliexpress, they now provide [a product](https://www.aliexpress.com/item/1005008006846323.html) which one can cut to hook into the CAN-EV bus.

 - Pin 11: +12V
 - Pin 15: CAN-L (of the CAN-EV bus)
 - Pin 16: CAN-L (of the CAN-EV bus)
 - Pin 31: GND

The CAN-EV bus is unfortunately named "powertrain CAN bus" in the ID.4 wiring diagram, even though it is a separate bus from the powertrain bus. To make matters even worse, the actual powertrain CAN bus is also named "powertrain CAN bus" (pin 13/14 on the gateway) in the wiring diagram.

![IMG_6314](https://github.com/user-attachments/assets/5db893a3-dfbd-468c-a423-8d09f005f737)
![IMG_6316](https://github.com/user-attachments/assets/7fa45861-95a9-4cbc-8cf9-8b60646e177c)

Triggering the heater from the CAN-EV bus works, it outputs max 5kW but throttles power to keep the heater output at 42C. "Dynamic limit for charging in amepere" stops increasing at different points, parameters unknown, sometimes at battery minimum of 21C with peak at 149.9kW (for my Wuxi battery). BMS 0% seems to correlate with 307.2V (3.2V / cell), fully charged (BMS 95.6%) is 394.75V (4.12V per cell), indicating a top buffer of 4.4% and max voltage of 4.3V. Rapidgating seems to happen at battery max temp of 40.5C.

My 2022 ID.4 RWD (Wuxi battery, manufactured 2021-09, EU, v3.7) managed to receive 449.52A @ 371.5V (~165kW) when preheated to 25C battery min. SoC BMS was 19.2%, charging started at 12.4%, dynamic limit indicated 448.2A max.

It seems a new 82kWh battery was introduced ~Jan 2022 for RWD MEB cars. The cars manufactured prior seem to share the battery and charging curves with the GTX models, [see comparison](https://youtu.be/Z7BFLUTt_bI?t=186).

It also seems the only way to reach 175kW (470A @ 375V) is to charge when the `battery_min >= 30C` but `SoC_bms < 20%`. It will only sustain those 470A while `SoC_bms < 24%`.

Open questions:
 - Does the J840 have an external EEPROM to dump the firmware from

## The J533 (gateway) method
The folks over at OBD11 has figured out a way to heat the battery using a UDS output test through the gateway to the battery module (J840, module 8C). This allows you to heat the battery for 5 minutes but requires your hood to be opened before being accepted. This is cool but not very practical for the purpose of preheating before DC fast charging while travelling. The output test can be re-run but the gateway firewall will lock you out after driving 200km.

## The Z132 (heater) method
My initial hypothesis was to man-in-the-middle the LIN communication between the J840 and the Z132 heater, turning the heater on. This unfortunately does not trigger the BMS to activate the circulation pump, as it is controlled by the temperature reported by G898/G898 sensors on the battery coolant inlet.

Relevant part showing the coolant circuit while heating the battery:

<img width="410" alt="Screenshot 2025-03-14 at 15 00 18" src="https://github.com/user-attachments/assets/889807ee-34fd-44f2-a610-42f394f634ba" />

The heater seems to be made by BorgWarner, and the pinout of the low voltage connector (black, connector type 1J0973714):
 - 1: +12V
 - 2: GND
 - 4: LIN

See the ID.4 wiring diagram, available from e.g. [vwidtalk.com](https://www.vwidtalk.com/threads/repair-manual-and-all-kinds-of-id-4-information.14263).

<img width="391" alt="Screenshot 2025-03-14 at 15 06 33" src="https://github.com/user-attachments/assets/dd0b78ba-e854-4e69-aba7-3779fff136a7" />

The LIN communication is documented is different from [older VW/VAG heaters](https://openinverter.org/wiki/Volkswagen_Heater#LIN_Bus_Communication).

Every 50ms a PID is polled, alternating between:
 - ID 15 (0x0F) for feedback
   - Byte 1: Current (0.25*X in amps)
   - Byte 2: HV battery voltage (2*X in volt)
   - Byte 3,4: Status? Always 0
   - Byte 5: LV battery voltage (0.1*X in volt)
   - Byte 6, 7, 8: Temp heater, in, out. Offset -50
 - ID 28 (0x1C) for control
   - Byte 3, bit 6: on/off
   - Byte 3, bit 0-5 and byte 4 bit 0-1: duty

When turning on the heater, the J840 BMS ramps the duty cycle at a rate 2% / second and stops at 99.

The J848 heater also implements UDSonLIN and the car issues Read data by identifier on startup to which the heater responds:
- F187: "1EE963231  "
- F189: "0020"
- F1A3: "H04"
- F191: "1EE963231  "
- F18C: "1EE96323121239000693"
- F17C: "BEO-BEO27.08.2100010693"
- F197: "J848 HV-PTC  "
