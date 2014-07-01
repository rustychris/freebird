#include "serialmux.h"

SerialMux mySerial;

// Originally just for SdFunctions, but maybe it would be useful more
// generally
ArduinoOutStream cout(mySerial);

int SerialMux::read(void) { 
  int res=src()->read();
  if (res>=0)
    last_activity_millis=millis();
  return res;
}     
