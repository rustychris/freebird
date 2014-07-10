These libraries have been copied from a number of sources, and lightly modified to
allow better operation on the Teensy and better integration between tasks.  To 
my (RH) knowledge they are all open source, freely distributable, and I'm not stepping
on any toes having them in this repository.  

Adafruit ADC breakout code: https://github.com/adafruit/Adafruit_ADS1X15
 BSD License
 
Sparkfun MPU9150 breakout: https://github.com/sparkfun/MPU-9150_Breakout
 This includes both I2CDev and MPU6050 (which despite its name also communicates with
 the MPU9150.) These are under the MIT License.
 
RTCLib: https://github.com/mizraith/RTClib which is apparently a fork of Jeelabs'
 RTCLib.
 
SdFat: The Arduino SdFat library, under a GPL license.

i2c_t3: http://forum.pjrc.com/threads/21680-New-I2C-library-for-Teensy3
This is a Teensy-specific I2C library - it has been slightly modified to allow chaining
asynchronous events, and many of the above libraries have been modified to use i2c_t3 instead
of stock Arduino Wire.
