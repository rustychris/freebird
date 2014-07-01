// Based on Code by JeeLabs http://news.jeelabs.org/code/
// Released to the public domain! Enjoy!
//
// Modified and expanded by Red Byer 7/24/2013 to work with 3231 better
//     www.redstoyland.com      Find the code under "mizraith" on github
//
//  See .h file for the additions

#if ARDUINO < 100
#include <WProgram.h>
#else
#include <Arduino.h>
#endif

#include <avr/pgmspace.h>

#ifdef TEENSYDUINO
#include <i2c_t3.h>
#else
#include <Wire.h>
#endif

#include "RTClib.h"
#include "RTC_DS3231.h"


#if ARDUINO < 100
#define SEND(x) send(x) 
#define RECEIVE(x) receive(x) 
#else
#define SEND(x) write(static_cast<uint8_t>(x))
#define RECEIVE(x) read(x)
#endif

////////////////////////////////////////////////////////////////////////////////
// RTC_DS3231 implementation

uint8_t RTC_DS3231::begin(void)
{
    return 1;
}

uint8_t RTC_DS3231::isrunning(void)
{
    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.SEND(0);
    Wire.endTransmission();

    Wire.requestFrom(DS3231_ADDRESS, 1);
    uint8_t ss = Wire.RECEIVE();
    return !(ss>>7);
}

/**
 * Set the datetime of the RTC
**/
void RTC_DS3231::adjust(const DateTime& dt)
{
	//set the address pointer and then start writing
    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.SEND( DS3231_REG_SECONDS );           // was just '0'
    Wire.SEND(bin2bcd(dt.second()));
    Wire.SEND(bin2bcd(dt.minute()));
    Wire.SEND(bin2bcd(dt.hour()));
    Wire.SEND(bin2bcd(0));
    Wire.SEND(bin2bcd(dt.day()));
    Wire.SEND(bin2bcd(dt.month()));
    Wire.SEND(bin2bcd(dt.year() - 2000));
    Wire.SEND(0);
    Wire.endTransmission();
}


/**
 * Get the datetime from the RTC
 **/
DateTime RTC_DS3231::now()
{
	//set the address pointer in preparation for read
    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.SEND( DS3231_REG_SECONDS );           // was just '0'
    Wire.endTransmission();

    Wire.requestFrom(DS3231_ADDRESS, 7);
    uint8_t ss = bcd2bin(Wire.RECEIVE() & 0x7F);
    uint8_t mm = bcd2bin(Wire.RECEIVE());
    uint8_t hh = bcd2bin(Wire.RECEIVE());
    Wire.RECEIVE();
    uint8_t d = bcd2bin(Wire.RECEIVE());
    uint8_t m = bcd2bin(Wire.RECEIVE());
    uint16_t y = bcd2bin(Wire.RECEIVE()) + 2000;

    return DateTime (y, m, d, hh, mm, ss);
}


/**
 * Return temperature as a float in degrees C
 * Data is 10bits provided in 2 bytes (11h and and 2bits in 12h)
 * Resolution of 0.25C
 * e.g.  00011001 01     =    25.25C
 *       00011001 = 25
 *             01 = 0.25 * 1 = .25
 *
 *  This function has not been tested
 */
float RTC_DS3231::getTempAsFloat()
{
	//set the address pointer in preparation for read
    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.SEND( DS3231_REG_TEMP_MSB );           // was just '0'
    Wire.endTransmission();

	Wire.requestFrom(DS3231_ADDRESS, 2);
    int8_t sig   = Wire.RECEIVE();                  //signed MSB
    uint8_t fract = Wire.RECEIVE() & 0b11000000;    //rest should be zeroes anyway.
    
    fract = fract >> 6;                // now in 2 lsb's
    float temp = (float)fract * 0.25;  // total up,  .00, .25, .50
    if(sig < 0) {
    	temp = sig - temp;             // calculate the fract correctly
    } else {
	    temp = sig + temp;             // add    25 + .25 = 25.25
    }
    return temp;
}

/**
 * Return temperature as a 2 bytes in degrees C
 *    MSB is the significant  00011001 = 25
 *    LSB is the mantissa     00011001 = .25
 *  Display by writing them out to the display   MSB . LSB
 *
 * Data from the 3231 10bits provided in 2 bytes (11h and and 2bits in 12h)
 * Resolution of 0.25C
 * e.g.  00011001 01     =    25.25C
 *       00011001 = 25
 *             01 = 0.25 * 1 = .25
 *
 *  This function has not been tested
 */
int16_t RTC_DS3231::getTempAsWord()
{
	//set the address pointer in preparation for read
    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.SEND( DS3231_REG_TEMP_MSB );           // was just '0'
    Wire.endTransmission();

	Wire.requestFrom(DS3231_ADDRESS, 2);
    int8_t sig   = Wire.RECEIVE();                  //signed MSB
    uint8_t fract = Wire.RECEIVE() & 0b11000000;    //rest should be zeroes anyway.
    
    int16_t temp = sig;           //put into lower byte
    temp = temp << 8 ;            //shift to upper byte
    
    fract = fract >> 6;                // shift upper 2 bits to lowest 2 
    fract = fract * 25;                // multiply up
    
    temp = temp + fract;
    
    return temp;
}


/**
 *  Enable or disable the 32kHz pin.  When 1 it
 * will output 32768kHz.
 *
 */
void RTC_DS3231::enable32kHz(uint8_t enable)
{
	Wire.beginTransmission(DS3231_ADDRESS);
	Wire.SEND( DS3231_REG_STATUS_CTL );  
	Wire.endTransmission();

	// status register
	Wire.requestFrom(DS3231_ADDRESS, 1);

	uint8_t sreg = bcd2bin(Wire.RECEIVE());    // do we need to wrap in bcd2bin?

	if (enable == true) {
		sreg |=  0b00001000; // Enable EN32KHZ bit
	} else {
		sreg &= ~0b00001000; // else set EN32KHZ bit to 0
	}

	Wire.beginTransmission(DS3231_ADDRESS);
	Wire.SEND( DS3231_REG_STATUS_CTL );
	Wire.SEND(sreg);
	Wire.endTransmission();
}


/**
 * Force the temp sensor to convert the temp
 * into digital code and execute the TCXO 
 * algorithm.  The DS3231 normally does
 * this every 64 seconds anyway.
 *
 * NOTE: This is a BLOCKING method.  You have been warned.
 *
 * This method has not been fully debugged.
 */
void RTC_DS3231::forceTempConv(uint8_t block)
{
	Wire.beginTransmission(DS3231_ADDRESS);
	Wire.SEND( DS3231_REG_CONTROL );
	Wire.endTransmission();

	// control register
	Wire.requestFrom(DS3231_ADDRESS, 1);

	uint8_t creg = Wire.RECEIVE();  // do we need the bcd2bin

	creg |= 0b00100000; // Write CONV bit

	Wire.beginTransmission(DS3231_ADDRESS);
	Wire.SEND(DS3231_REG_CONTROL);
	Wire.SEND(creg);
	Wire.endTransmission();

	do
	{
		// Block until CONV is 0
		Wire.beginTransmission(DS3231_ADDRESS);
		Wire.SEND(DS3231_REG_CONTROL);
		Wire.endTransmission();
		Wire.requestFrom(DS3231_ADDRESS, 1);
	} while ((block && (Wire.RECEIVE() & 0b00100000) != 0));
}  


/**
 * Enable or disable the output to the SQW pin.
 *  Send 1 or true to enable.
 *  This function does _not_ enable battery backed output
 * which helps conserve battery life.
 *
 */
void RTC_DS3231::SQWEnable(uint8_t enable)
{
	Wire.beginTransmission(DS3231_ADDRESS);
	Wire.SEND(DS3231_REG_CONTROL);
	Wire.endTransmission();

	// control register
	Wire.requestFrom(DS3231_ADDRESS, 1);

	uint8_t creg = Wire.RECEIVE();     //do we need the bcd2bin

	
	if (enable == true) {
		creg &= ~0b00000100; // Clear INTCN bit to output the square wave
	} else {
		creg |= 0b00000100;      // Set INTCN to 1 -- disables SQW
	}

	Wire.beginTransmission(DS3231_ADDRESS);
	Wire.SEND(DS3231_REG_CONTROL);
	Wire.SEND(creg);
	Wire.endTransmission();
}


/**
 * Enable or disable the output to the SQW pin.
 *  Send 1 or true to enable.
 * This method enables BOTH the square wave
 * and the battery backed output.
 *
 */
void RTC_DS3231::BBSQWEnable(uint8_t enable)
{
	Wire.beginTransmission(DS3231_ADDRESS);
	Wire.SEND(DS3231_REG_CONTROL);
	Wire.endTransmission();

	// control register
	Wire.requestFrom(DS3231_ADDRESS, 1);

	uint8_t creg = Wire.RECEIVE();     //do we need the bcd2bin


	if (enable == true) {
		creg |=  0b01000000; // Enable BBSQW if required so SQW continues to output (battery life reduction).
		creg &= ~0b00000100; // Clear INTCN bit to output the square wave
	} else {
		creg &= ~0b01000000;    // Set BBSQW 0 to disable battery back
		//creg |=  0b00000100;      // Set INTCN to 1 -- disables SQW
	}

	Wire.beginTransmission(DS3231_ADDRESS);
	Wire.SEND(DS3231_REG_CONTROL);
	Wire.SEND(creg);
	Wire.endTransmission();
}

/**
 *  Set the output frequence of the squarewave SQW pin
 *
 *  HINT:  Use the defined frequencies in the .h file
 *  
 */
void RTC_DS3231::SQWFrequency(uint8_t freq)
{
	Wire.beginTransmission(DS3231_ADDRESS);
	Wire.SEND( DS3231_REG_CONTROL );
	Wire.endTransmission();

	// control register
	Wire.requestFrom(DS3231_ADDRESS, 1);

	uint8_t creg = Wire.RECEIVE();

	creg &= ~0b00011000; // Set to 0
	creg |= freq; // Set freq bits

	Wire.beginTransmission(DS3231_ADDRESS);
	Wire.SEND(0x0E);
	Wire.SEND(creg);
	Wire.endTransmission();
}

void RTC_DS3231::getControlRegisterData(char &datastr) {

	Wire.beginTransmission(DS3231_ADDRESS);
    Wire.write( DS3231_REG_CONTROL );
    Wire.endTransmission();

    // control registers
    Wire.requestFrom(DS3231_ADDRESS, 2);
    uint8_t creg = Wire.RECEIVE();  // do we need the bcd2bin
    uint8_t sreg = Wire.RECEIVE(); 
    
    char cregstr[] = "00000000";
    char sregstr[] = "00000000";
    getBinaryString(creg, cregstr);
    getBinaryString(sreg, sregstr);
    
    strcpy(&datastr, "\n----- DS3231 Information -----\ncreg: ");
    strcat(&datastr, cregstr);
    strcat(&datastr, "\nsreg: ");
    strcat(&datastr, sregstr);
    strcat(&datastr, "\n------------------------------\n");     

    return;
}


/********************************************************
* UTILITY and WORKER METHODS
*********************************************************/

/**
 * Takes one byte and loads up bytestr[8] with the value
 *  eg  "8" loads bytestr with "00000111"
 */
void RTC_DS3231::getBinaryString(uint8_t byteval, char bytestr[]) 
{
	uint8_t bitv;
	int i = 0;
	
    for (i = 0; i < 8; i++) {                    
           bitv = (byteval >> i) & 1;
           if (bitv == 0) {
               bytestr[7-i] = '0';
           } else {
               bytestr[7-i] = '1';
           }
    }
}




// vim:ci:sw=4 sts=4 ft=cpp
