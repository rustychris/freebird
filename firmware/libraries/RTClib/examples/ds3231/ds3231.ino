// Test Functions for  DS3231 RTC LIBRARY connected via I2C and Wire lib
//   Excellent for use with the macetech.com Chronodot
//
//  INCLUDES base code with detailed comments for setting up Timer0 or Timer1 as input counters.
//   These input counters are useful for more accurate interrupt-based timekeeping.
//
//
// CREDITS:
// Base for code thanks to Jeelabs on the DS1307
// Modified by Red Byer 7/25/2013 for DS3231   www.redstoyland.com   Find code on github  under mizraith
// Released to public domain.  Buy me a beer if you like or use the code.

// CONNECTIONS:
// DS3231 SDA --> A4    Don't forget to pullup (4.7k to VCC)
// DS3231 SCL --> A5    Don't forget to pullup (4.7k to VCC)
// SQW --->  D5 / T1 (pick this or 32kHz to test)  Don't forget to pullup (4.7k to 10k to VCC)
// 32k --->  D5 / T1  Don't forget to pullup (4.7k to 10k to VCC)


#include <Wire.h>
#include <SPI.h>
#include <RTClib.h>
#include <RTC_DS3231.h>

RTC_DS3231 RTC;


// easier to reference here...see .h file for more options
//#define SQW_FREQ DS3231_SQW_FREQ_1      //  0b00000000  1Hz
#define SQW_FREQ DS3231_SQW_FREQ_1024     //0b00001000   1024Hz
//#define SQW_FREQ DS3231_SQW_FREQ_4096  // 0b00010000   4096Hz
//#define SQW_FREQ DS3231_SQW_FREQ_8192 //0b00011000      8192Hz

#define PWM_COUNT 1020   //determines how often the LED flips
#define LOOP_DELAY 5000 //ms delay time in loop

#define RTC_SQW_IN 5     // input square wave from RTC into T1 pin (D5)
                               //WE USE TIMER1 so that it does not interfere with Arduino delay() command
#define INT0_PIN   2     // INT0 pin for 32kHz testing?
#define LED_PIN    9     // random LED for testing...tie to ground through series resistor..
#define LED_ONBAORD 13   // Instead of hooking up an LED, the nano has an LED at pin 13.

//----------- GLOBALS  -------------------------

volatile long TOGGLE_COUNT = 0;


//####################################################################################
// INTERRUPT SERVICE ROUTINES
//####################################################################################

//ISR(TIMER0_COMPA_vect) {
  //digitalWrite(LED_PIN, !digitalRead(LED_PIN));      // ^ 1);
//}
    
ISR(TIMER1_COMPA_vect) {
  //digitalWrite(LED_PIN, !digitalRead(LED_PIN));      // ^ 1);
  digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  digitalWrite(LED_ONBAORD, !digitalRead(LED_ONBAORD)); //useful on nano's and some other 'duino's
  TOGGLE_COUNT++;
}    

ISR(INT0_vect) {
  // Do something here
  //digitalWrite(LED_PIN, !digitalRead(LED_PIN));      // ^ 1);
   //TOGGLE_COUNT++;
}


//####################################################################################
// SETUP
//####################################################################################
void setup () {
    Serial.begin(57600);
    
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);  
    pinMode(RTC_SQW_IN, INPUT);
    pinMode(INT0_PIN, INPUT);
        
    
    //--------RTC SETUP ------------
    Wire.begin();
    RTC.begin();

    if (! RTC.isrunning()) {
      Serial.println("RTC is NOT running!");
      // following line sets the RTC to the date & time this sketch was compiled
      RTC.adjust(DateTime(__DATE__, __TIME__));
    }
  
    DateTime now = RTC.now();
    DateTime compiled = DateTime(__DATE__, __TIME__);
    if (now.unixtime() < compiled.unixtime()) {
      //Serial.println("RTC is older than compile time!  Updating");
      RTC.adjust(DateTime(__DATE__, __TIME__));
    }
    
    RTC.enable32kHz(true);
    RTC.SQWEnable(true);
    RTC.BBSQWEnable(true);
    RTC.SQWFrequency( SQW_FREQ );
  
    char datastr[100];
    RTC.getControlRegisterData( datastr[0]  );
    Serial.print(  datastr );
 
  
  
    //--------INT 0---------------
    EICRA = 0;      //clear it
    EICRA |= (1 << ISC01);
    EICRA |= (1 << ISC00);   //ISC0[1:0] = 0b11  rising edge INT0 creates interrupt
    EIMSK |= (1 << INT0);    //enable INT0 interrupt
        
    //--------COUNTER 1 SETUP -------
    setupTimer1ForCounting((int)PWM_COUNT); 
    printTimer1Info();   
}


//####################################################################################
// MAIN
//####################################################################################
void loop () {
    Serial.print("Toggle Count over ");
    Serial.print(LOOP_DELAY, DEC);
    Serial.print("ms with PWM_COUNT of ");
    Serial.print(PWM_COUNT, DEC);
    Serial.print(":  ");
    Serial.print(TOGGLE_COUNT, DEC);
    Serial.println();
    TOGGLE_COUNT = 0;
  
    DateTime now = RTC.now();
    
    RTC.forceTempConv(true);  //DS3231 does this every 64 seconds, we are simply testing the function here
    float temp_float = RTC.getTempAsFloat();
    int16_t temp_word = RTC.getTempAsWord();
    int8_t temp_hbyte = temp_word >> 8;
    int8_t temp_lbyte = temp_word &= 0x00FF;
    
    
    
    Serial.print(now.year(), DEC);
    Serial.print('/');
    Serial.print(now.month(), DEC);
    Serial.print('/');
    Serial.print(now.day(), DEC);
    Serial.print(' ');
    Serial.print(now.hour(), DEC);
    Serial.print(':');
    Serial.print(now.minute(), DEC);
    Serial.print(':');
    Serial.print(now.second(), DEC);
    Serial.println();
    
    Serial.print(" since midnight 1/1/1970 = ");
    Serial.print(now.unixtime());
    Serial.print("s = ");
    Serial.print(now.unixtime() / 86400L);
    Serial.println("d");
    
    // calculate a date which is 7 days and 30 seconds into the future
    DateTime future (now.unixtime() + 7 * 86400L + 30);
    
    Serial.print(" now + 7d + 30s: ");
    Serial.print(future.year(), DEC);
    Serial.print('/');
    Serial.print(future.month(), DEC);
    Serial.print('/');
    Serial.print(future.day(), DEC);
    Serial.print(' ');
    Serial.print(future.hour(), DEC);
    Serial.print(':');
    Serial.print(future.minute(), DEC);
    Serial.print(':');
    Serial.print(future.second(), DEC);
    Serial.println();
    
    //Display temps
    Serial.print("Temp as float: ");
    Serial.print(temp_float, DEC);
    Serial.println();
    Serial.print("Temp as word: ");
    Serial.print(temp_hbyte, DEC);
    Serial.print(".");
    Serial.print(temp_lbyte, DEC);
    Serial.println();
    
    Serial.println();
    
    delay(LOOP_DELAY);
}


//#########################################################
// TIMER/COUNTER CONTROLS 
//   Timer 0 is PD4
//   Timer 1 is PD5
//#########################################################

/**
* Setup Timer 0 to count and interrupt
*
* --- TCCR0A ---
* COM0A1  COM0A0  COM0B1 COM0B0  xxx  xxx  WGM01  WGM00
* --- TCCR0B ---
* FOC0A   FOC0B   xxx     xxx    WGM02 CS02  CS01  CS00
*
*    In non-PWM Mode COM0A1:0 == 0b00  Normal port operation, OC0A disconnected
*        same for COM0B1:0   
* For WGM02 WGM01 WGM00 
*  0b000  Normal mode, with a TOP of 0xFF and update of OCRx immediately
*           Counting is upwards.  No counter clear performed.  Simply overruns and restarts.
*  0b010  CTC Mode with top of OCRA and update of OCRx immediately.
*        Clear Timer on Compare Match, OCR0A sets top.  Counter is cleared when TCNT0 reaches OCR0A
*        Need to set the OCF0A interrupt flag
*
* FOC0A/FOC0B should be set zero. In non-PWM mode it's a strobe. 
*        But does not generate any interrupt if using CTC mode.
* 
* CS02:0  Clock Select:
*   0b000  No clock source. Timer/Counter stopped
*   0b110  External clock on T0 pin FALLING EDGE
*   0b111  External clock on T0 pin RISING EDGE
*
* TCNT0  Timer counter register
*
* OCR0A/OCR0B  Output Compare Register
*
* --- TIMSK0 ---
* xxx xxx xxx xxx xxx OCIE0B  OCIE0A  TOIE0
*    OCIE0B/A are the Output Compare Interrupt Enable bits
*    TOIE0 is the Timer Overflow Interrup Enable
*
*  Must write OCIE0A to 1 and then I-bit in Status Register (SREG)
*
* --- TIFR0 ---   Timer Counter Interrupt Flag Register
*  xx xx xx xx   xx OCF0B OCF0A TOV0
*   OCF0B/A  Output Compare A/B Match Flag 
*          is set when a match occurs.  Cleared by hardware when executing interrupt.
*          Can also be cleared by writing a 1 to the flag.
*
*/
void setupTimer0ForCounting(uint8_t count) {
  //set WGM2:0 to 0b010 for CTC
  //set CS02:0 to 0b111 for rising edge external clock

  TCCR0A = 0;
  TCCR0B = 0;
  TIMSK0 = 0;
 
  TCCR0A |= (1 << WGM01);
//  TCCR0A = _BV(WGM01);

  TCCR0B |= (1 << CS02);
  TCCR0B |= (1 << CS01);
  TCCR0B |= (1 << CS00);

//  TCCR0B = _BV(CS02) || _BV(CS01) || _BV(CS00);
  
  TCNT0 = 0;
  
  OCR0A = count;      // SET COUNTER 
  
 // TIMSK0 = _BV(OCIE0A);   // SET INTERRUPTS
  TIMSK0 |= (1 << OCIE0A);
}

/**
* @function: setupTimer1ForCounting
* @param count  16-bit integer to go into OCR1A
*
* TCCR1A = [ COM1A1, COM1A0, COM1B1, COM1B0, xxx, xxx, WGM11, WGM10]
* TCCR1B = [ ICNC1,  ICES1,  xxx,    WGM13, WGM12, CS12, CS11, CS10]
* TCCR1C = [ FOC1A,  FOC1B, xxx, xxx, xxx, xxx, xxx, xxx]
* TIMSK1 = [ xxxx,  xxxx,  ICIE1,  xxxx, xxxx, OCIE1B, OCIE1A, TOIE1]
*
*  Set COM1A, COM1B to 0 for normal operation with OC1A/OC1B disonnected.
*  Set WGM13:0 to 0b0100 for CTC Mode using OCR1A
*
*  We won't use the Input Capture Noise Canceler (ICNC1)
*  CS12:0 to 0b111 for external source on T1, clock on rising edge.
*
*  TCNT1H and TCNT1L  (TCNT1) 
*  OCR1AH / OCR1AL  Output Compare Register 1A
*       Can we set this by controlling OCR1A only?  Or do we need to bit shift.
*
*  Set OCIE1A
*/
void setupTimer1ForCounting(int count) {
  //set WGM1[3:0] to 0b0100 for CTC mode using OCR1A. Clear Timer on Compare Match, OCR1A sets top. 
  //                            Counter is cleared when TCNT0 reaches OCR0A
  //set CS1[2:0] to 0b111 for external rising edge T1 clocking.
  //set OCR1A to count
  //set TIMSK1 to OCIE1A

  //clear it out
  TCCR1A = 0;      //nothing else to set
  TCCR1B = 0;
  TIMSK1 = 0;
 
  TCCR1B |= (1 << WGM12);
  TCCR1B |= (1 << CS12);
  TCCR1B |= (1 << CS11);
  TCCR1B |= (1 << CS10);  
  
  TCNT1 = 0;
  
  OCR1A = count;      // SET COUNTER 
  
  TIMSK1 |= (1 << OCIE1A);
}


void printTimer0Info() {
  Serial.println(" ");  
  Serial.println("----- Timer1 Information -----");
  Serial.print("TCCR0A: ");
  Serial.println(TCCR0A, BIN);
  Serial.print("TCCR0B: ");
  Serial.println(TCCR0B, BIN);
  Serial.print("TIMSK0: ");
  Serial.println(TIMSK0, BIN);
  Serial.println("------------------------------");
  Serial.println(" ");  
}


void printTimer1Info() {
  Serial.println(" ");  
  Serial.println("----- Timer1 Information -----");
  Serial.print("TCCR1A: ");
  Serial.println(TCCR1A, BIN);
  Serial.print("TCCR1B: ");
  Serial.println(TCCR1B, BIN);
  Serial.print("TIMSK1: ");
  Serial.println(TIMSK1, BIN);
  Serial.print("OCR1A: " );
  Serial.println(OCR1A, BIN);
  Serial.println("------------------------------");
  Serial.println(" ");  
}









// vim:ci:sw=4 sts=4 ft=cpp
