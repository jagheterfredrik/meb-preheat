# meb-preheat
Investigating battery preheating on older MEB cars such as the ID.4. This is currently a scratchpad for my research and ideas.

Lots of general information available from [NHTSA](https://static.nhtsa.gov/odi/tsbs/2021/MC-10186407-0001.pdf).

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

## The J840 (BMS/BMCe) method
As a third option, I have been looking into if the J840 exposes functionality for turning on preheating by talking directly to it on the EV-CAN bus, this would require a man-in-the-middle harness in the gateway (located behind the glove box).

I got a hold of the EV_BMCeVWBSMEB ODX decription of the unit but can only find the output test used by OBD11. It might works to trigger it directly over the EV-CAN bus without hitting the firewall.
I also got hold of the J840 firwmare (FL_0Z1915184J_1041_V001_S.frf). Inside is an ODX container with the AES encrypted firmware but I haven't managed to find the key.

Open questions:
 - Does the J840 have an external EEPROM to dump the firmware from?
 - Can we simply call the output test UDS on the EV-CAN bus?
 - What connector do we need to build the harness?
