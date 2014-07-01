#ifndef __FREEBIRD_CONFIG_H__
#define __FREEBIRD_CONFIG_H__

// #define BOARDPROTO
#define BOARDV01

#define FREEBIRD_VERSION_STR "0001"

// Configuration options for the Freebird
#ifdef __arm__
#if defined(__MK20DX128__) || defined(__MK20DX256__)
#define TEENSY3
#else
#error it is a teensy
#define DUE
#endif
#endif

#ifdef DUE
// These settings allow a stock Adafruit board to work on the Due
#define SOFTWARE_SPI
#define SD_PIN_CS 10
#define SD_PIN_MOSI 11
#define SD_PIN_MISO 12
#define SD_PIN_SCK 13
// this only matters if/when we have hardware SPI
#define SD_SPEED SPI_HALF_SPEED

#define ADC_RES 12
#endif

#ifdef TEENSY3
#define SD_PIN_CS 10
#define SD_PIN_MOSI 11
#define SD_PIN_MISO 12
#define SD_PIN_SCK 13
#define SD_SPEED SPI_HALF_SPEED

#define ADC_RES 16
#endif

// enable extra debugging messages
#define DEBUG_PRINT 1

#define CMDFILE "STARTUP.CMD"

// this MUST be a full 8.3 filename - like DATAxxxx.BIN
#define DATAFILETEMPLATE "DATAxxxx.BIN"

// all calls to SD storage become noop - note that
// data is still buffered the same way, it just never gets written out
// #define DISABLE_STORE

// remove all calls to RTC
#define RTC_ENABLE
// time the sampling based on the RTC pulses for low drift

#ifdef RTC_ENABLE

#define RTC_TIMER

#ifdef BOARDPROTO
#define SQW_PIN 0
#else
#define SQW_PIN 2
#endif
#endif


// enable an MPU9150 9-dof IMU.
#define MPU9150_ENABLE 
#define MPU9150_MAG_ENABLE
#define MPU9150_MAG_MIN_INTERVAL_US 100000

#ifdef BOARDPROTO
#define PIEZO_PIN 20
#else
#define PIEZO_PIN 3
#endif

// if defined, BT2S is the serial object, and the baud rate
// must be pre-configured on the module
// Serial3 is the one led out to the expansion pads
#define BT2S Serial3
#define BT2S_CONTROL_PIN 22
#define BT2S_BAUD 115200

#define STREAM_START_LINE "$"
#define MONITOR_START_LINE "#"

#endif // __FREEBIRD_CONFIG_H__
