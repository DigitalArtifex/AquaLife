AquaLife: Fish Lighting - LPD8806 Variant
==
Version: 0.1 ALPHA

Author: James Dudeck

Project: AquaLife

- Links
 * [AquaLife] Facebook page
 * [3rd-party] Algorithm to convert Kelvin to RGB

Description:

This software has been designed to run on the Arduino UNO,
but can be run on most Arduinos with no changes. This software
has been designed to work with LED strips that have embedded
chips that are compliant with the LPD8806 chip.

The Color class includes the logic of a 3rd party to convert
a color temperature, in Kelvin, to the related RGB values.
This code was converted from VB.

I personally used this design to emulate the light intensities
that the fish I was raising would likely see at the depths
they would naturally be found in.

- Two words of caution:

 * (Arduino) - The TimeTriggers are saved in the EEPROM. Be sure to not over-flow your EEPROM, as the size will vary from model to model.
 * (Lights) -  This setup uses the strips with the LPD8806 (or compliant) chipset. This chipset has a 7-bit resolution, which gives us 127 steps. With the LPD8806 variation of this light, if you try to increment or decrement with too high of a resolution (number of places beyond the decimal), it may do nothing.

[AquaLife]:https://www.facebook.com/groups/774109749313231/
[3rd-party]:www.tannerhelland.com/4435/convert-temperature-rgb-algorithm-code/