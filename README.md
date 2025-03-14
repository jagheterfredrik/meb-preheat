# meb-preheat
Investigating battery preheating on older MEB cars such as the ID.4. This is currently a scratchpad for my research and ideas.

Lots of general information available from [NHTSA](https://static.nhtsa.gov/odi/tsbs/2021/MC-10186407-0001.pdf).

## The J533 (gateway) method
The folks over at OBD11 has figured out a way to heat the battery using a UDS output test through the gateway to the battery module (J840, module 8C). This allows you to heat the battery for 5 minutes but requires your hood to be opened before being accepted. This is cool but not very practical for the purpose of preheating before DC fast charging while travelling. The output test can be re-run but the gateway firewall will lock you out after driving 200km.

## The Z132 (heater) method
Current hypothesis is to man-in-the-middle the LIN communication between the J840 and the Z132 heater, turning the heater on.

Relevant part showing the coolant circuit while heating the battery:

<img width="410" alt="Screenshot 2025-03-14 at 15 00 18" src="https://github.com/user-attachments/assets/889807ee-34fd-44f2-a610-42f394f634ba" />

The heater seems to be made by DBK and has been investigated a bit over [openinverter.org](https://openinverter.org/forum/viewtopic.php?t=2211), particularily relevant is the LIN bus info:

Low voltage connector (black, connector type 1J0973714) pinout
 - 1: +12V
 - 2: GND
 - 4: LIN

Which agrees with the ID.4 wiring diagram, available from e.g. [vwidtalk.com](https://www.vwidtalk.com/threads/repair-manual-and-all-kinds-of-id-4-information.14263).

<img width="391" alt="Screenshot 2025-03-14 at 15 06 33" src="https://github.com/user-attachments/assets/dd0b78ba-e854-4e69-aba7-3779fff136a7" />

The LIN communication is documented in the thread as "The feedback comes on ID48. Byte 0 is power. 13 for 770W, 26 for 1540W. ID28 is sent for control. Byte 0 is power, last bit of byte 1 starts and stops."

The thread goes on to say that the power is a value in percent 0-100 of a maximum of 5kW (although the datasheet mentions a boost mode of 7kW). The thread also "...confirmed the heater self regulates if it gets too hot..."

According to the NHTSA document: "The coolant temperature sensors are connected directly to the J840 Battery Regulation Control Module. The control unit uses the sensor information to regulate the V590 High-Voltage Battery Coolant Pump." If this is the case, then hopefully all we have to do is to enable the Z132 PTC heater over LIN and the J840 module would regulate the coolant pump for us. If this is not the case, we would have to also interface with the V590 coolant pump, which is probably controlled using a PWM signal.

Open questions:
 - Is my heater the same as dicsussed in the thread? (It looks visually similar)
 - Will the J840 turn on the V590 coolant pump for me?
 - Will the J840 throw an error when the Z132 heater is running without it saying so? (Thermal runaway protection?)

## The J840 (BMS/BMCe) method
As a third option, I have been looking into if the J840 exposes functionality for turning on preheating by talking directly to it on the EV-CAN bus, this would require a man-in-the-middle harness in the gateway (located behind the glove box).

I got a hold of the EV_BMCeVWBSMEB ODX decription of the unit but can only find the output test used by OBD11. It might works to trigger it directly over the EV-CAN bus without hitting the firewall.
I also got hold of the J840 firwmare (FL_0Z1915184J_1041_V001_S.frf). Inside is an ODX container with the AES encrypted firmware but I haven't managed to find the key.

Open questions:
 - Does the J840 have an external EEPROM to dump the firmware from?
 - Can we simply call the output test UDS on the EV-CAN bus?
 - What connector do we need to build the harness?
