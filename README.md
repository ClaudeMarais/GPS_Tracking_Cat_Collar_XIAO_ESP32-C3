# GPS Tracking Cat Collar XIAO ESP32-C3
 An Arduino project to log GPS data to a MicroSD using the tiny XIAO ESP32-C3. The components are small and light enough to fit around a cat collar! Now we know where the cat runs around at night :-)

 This is a very simple project that reads GPS data and then writes the data to a MicroSD card in a format that can be used to create custom GoogleMaps (https://www.google.com/maps/d/)

LED Status:
* Off         - No power, check battery
* Constant on - Critical error, check if MicroSD card was inserted, otherwise perhaps some of the wires are not connected anymore
* Fast blink  - No valid GPS data. This is not a critical error, since the GPS might still be trying to aquire satelites
* Slow blink  - Everything is fine, we're getting and logging valid data!

# Hardware
* XIAO ESP32-C3, 21mm x 17.5mm
* BN-220 GPS, 22mm x 20mm
* SPI MicroSD Card module, 18mm x 18mm
* 160mAh Lipo battery. 20mm x 20mm (Lasts about an hour with this setup)
* Red LED 2.5V 20mA, Small :-)
* 1K ohm resistor, Very small ;-D (I just need a dim LED blink at night, so 1K works fine)

