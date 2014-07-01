#include <RTC_DS3231.h>
#include <RTClib.h>

//#ifdef MPU9150_ENABLE
#include "I2Cdev.h"
#include "MPU6050.h"
//#endif

#include "SdFunctions.h"
#include "filter.h"

#ifndef __LOGGER_H__
#define __LOGGER_H__

#define DS1307_ADDRESS 0x68

#define CMD_BUFFLEN 100
#define LABEL_BUFFLEN 100
#define MAX_FILENAME_LEN 256

#define MODE_BOOT 0
#define MODE_SAMPLE 1
#define MODE_COMMAND 2

#define MAX_FRAME_BYTES 20

typedef enum { COMM_OFF, COMM_ON, COMM_MAGNETO} wireless_mode;

class Logger {
private:
  char label[LABEL_BUFFLEN];
  char cmd[CMD_BUFFLEN];
  // when a command with an '=' is found, it's split there
  // with the second part pointed to by cmd_arg
  char *cmd_arg; 

  char cmd_filename[MAX_FILENAME_LEN];
  uint32_t cmd_file_pos;
  bool sample_monitor;

  uint8_t frame[MAX_FRAME_BYTES];
  uint8_t frame_pos;
#ifdef BT2S_CONTROL_PIN
  uint8_t bluetooth_state;
  wireless_mode bluetooth_mode;
  int16_t bluetooth_mag_threshold;
  uint16_t bluetooth_idle_s;
#endif
public:
  // Configuration parameters:
  uint32_t adc_interval_us;
  uint32_t storage_interval;
  
  bool log_adc;
#ifdef MPU9150_ENABLE
  bool log_imu;
  bool query_imu(void) { return log_imu || (bluetooth_mode==COMM_MAGNETO); }
#endif

  Logger(void)
#ifdef MPU9150_ENABLE
    : accelgyro(0x69)
#endif
  {
    // set some basic, sane settings
    // note that if the RTC is used for timing, the
    // adc interval may not be exactly as requested
    // since the RTC clock is 1024Hz.
    // adc_interval_us = 2000; // 500Hz
    adc_interval_us=2000; // 500Hz
    storage_interval = 1; // same as ADC
    beep_interval=5000;
    track_variance=true;
    sample_monitor=true;
    cmd_filename[0]='\0';
    log_adc=true;
#ifdef MPU9150_ENABLE
    log_imu=true;
#endif

#ifdef BT2S_CONTROL_PIN 
    bluetooth_mode=COMM_MAGNETO;
    bluetooth_mag_threshold=600;
    bluetooth_idle_s=60;
#endif // BT2S_CONTROL_PIN

    // label is initialized to 0 automatically
  }

  float counts_to_mS_cm(float counts) {
    // for testing, a steady state conversion (i.e. no deemphasis)

    //  // for squid sn105
    //  const float a=2.802,b=-112.1;
    //  const float Kp=0.00115; // Kprime for probe C171

    // for squid sn104
    const float a=2.812,b=-113.5;
    const float Kp=0.00108; // Kprime=K for probe C175

    // with unity gain, 0V => 0 counts, and 4.096V=>2^15
    // values over 2^15 are 2s-complement negative
    float Vx=(float)counts*4.096 / 32768;
    float Y=(Vx-a)/b;

    // conductance * cell constant * S/m to mS/cm
    return 10*Y/Kp;
  }

  // most code should just change request_mode, which
  // arranges for the next loop() call to exit the current
  // mode and setup the new mode.
  uint8_t request_mode,mode;
  volatile uint32_t storage_counter;
  uint32_t beep_interval; // how often beep while sampling
  
  void setup(void);

  void loop(void);

  void timer_interrupt(void);

  void sample_setup(void);
  void sample_loop(void);
  void sample_cleanup(void);

  void store_sample(uint16_t sample);
  void start_frame(void);
  void end_frame(void);

  void command_setup(void) {};
  void command_loop(void);
  void command_cleanup(void) {};

  // command mode support fns

  // if fname exists, it will be activated as the source for
  // subsequent calls to get_next_command(), until end of file is
  // reached.  returns true if file was found.
  bool activate_cmd_file(const char *fname);

  // if there is an active command file, return the next
  // command from it, otherwise, prompt for serial input.
  void get_next_command(const char *);
  uint8_t confirm(void);
  void help(void);
  void cmd_ls(void);

  void system_info(void);  // defaults to mySerial.
  void system_info(Print &); // write system details 
  void frame_info(Print &);
  void rtc_info(Print &);
  
  void send_file(char *);

  uint16_t calc_frame_bytes(void);

  void set_datetime(const char *);

  Storage store;

  Filter filter;

  //date tracking
  uint32_t unixtime;

#ifdef RTC_ENABLE
  RTC_DS3231 rtc;
  const uint32_t rtc_freq=1024;
  uint16_t rtc_pulse_count; // fraction of 1024

  void delay_until_rtc_transition(void);
  void rtc_sync_time(void);
  void rtc_pulse_isr(void);
  DateTime now(void) {return DateTime(unixtime);}
#endif

#ifdef MPU9150_ENABLE 
  MPU6050 accelgyro;
  void imu_status(void);
#endif

#ifdef BT2S_CONTROL_PIN 
  void bluetooth_poll_status(uint8_t query_magnetometer=0);
  void bluetooth_set_state(uint8_t);
#endif // BT2S_CONTROL_PIN

  // for testing mainly - currently tracks variance as 
  // samples are stored
  bool track_variance;
  uint32_t N_accum;
  uint32_t mean_accum;
  double var_accum;
  uint16_t min_reading,max_reading;
};

extern Logger logger;


#endif
