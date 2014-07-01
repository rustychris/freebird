
#include "freebird_config.h"

#include <i2c_t3.h>
#include <Adafruit_ADS1015_t3.h>

#include <RTClib.h>

#include <SdFat.h>

#include "filter.h"
#include "logger.h"

#include "I2Cdev.h"
#include "MPU6050.h"

Logger logger;

void setup() {
  Serial.begin(115200);
#ifdef BT2S
  BT2S.begin(BT2S_BAUD);
#endif
  logger.setup();
}

void loop() {
  logger.loop();
}
