#include "freebird_config.h"
#include <Arduino.h>
#include <i2c_t3.h>

#include "serialmux.h"

//#ifdef MPU9150_ENABLE
#include "I2Cdev.h"
#include "MPU6050.h"
//#endif

#include <Adafruit_ADS1015_t3.h>

#include <math.h>

#include "logger.h"
#include "SdFunctions.h"
#include <SdFatUtil.h>

#define myWIRE Wire

void timerIsr(void);

#ifdef RTC_TIMER
// how RTC pulses related to sampling events:
volatile uint32_t pulse_count_start;
volatile uint32_t pulse_count;

void pulse_isr(void) {
  logger.rtc_pulse_isr();
}

void Logger::rtc_pulse_isr(void) {
  // update teensy time
  rtc_pulse_count++;
  if( rtc_pulse_count >= rtc_freq ) { 
    rtc_pulse_count-=rtc_freq;
    unixtime++;
  }

  // see if sampling should be triggered
  if( pulse_count_start>0 ) {
    if(pulse_count==0) {
      pulse_count=pulse_count_start;
      timer_interrupt();
    }
    pulse_count--;
  }
}

#else
IntervalTimer Timer3;
#endif


// ADR->GND
Adafruit_ADS1115 ads1115(0x48);	// construct an ads1115 at address 0x48


#define CLEARINPUT while( mySerial.available() ) mySerial.read()

void datetime_println(Print &out, DateTime const &dt) {
  out.print(dt.year(), DEC);
  out.print('-');
  if( dt.month() < 10 ) out.print("0");
  out.print(dt.month(), DEC);
  out.print('-');
  if( dt.day() < 10 ) out.print("0");
  out.print(dt.day(), DEC);
  out.print(' ');
  if( dt.hour() < 10 ) out.print("0");
  out.print(dt.hour(), DEC);
  out.print(':');
  if( dt.minute() < 10 ) out.print("0");
  out.print(dt.minute(), DEC);
  out.print(':');
  if( dt.second() < 10 ) out.print("0");
  out.println(dt.second(), DEC);
}


//***** ADS1115  ******
/// purely asynchronous version
uint16_t adc_config; // encodes the input multiplexer, speed, etc.
volatile uint16_t adc_result; // ISR places result here
unsigned long t_start,t_elapsed;

void request_result(void);
void read_result(void);
void save_result(void);
void trigger_conversion(void);
void mark_completion(void);

#ifdef MPU9150_ENABLE
volatile int16_t ax, ay, az;
volatile int16_t gx, gy, gz;

void mpu9150_request_result(void);
void mpu9150_read_result(void);
void mpu9150_save_result(void);

#ifdef MPU9150_MAG_ENABLE
volatile int16_t mx,my,mz;
int16_t mag_count=0;
// max. sample rate for magnetometer is 100Hz, but for power reasons
// how about 10Hz.  if the base sampling is 500Hz, then divider is 50.
// this should get set dynamically in sample_setup() below.
int32_t mag_divider=50;
void mpu9150_mag_save_result(void);
void mpu9150_mag_save_and_trigger(void);
void mpu9150_mag_read_result(void);
void mpu9150_mag_trigger(void);
void mpu9150_mag_oneshot(void);

#endif
#endif


void request_result(void) {
  Wire.onFinish(read_result);
  Wire.beginTransmission(ads1115.m_i2cAddress);
  Wire.write(ADS1015_REG_POINTER_CONVERT);
  Wire.sendTransmission(I2C_STOP);
}

void read_result(void) {
  Wire.onFinish(save_result);
  Wire.sendRequest(ads1115.m_i2cAddress,(uint8_t)2,I2C_STOP);
}

void save_result(void) {  
  // these are non-blocking, just copying from the buffer
  adc_result = ((Wire.read() << 8) | Wire.read());  
  trigger_conversion();
}

void trigger_conversion(void) {
#ifdef MPU9150_ENABLE
  // even if we're not logging the IMU, may need
  // to query it for bluetooth triggering
  if ( logger.query_imu() ) {
    Wire.onFinish(mpu9150_request_result);
  } else {
    Wire.onFinish(mark_completion);
  }
#else
  Wire.onFinish(mark_completion);
#endif

  Wire.beginTransmission(ads1115.m_i2cAddress);
  Wire.write((uint8_t)ADS1015_REG_POINTER_CONFIG);
  Wire.write((uint8_t)(adc_config>>8));
  Wire.write((uint8_t)(adc_config & 0xFF));
  Wire.sendTransmission(I2C_STOP);
}


#ifdef MPU9150_ENABLE
void mpu9150_request_result(void) {
  Wire.onFinish(mpu9150_read_result);

  Wire.beginTransmission(logger.accelgyro.devAddr);
  Wire.write(MPU6050_RA_ACCEL_XOUT_H);
  Wire.sendTransmission(I2C_STOP);
}

void mpu9150_read_result(void) {
  Wire.onFinish(mpu9150_save_result);

  Wire.sendRequest(logger.accelgyro.devAddr, 14, I2C_STOP);
}

void mpu9150_save_result(void) {
  // all immediate
  ax = (((int16_t)Wire.read()) << 8) | Wire.read();
  ay = (((int16_t)Wire.read()) << 8) | Wire.read();
  az = (((int16_t)Wire.read()) << 8) | Wire.read();
  Wire.read(); Wire.read(); // unused
  gx = (((int16_t)Wire.read()) << 8) | Wire.read();
  gy = (((int16_t)Wire.read()) << 8) | Wire.read();
  gz = (((int16_t)Wire.read()) << 8) | Wire.read();
  
#ifdef MPU9150_MAG_ENABLE
  mag_count++; 
  if (mag_count>=mag_divider) {
    mag_count=0;
    Wire.onFinish(mpu9150_mag_read_result);
    // request result:
    Wire.beginTransmission(MPU9150_RA_MAG_ADDRESS);
    Wire.write(MPU9150_RA_MAG_XOUT_L);
    Wire.sendTransmission(I2C_STOP);
    return;
  }
#endif
  mark_completion();
}
#endif

#ifdef MPU9150_MAG_ENABLE
void mpu9150_mag_read_result(void) {
  Wire.onFinish(mpu9150_mag_save_and_trigger);
  Wire.sendRequest(MPU9150_RA_MAG_ADDRESS,(uint8_t)6,I2C_STOP);
}
void mpu9150_mag_save_result(void){
  // N.B. This is called during the sample loop, but also 
  // during command mode to see if bluetooth should be
  // activated.

  // apparently the magnetometer has reverse endianness from 
  // the accelerometer/gyro
  mx = ((int16_t)Wire.read()) | (((int16_t)Wire.read()) << 8);
  my = ((int16_t)Wire.read()) | (((int16_t)Wire.read()) << 8);
  mz = ((int16_t)Wire.read()) | (((int16_t)Wire.read()) << 8);		
}
void mpu9150_mag_save_and_trigger(void) {
  mpu9150_mag_save_result();
  // also triggers the next conversion of the magnetometer
  Wire.onFinish(mark_completion);
  mpu9150_mag_trigger();
}

void mpu9150_mag_trigger(void) {
  Wire.beginTransmission(MPU9150_RA_MAG_ADDRESS);
  Wire.write(0x0A); // control register
  Wire.write(0x01); // enable one-shot magnetometer mode
  Wire.sendTransmission(I2C_STOP);
}

// not part of the regular sampling loop - just for command
// mode, synchronous access to the magnetometer.
void mpu9150_mag_oneshot(void)
{
  // blocking, one-shot read of the magnetometer.
  // note - this is slow! 10+ms
  Wire.beginTransmission(MPU9150_RA_MAG_ADDRESS);
  Wire.write(0x0A); // control register
  Wire.write(0x01); // enable one-shot magnetometer mode
  if ( Wire.endTransmission(I2C_STOP) )  { // blocking
    mySerial.println("Problem with mag_oneshot");
  }

  delay(10); // takes about 10ms for a magneto conversion

  // then request the result
  Wire.beginTransmission(MPU9150_RA_MAG_ADDRESS);
  Wire.write(MPU9150_RA_MAG_XOUT_L);
  Wire.endTransmission(I2C_STOP);

  if ( Wire.requestFrom(MPU9150_RA_MAG_ADDRESS,(uint8_t)6)!=6 ) {
    mySerial.println("Failed to receive data");
  }
  mpu9150_mag_save_result();
}

#endif // MPU9150_MAG_ENABLE


// Call this last, to maintain an estimate of the duty-cycle of the 
// asynchronous processing.
void mark_completion(void) {
  Wire.onFinish(NULL);
  t_elapsed = micros() - t_start;
}


uint16_t my_analogRead_async(void){
  uint16_t last_result=adc_result;

  adc_result=37; // dummy init value
  t_start=micros();

  if ( logger.log_adc ) {
    request_result();
  }
#ifdef MPU9150_ENABLE
  else if ( logger.query_imu() ) {
    mpu9150_request_result();
  }
#endif

  return last_result;
}
//***** END ADS1115 ******



void Logger::setup(void) {
  mode = MODE_BOOT;
  request_mode = MODE_COMMAND; 

  tone(PIEZO_PIN,1000);
  delay(100);
  noTone(PIEZO_PIN);
  delay(50);
  tone(PIEZO_PIN,1500);
  delay(100);
  noTone(PIEZO_PIN);

#ifdef BT2S_CONTROL_PIN
  pinMode(BT2S_CONTROL_PIN,OUTPUT);
  bluetooth_set_state(1);
#endif

  myWIRE.begin(I2C_MASTER, 0x00, I2C_PINS_18_19, I2C_PULLUP_EXT, I2C_RATE_400);

  ads1115.begin(); 
  ads1115.setGain(GAIN_ONE);
  // this will request double-end channels 0/1, 860SPS, one-shot conversions
  adc_config=ads1115.conversion_config(0,0);

#ifdef MPU9150_ENABLE
  accelgyro.initialize();
#ifdef MPU9150_MAG_ENABLE
  // should be able to do this exactly once, and it will stay in passthrough mode.
  Wire.beginTransmission(accelgyro.devAddr);
  Wire.write(MPU6050_RA_INT_PIN_CFG);
  Wire.write(0x02); //set i2c bypass enable pin to true to access magnetometer
  Wire.endTransmission();

#endif

#endif

#ifdef RTC_ENABLE
  rtc.begin();

  if (! rtc.isrunning()) {
    mySerial.println("RTC is NOT running!");
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(__DATE__, __TIME__));
  }
  pinMode(SQW_PIN,INPUT);
#endif

  // initializes SPI bus and SD card library.
  store.begin();

  mySerial.println("About to activate " CMDFILE);
  activate_cmd_file(CMDFILE); 
}


bool Logger::activate_cmd_file(const char *fname) {
  // Check if file exists
  SdBaseFile file;
  if ( file.open(fname,O_READ) ) {
    mySerial.print("Found cmd file ");
    mySerial.println(fname);
    strcpy(cmd_filename,fname);
    cmd_file_pos=0;
    return true;
  } else {
    mySerial.print("Failed to activate command file ");
    mySerial.println(fname);
    
    cmd_filename[0]='\0';
    cmd_file_pos=0;
    return false;
  }
}


void Logger::loop(void) {
  if ( request_mode != mode ) {
    if( mode==MODE_SAMPLE )
      sample_cleanup();
    else if (mode==MODE_COMMAND) 
      command_cleanup();
    mode = request_mode;
    if( mode==MODE_SAMPLE )
      sample_setup();
    else if( mode==MODE_COMMAND )
      command_setup();
  }
  if( mode == MODE_SAMPLE ) 
    sample_loop();
  else // mode == MODE_COMMAND
    command_loop();
}




void timerIsr(void) {
  logger.timer_interrupt();
}

void Logger::start_frame(void){
  frame_pos=0;
}

void Logger::end_frame(void){
  store.frame_bytes=frame_pos;
  store.store_frame(frame);
  frame_pos=0; // redundant with start, but safer.
}

uint16_t last_measurement; 
void Logger::timer_interrupt(void) {
  // testing hack to difference the readings between
  // a pseudo aref and the signal.
  uint16_t adSig; // adRef

  adSig = my_analogRead_async();

  // this is where the filtering was.

  last_measurement=adSig;
  
  if(storage_counter==0) {
    storage_counter = storage_interval;
    start_frame();

    if( log_adc ){
      store_sample(adSig);
    }
    
    if( track_variance ){
      N_accum++;
      mean_accum += adSig;
      var_accum += double(adSig)*double(adSig);
      if( adSig<min_reading ) min_reading=adSig;
      if( adSig>max_reading ) max_reading=adSig;
    }
#ifdef MPU9150_ENABLE
    if( log_imu ) {
      // accelerations are signed 16-bit - 
      store_sample((uint16_t)ax);
      store_sample((uint16_t)ay);
      store_sample((uint16_t)az);
      store_sample((uint16_t)gx);
      store_sample((uint16_t)gy);
      store_sample((uint16_t)gz);
#ifdef MPU9150_MAG_ENABLE
      store_sample((uint16_t)mx);
      store_sample((uint16_t)my);
      store_sample((uint16_t)mz);
#endif // MPU9150_MAG_ENABLE
    }
#endif
    end_frame();
  }
  storage_counter--;
}

void Logger::store_sample(uint16_t sample) { 
  // little endian, to be consistent with the default ARM
  // architecture with gcc.
  frame[frame_pos++] = (uint8_t)(sample&0xFF);
  frame[frame_pos++] = (uint8_t)(sample>>8);
}

void Logger::sample_setup(void) {
  mySerial.println("Changing to sample mode");

  tone(20,1500);
  delay(100);
  noTone(20);
  delay(50);
  tone(20,1000);
  delay(100);
  noTone(20);

#ifdef MPU9150_MAG_ENABLE
  mpu9150_mag_trigger();
  delay(10);

  if( adc_interval_us < MPU9150_MAG_MIN_INTERVAL_US ) {
    mag_divider=MPU9150_MAG_MIN_INTERVAL_US/adc_interval_us;
  } else {
    mag_divider=1;
  }
#endif

  CLEARINPUT;

#ifdef RTC_TIMER
  // setup the clock before initializing store, so that the
  // file gets a good timestamp:

  // sync the teensy time to the RTC
  delay_until_rtc_transition();
  unixtime=rtc.now().unixtime();
  rtc_pulse_count=0;
  // start counting time but don't start logging samples yet
  // since it may take some time to open the file.  Maybe not 
  // an issue since we have a lot of buffer room, but this 
  // is likely one of the worst moments, so don't push it.
  rtc.SQWFrequency(DS3231_SQW_FREQ_1024);
  rtc.SQWEnable(1);
  pulse_count=0;
  pulse_count_start=0; //hopefully it was already zero...
  attachInterrupt(SQW_PIN,pulse_isr,FALLING);
#endif

  store.frame_bytes=calc_frame_bytes();
  store.setup();

  system_info(store);

  if(track_variance) {
    N_accum=0;
    mean_accum = 0;
    var_accum = 0;
    min_reading=65535;
    max_reading=0;
  }

#ifdef RTC_TIMER
  // now, it's okay to start triggering sampling events
  pulse_count_start=(adc_interval_us*rtc_freq)/1000000;
#else

#ifdef TEENSY3
  Timer3.begin(timerIsr,adc_interval_us);
#endif
#ifdef DUE
  Timer3.attachInterrupt( timerIsr );
  Timer3.start(adc_interval_us); // us
#endif
#endif
}



/**
   This is where the meat is.  
   Loops until either a predefined interval has passed, the card is full,
   or serial input is detected.
   The looping is handled here - so typically sample_loop is called only once,
   and only exists when transitioning out of sample mode.  The only thing that
   gets skipped is an Arduino SerialEventRun check, which we can do more directly
   right here.
 */ 
void Logger::sample_loop(void) {
  // record the last time/sample count reported
  uint32_t beep_timestamp,status_timestamp;
  const uint32_t status_interval=1000;
  uint32_t now_millis;
  uint32_t status_count=0;
  storage_counter = 0;
  beep_timestamp=status_timestamp=millis();
  beep_timestamp+=beep_interval;
  status_timestamp+=status_interval;

  while(request_mode==mode) {
    now_millis=millis();

    if ( (beep_interval>0) && (now_millis > beep_timestamp) ) {
      beep_timestamp = now_millis+beep_interval;
      tone(PIEZO_PIN,4000,80);
    }

#ifdef BT2S_CONTROL_PIN
    bluetooth_poll_status();
#endif

    // only report sampling progress every 1s
    if ( sample_monitor && (now_millis > status_timestamp) ) {
      mySerial.print(MONITOR_START_LINE "Read ");
      mySerial.print(store.frame_count - status_count);
      
      mySerial.print(" samples in ");
      mySerial.print(now_millis - status_timestamp + status_interval);
      mySerial.println("ms");

      status_timestamp = now_millis + 1000; // schedule another report in a second
      status_count=store.frame_count;
      mySerial.print(MONITOR_START_LINE);
      mySerial.print(store.overruns);
      mySerial.print(" overruns   " " usec=");
      mySerial.print(t_elapsed);
      mySerial.print(" signal=");
      mySerial.println(last_measurement);

#ifdef BT2S
      mySerial.print(MONITOR_START_LINE "USB connected ");
      mySerial.println(Serial.dtr());
#endif

      if(track_variance) {
        noInterrupts();
        uint32_t local_mean = mean_accum;
        uint32_t local_N=N_accum;
        double local_var = var_accum;
        uint32_t local_max=max_reading;
        uint32_t local_min=min_reading;

        // and reset
        N_accum=0;
        mean_accum = 0;
        var_accum = 0;
        min_reading=65535;
        max_reading=0;
        interrupts();

        float mean = float(local_mean) / local_N;
        local_var = local_var / local_N - mean*mean;
        mySerial.print(MONITOR_START_LINE "Mean, std.dev: ");
        mySerial.print(mean);
        mySerial.print(" ");
        mySerial.println(sqrt(local_var));
        mySerial.print(MONITOR_START_LINE "Absolute spread: ");
        mySerial.println(local_max-local_min);
        mySerial.print(MONITOR_START_LINE "Converted mS/cm: ");
        mySerial.println(counts_to_mS_cm(mean));
      }

#ifdef MPU9150_ENABLE
      if( log_imu ) {
        mySerial.print(MONITOR_START_LINE "ax: ");
        mySerial.print(ax);
#ifdef MPU9150_MAG_ENABLE
        mySerial.print("  mx: ");
        mySerial.println(mx);
#else
        mySerial.println("");
#endif // MPU9150_MAG_ENABLE
      }
#endif // MPU9150_ENABLE
    }

    // the actual output part:
    store.loop();
    
    if( mySerial.available() ) {
      uint8_t c=mySerial.read();
      // stop it when an exclamation or ESC is read
      if ( (c=='!') || (c==27) ) {
        request_mode=MODE_COMMAND;
      } else {
        mySerial.println(MONITOR_START_LINE "Use ! or ESC to stop");
      }
    }
  }
  noTone(PIEZO_PIN);
}

void Logger::sample_cleanup(void) 
{ 
  mySerial.println("Exiting sample mode");
#ifdef RTC_TIMER
  detachInterrupt(SQW_PIN);
  pulse_count_start=0;
#else
#ifdef TEENSY3
  Timer3.end();
#endif
#ifdef DUE
  Timer3.detachInterrupt();
#endif
#endif

  // DBG: Write another header section at the end to help
  // debug clock issues
  system_info(store);

  store.cleanup();
}


uint8_t Logger::confirm(void) {
  mySerial.println("Are you sure you want to proceed? Type yes to confirm");
  get_next_command("confirm>\n");
  return strcmp(cmd,"yes")==0;
}

void Logger::command_loop(void) {
  get_next_command("freebird>\r\n");

  if(strcmp(cmd,"erase")==0) {
    if( confirm() )
      store.format('E');
  } else if ( strcmp(cmd,"format")==0 ) { 
    if( confirm() )
      store.format('F'); 
  } else if ( strcmp(cmd,"quickformat")==0 ) {
    if( confirm() )
      store.format('Q');
  } else if ( strcmp(cmd,"sample")==0) {
    request_mode = MODE_SAMPLE; 
  } else if ( strcmp(cmd,"datetime")==0) {
    if ( cmd_arg ) {
      set_datetime(cmd_arg);
    } else {
      rtc_sync_time();
    }
  } else if ( strcmp(cmd,"!")==0 ) {
    // do nothing - this is just to get to command mode.
  } else if ( strcmp(cmd,"info")==0) {
    system_info(); 
  } else if ( strcmp(cmd,"help")==0 ) {
    help();
  } 
#ifdef MPU9150_ENABLE
  else if ( strcmp(cmd,"imu_status")==0 ) {
    imu_status();
  }
#endif
  else if ( strcmp(cmd,"ls")==0) {
    cmd_ls();
  } else if ( strcmp(cmd,"sample_monitor")==0 ) {
    // use "sample_monitor=0" to disable printing sampling status
    sample_monitor=(cmd_arg[0]!='0');
  } else if ( strcmp(cmd,"log_to_serial")==0 ) {
    store.log_to_serial=(cmd_arg[0]!='0');
  } else if ( strcmp(cmd,"sample_interval_us")==0 ) {
    adc_interval_us=(uint32_t)atoi(cmd_arg);
  } else if ( strcmp(cmd,"beep_interval_ms")==0 ) {
    beep_interval=atoi(cmd_arg);
  } else if ( strcmp(cmd,"log_adc")==0 ) {
    log_adc=(cmd_arg[0]!='0');
  } else if ( strcmp(cmd,"label")==0 ) {
    strncpy(label,cmd_arg,LABEL_BUFFLEN-1);
  }
#ifdef BT2S_CONTROL_PIN
  else if ( strcmp(cmd,"bluetooth_mode")==0 ) {
    bluetooth_mode=(wireless_mode)atoi(cmd_arg);
    bluetooth_poll_status();
  }
#endif
  else if ( strcmp(cmd,"softboot")==0 ) {
    // ugly...
    // from http://forum.pjrc.com/threads/24304-_reboot_Teensyduino()-vs-_restart_Teensyduino()
    // but adapted to use registers from mk20dx128.h
    // not sure what the exact meaning of the bits are.
    // note that this DOES reset USB.
    SCB_AIRCR = 0x5FA0004;
    delay(100); // restart has a small lag
    // never gets here
  }
#ifdef MPU9150_ENABLE
  else if ( strcmp(cmd,"log_imu")==0 ) {
    log_imu=(cmd_arg[0]!='0');
  }
#endif
  else if ( strcmp(cmd,"send_file")==0 ) {
    // for the moment, no support for sending just part of a file...
    send_file(cmd_arg);
  }
  else {
    mySerial.println("\nUNKNOWN_COMMAND");
    mySerial.println(cmd);
  }
}

void Logger::get_next_command(const char *prompt) {
  /*  if processing from serial, show prompt and then wait for a complete
   *  line of input.
   */
  bool use_serial=(!cmd_filename[0]);
  SdFile file;

  uint8_t pos=0;
  int16_t tmp;

  if( !use_serial ) {
    if ( !file.open(cmd_filename,O_READ) ) {
      mySerial.print("Failed to open cmd file");
      mySerial.println(cmd_filename);
      use_serial=true;
    } else {
      file.seekSet(cmd_file_pos);
    }
  }
  
  while( pos==0 ) {
    if( use_serial ) {
      CLEARINPUT;
      mySerial.print(prompt);
    }

    cmd_arg=NULL;
    
    while(pos<CMD_BUFFLEN-1) {
      if ( use_serial ) {
        while(!mySerial.available()){
#ifdef BT2S_CONTROL_PIN
          bluetooth_poll_status(1);
#endif
        }
        cmd[pos] = mySerial.read(); 
      } else {
        tmp=file.read();
        if( tmp<0 ) {
          // error or end of file.
          cmd_filename[0]='\0';
          pos=0; // signal restart with prompt
          use_serial=true;
          // would be nice to announce this, but at least in the case of
          // sample, this gets printed way after the fact, and is just
          // confusing.
          // mySerial.println("hit EOF");
          break;
        } else {
          cmd[pos] = (uint8_t)tmp;
        }
      }

      // handle backspace
      if( cmd[pos] == 8 ) {
        mySerial.print(" CANCEL");
        pos = 0; // signal restart
        break;
      } else {
        if( cmd[pos] == '\r' or cmd[pos] == '\n' ) {
          // a little tricky, since it's possible to get cr, cr/lf, or lf
          // newlines.  This could be left over from a previous line.
          // for console use, better to print the prompt again so that
          // on connect, you can just hit enter a few times to make sure
          // there is a connection.
          if( use_serial )
            mySerial.println();
          cmd[pos] = '\0'; // null terminate it
          break;
        }
        if( use_serial )
          mySerial.print(cmd[pos]); // echo back to user

        // special handling for separate =
        if( cmd[pos]=='=' ) {
          cmd[pos] = '\0';
          cmd_arg=cmd+pos+1;
        }
        pos++;
        if(pos==CMD_BUFFLEN) { // protect overrun:
          mySerial.print(" TOO_LONG");
          pos = 0; // signal restart
          break;
        }
      }
    }
  }
  if( !use_serial ) 
    cmd_file_pos=file.curPosition();
}

// write system details to mySerial.
void Logger::system_info(void) {
  system_info(mySerial);
}

void print_hex(Print &out,uint32_t v) {
  // Teensy is little-endian
  for(uint8_t nibble=0;nibble<8;nibble++)
    out.print( (v&(0xF<<(4*nibble))) >> (4*nibble), HEX );
}

void Logger::system_info(Print &out) {
  out.println("freebird_version: " FREEBIRD_VERSION_STR);
  out.print("teensy_uid: 0x");
  // Looks like the interesting part is the low 64 bits.
  //print_hex(out,SIM_UIDH);
  //out.print(" ");
  //print_hex(out,SIM_UIDMH);
  //out.print(" ");
  print_hex(out,SIM_UIDML);
  print_hex(out,SIM_UIDL);
  out.println("");

  out.print("label: ");
  out.println(label);
  out.print("free_ram: ");
  out.println(FreeRam());
  out.print("sample_interval_us: ");
  out.println(adc_interval_us);
  out.print("storage_interval_div: ");
  out.println(storage_interval);
  out.print("beep_interval_ms: ");
  out.println(beep_interval);
  out.print("log_adc: ");
  out.println(log_adc);

  frame_info(out);

  out.print("log_to_serial: ");
  out.println(store.log_to_serial);

#ifdef MPU9150_MAG_ENABLE
  out.print("log_imu: ");
  out.println(log_imu);
  out.print("magnetometer_interval_div: ");
  out.println(mag_divider);
#endif

#ifdef BT2S_CONTROL_PIN
  out.print("bluetooth_mode: ");
  out.println(bluetooth_mode);
  out.print("bluetooth_state: ");
  out.println(bluetooth_state);
#endif

  if ( storage_interval>1 ) {
    filter.filter_info(out);
  } else {
    out.println("filter: disabled since storage interval is 1");
  }
  rtc_info(out);
}


uint16_t Logger::calc_frame_bytes(void){ 
  uint16_t frame_bytes=0;
  if( log_adc ) frame_bytes+=2;
#ifdef MPU9150_ENABLE 
  if( log_imu ) {
#ifdef MPU9150_MAG_ENABLE
    frame_bytes+=18;
#else
    frame_bytes+=12;
#endif
  }
#endif
  return frame_bytes;
}

void Logger::frame_info(Print &out) {
  out.print("frame_bytes: ");
  out.println(calc_frame_bytes());
  out.print("frame_format: [");

  if( log_adc ) {
    out.print("('counts','<i2'),");
  }
#ifdef MPU9150_ENABLE
  if( log_imu ) {
    out.print("('imu_a','<i2',3),");
    out.print("('imu_g','<i2',3),");
#ifdef MPU9150_MAG_ENABLE
    out.print("('imu_m','<i2',3),");
#endif
  }
#endif
  out.println("]");
}

void Logger::rtc_info(Print &out){
  uint32_t count;

#ifndef RTC_ENABLE
  out.println("rtc_status: disabled at compile time");
#else
  out.print("rtc_status: ");
  out.println(rtc.isrunning());
  out.print("rtc_temp: ");
  out.println(rtc.getTempAsFloat());				
  out.print("rtc_time: ");
  datetime_println(out,rtc.now());

#ifdef RTC_TIMER
  out.print("rtc_timer_freq_hz: ");
  out.println(rtc_freq);
  out.print("ticks_per_second: ");
  out.println(rtc_freq);
  out.print("sample_rate_hz: ");
  count=(adc_interval_us*rtc_freq)/1000000;
  out.println(float(rtc_freq)/float(count));
#else
  // should fix this - it should be consistent with how 
  // store gets subsecond timestamp info.
  out.println("ticks_per_second: nan");
  out.println("sample_rate_hz: ");
  out.println(float(1000000)/float(adc_interval_us));
#endif
#endif
}

void Logger::rtc_sync_time(void) {
  mySerial.print("datetime: ");
  // to get sub-second precision, wait for the RTC to transition
  // to the next digit.
  delay_until_rtc_transition();
  datetime_println(mySerial,rtc.now());
}

// stolen from RTClib.cpp
static uint16_t conv_digits(const char* p,int digits)
{
  uint16_t v = 0;
  for(int digit=0;digit<digits;digit++) {
    v*=10;
    if ('0' <= *p && *p <= '9') 
      v += *p - '0';
    p++;
  }
  return v;
}

void Logger::set_datetime(const char *str) {
  // format is YYYY-MM-DDTHH:MM:SS
  if( strlen(str) != strlen("YYYY-MM-DD HH:MM:SS") ) {
    mySerial.println("Error: format is YYYY-MM-DD HH:MM:SS");
    return;
  }
  
  DateTime dt(conv_digits(str,4),
              conv_digits(str+5,2),
              conv_digits(str+8,2),
              conv_digits(str+11,2),
              conv_digits(str+14,2),
              conv_digits(str+17,2));
#ifndef RTC_ENABLE
  mySerial.println("RTC was disabled at compile");
#else
  rtc.adjust(dt);
#endif
}

void Logger::help(void){
  mySerial.println("help  --> show this text");
  mySerial.println("info  --> print system info");
  mySerial.println("datetime=YYYY-MM-DD hh:mm:ss  --> set persistent system clock");
  mySerial.println("datetime  --> get persistent system clock with sub-second accuracy");
  mySerial.println("sample   --> begin sampling data, exit with any keypress");
  mySerial.println("format   --> erase and format the SD card");
  mySerial.println("quickformat  --> format the SD card");
  mySerial.println("bluetooth_mode={0,1,2} --> OFF, ON, MAGNETO trigger");
  mySerial.println("ls --> print list of files in root directory of SD card");
  mySerial.println("sample_monitor={1,0} --> enable/disable 1Hz data monitor to serial");
  mySerial.println("beep_interval_ms=N --> set beep timing in milliseconds");
  mySerial.println("log_to_serial={1,0} --> enable/disable logging hex data to serial instead of SD");
  mySerial.println("sample_interval_us=NNNN --> set sampling interval in microseconds");
  mySerial.println("softboot --> trigger a software reset");
#ifdef MPU9150_ENABLE
  mySerial.println("imu_status --> sample the IMU for one step and display results");
#endif
}

#ifdef MPU9150_ENABLE
void Logger::imu_status(void) {
  Wire.onFinish(NULL);
  
  CLEARINPUT;

  while ( 1 ) {
    if( mySerial.available() ) {
      break;
    }

    Wire.beginTransmission(logger.accelgyro.devAddr);
    Wire.write(MPU6050_RA_ACCEL_XOUT_H);
    Wire.endTransmission();
    
    Wire.requestFrom((uint8_t)logger.accelgyro.devAddr, (size_t)14);
    
    ax = (((int16_t)Wire.read()) << 8) | Wire.read();
    ay = (((int16_t)Wire.read()) << 8) | Wire.read();
    az = (((int16_t)Wire.read()) << 8) | Wire.read();
    Wire.read(); Wire.read(); // unused
    gx = (((int16_t)Wire.read()) << 8) | Wire.read();
    gy = (((int16_t)Wire.read()) << 8) | Wire.read();
    gz = (((int16_t)Wire.read()) << 8) | Wire.read();
    
#ifdef MPU9150_MAG_ENABLE
    Wire.beginTransmission(MPU9150_RA_MAG_ADDRESS);
    Wire.write(0x0A); // control register
    Wire.write(0x01); // enable one-shot magnetometer mode
    Wire.sendTransmission(I2C_STOP);
    delay(20); // allow time for conversion
    
    Wire.beginTransmission(MPU9150_RA_MAG_ADDRESS);
    Wire.write(MPU9150_RA_MAG_XOUT_L);
    Wire.sendTransmission(I2C_STOP);
    while( !Wire.done() );
    
    // Wire.requestFrom(MPU9150_RA_MAG_ADDRESS,(uint8_t)6);
    Wire.sendRequest(MPU9150_RA_MAG_ADDRESS,(uint8_t)6,I2C_STOP);
    while( !Wire.done() );

    // opp. endianness from accel/gyro
    mx = (((int16_t)Wire.read())) | (((int16_t)Wire.read()) << 8);
    my = (((int16_t)Wire.read())) | (((int16_t)Wire.read()) << 8);
    mz = (((int16_t)Wire.read())) | (((int16_t)Wire.read()) << 8);		
#endif
    
    // display tab-separated accel/gyro x/y/z values
    mySerial.print("a/g/m:\t");
    mySerial.print(ax); mySerial.print("\t");
    mySerial.print(ay); mySerial.print("\t");
    mySerial.print(az); mySerial.print("\t");
    mySerial.print(gx); mySerial.print("\t");
    mySerial.print(gy); mySerial.print("\t");
    mySerial.print(gz); mySerial.print("\t");
#ifdef MPU9150_MAG_ENABLE
    mySerial.print(mx); mySerial.print("\t");
    mySerial.print(my); mySerial.print("\t");
    mySerial.print(mz);
#endif
    mySerial.println("");
  }
}
#endif

void Logger::cmd_ls(void) {
  sd.ls(&mySerial,LS_SIZE);
}

#ifdef RTC_ENABLE
void Logger::delay_until_rtc_transition(void) {
  rtc.SQWFrequency(DS3231_SQW_FREQ_1);
  rtc.SQWEnable(1);
  // transition of the seconds digit is on falling edge 
  // of SQW.

  // wait for it to be high:
  while( !digitalRead(SQW_PIN) ) ;
  delay(10); // anti-jitter
  while( digitalRead(SQW_PIN) );
}
#endif

#ifdef BT2S
void Logger::bluetooth_poll_status(uint8_t query_magnetometer)
{
  // Check for trigger events to turn bluetooth on, or
  // if it has timed out and should be turned off.
  // query_magnetometer: if true, it is up to this function to 
  // update the mx,my,mz samples.

  // Assumes that mx,my,mz are reasonably up to date.

  uint8_t bt_mag_enable=0; // should it be on based on magnetometer?
  uint8_t bt_idle=0; // has it been idle and should be powered down?

  if ( bluetooth_mode == COMM_ON ) {
    if ( !bluetooth_state ) 
      bluetooth_set_state(1);
    return;
  } else if ( bluetooth_mode == COMM_OFF ) {
    if ( bluetooth_state ) 
      bluetooth_set_state(0);
    return;
  } else { // bluetooth_mode==COMM_MAGNETO
#ifdef MPU9150_MAG_ENABLE
    if ( query_magnetometer ) {
      mpu9150_mag_oneshot();
    } 
   
    bt_mag_enable= (abs(mx)>bluetooth_mag_threshold) ||
      (abs(my)>bluetooth_mag_threshold) ||
      (abs(mz)>bluetooth_mag_threshold) ;
#endif
    uint32_t idle_ms=(millis()-mySerial.last_activity_millis);
    bt_idle=idle_ms/1000 > bluetooth_idle_s;

    if ( bluetooth_state && (!bt_mag_enable) && bt_idle ) {
      bluetooth_set_state(0);
    } else if ( !bluetooth_state && bt_mag_enable ) {
      bluetooth_set_state(1);
      mySerial.last_activity_millis=millis();
      // hope that this doesn't disrupt sampling...
      tone(PIEZO_PIN,500,200);
    }
    return;
  }
}

void Logger::bluetooth_set_state(uint8_t state) {
  bluetooth_state=state;
  digitalWrite(BT2S_CONTROL_PIN,state);
}
#endif

void Logger::send_file(char *cmd_arg){
  // cmd_arg:  filename
  //           filename,start,bytes
  // start defaults to 0, bytes defaults to size of the file.
  // for now, it just sends the whole file.
  char *start_ptr=NULL,*bytes_ptr=NULL;
  long start,bytes;

  // for(int c=0;c<strlen(cmd_arg);c++){
  //   if( cmd_arg[c] == ',') {
  //     cmd_arg[c]=0;
  //   }
  // }
  store.send_data(cmd_arg);
}
