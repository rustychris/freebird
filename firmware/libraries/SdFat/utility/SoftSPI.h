/* Arduino DigitalIO Library
 * Copyright (C) 2013 by William Greiman
 *
 * This file is part of the Arduino DigitalIO Library
 *
 * This Library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the Arduino DigitalIO Library.  If not, see
 * <http://www.gnu.org/licenses/>.
 */
/**
 * @file 
 * @brief  Software SPI.
 *
 * @defgroup softSPI Software SPI
 * @details  Software SPI Template Class.
 * @{
 */

#ifndef SoftSPI_h
#define SoftSPI_h
#include <DigitalPin.h>
//------------------------------------------------------------------------------
/** Nop for timing. */
#define nop asm volatile ("nop\n\t")
//------------------------------------------------------------------------------
/** Pin Mode for MISO is input.*/
const bool MISO_MODE  = false;
/** Pullups disabled for MISO are disabled. */
const bool MISO_LEVEL = false;
/** Pin Mode for MOSI is output.*/
const bool MOSI_MODE  = true;
/** Pin Mode for SCK is output. */
const bool SCK_MODE   = true;
//------------------------------------------------------------------------------
/**
 * @class SoftSPI
 * @brief Fast software SPI.
 */


template<uint8_t MisoPin, uint8_t MosiPin, uint8_t SckPin, uint8_t Mode = 0>
class SoftSPI {
 public:
#ifdef __arm__
  // needed by the fast bit-banging SPI code stolen from SDadafruit
  volatile RwReg *mosiport, *clkport, *misoport;
  uint32_t mosipinmask, clkpinmask, misopinmask;
#endif

 //-----------------------------------------------------------------------------
 /** Initialize SoftSPI pins. */
  void begin() {
#ifdef __AVR__
    fastPinConfig(MisoPin, MISO_MODE, MISO_LEVEL);
    fastPinConfig(MosiPin, MOSI_MODE, !MODE_CPHA(Mode));
    fastPinConfig(SckPin, SCK_MODE, MODE_CPOL(Mode));
#elif defined __arm__
    // from Adafruit Sd2Card.cpp
    // use slow bitbang mode
    pinMode(MisoPin, INPUT);
    pinMode(MosiPin, OUTPUT);
    pinMode(SckPin, OUTPUT);
    clkport     = portOutputRegister(digitalPinToPort(SckPin));
    clkpinmask  = digitalPinToBitMask(SckPin);
    mosiport    = portOutputRegister(digitalPinToPort(MosiPin));
    mosipinmask = digitalPinToBitMask(MosiPin);
    misoport    = portInputRegister(digitalPinToPort(MisoPin));
    misopinmask = digitalPinToBitMask(MisoPin);
#endif 
  }
  //----------------------------------------------------------------------------
  /** Soft SPI receive byte.
   * @return Data byte received.
   */
  inline __attribute__((always_inline))
  uint8_t receive() {
    uint8_t data = 0;
    receiveBit(7, &data);
    receiveBit(6, &data);
    receiveBit(5, &data);
    receiveBit(4, &data);
    receiveBit(3, &data);
    receiveBit(2, &data);
    receiveBit(1, &data);
    receiveBit(0, &data);
    return data;
  }
  //----------------------------------------------------------------------------
  /** Soft SPI send byte.
   * @param[in] data Data byte to send.
   */
  inline __attribute__((always_inline))
  void send(uint8_t data) {
    sendBit(7, data);
    sendBit(6, data);
    sendBit(5, data);
    sendBit(4, data);
    sendBit(3, data);
    sendBit(2, data);
    sendBit(1, data);
    sendBit(0, data);
  }
  //----------------------------------------------------------------------------
  /** Soft SPI transfer byte.
   * @param[in] txData Data byte to send.
   * @return Data byte received.
   */
  inline __attribute__((always_inline))
  uint8_t transfer(uint8_t txData) {
    uint8_t rxData = 0;
    transferBit(7, &rxData, txData);
    transferBit(6, &rxData, txData);
    transferBit(5, &rxData, txData);
    transferBit(4, &rxData, txData);
    transferBit(3, &rxData, txData);
    transferBit(2, &rxData, txData);
    transferBit(1, &rxData, txData);
    transferBit(0, &rxData, txData);
    return rxData;
  }
 private:
  //----------------------------------------------------------------------------
  inline __attribute__((always_inline))
  bool MODE_CPHA(uint8_t mode) {return (mode & 1) != 0;}
  inline __attribute__((always_inline))
  bool MODE_CPOL(uint8_t mode) {return (mode & 2) != 0;}
  inline __attribute__((always_inline))
  void receiveBit(uint8_t bit, uint8_t* data) {
#ifdef __AVR__
    if (MODE_CPHA(Mode)) {
      fastDigitalWrite(SckPin, !MODE_CPOL(Mode));
    }
    nop;nop;
    fastDigitalWrite(SckPin,
      MODE_CPHA(Mode) ? MODE_CPOL(Mode) : !MODE_CPOL(Mode));
    if (fastDigitalRead(MisoPin)) *data |= 1 << bit;
    if (!MODE_CPHA(Mode)) {
      fastDigitalWrite(SckPin, MODE_CPOL(Mode));
    }
#elif defined __arm__
    // this is the inner loop from Sd2Card.cpp:90
    // output pin high - like sending 0XFF
    *mosiport |= mosipinmask;  // not sure why we care what mosi is set to...
    *clkport |= clkpinmask;
    nop; nop; // RH - originally 2 nops
    //if (fastDigitalRead(SPI_MISO_PIN)) data |= 1;
    if ((*misoport) & misopinmask)  *data |= 1<<bit;
    *clkport &=  ~clkpinmask;
    // adjust so SCK is nice
    nop; nop; // originally 2 nops
#endif
  }
  //----------------------------------------------------------------------------
  inline __attribute__((always_inline))
  void sendBit(uint8_t bit, uint8_t data) {
#ifdef __AVR__
    if (MODE_CPHA(Mode)) {
      fastDigitalWrite(SckPin, !MODE_CPOL(Mode));
    }
    fastDigitalWrite(MosiPin, data & (1 << bit));
    fastDigitalWrite(SckPin,
      MODE_CPHA(Mode) ? MODE_CPOL(Mode) : !MODE_CPOL(Mode));
    nop;nop;
    if (!MODE_CPHA(Mode)) {
      fastDigitalWrite(SckPin, MODE_CPOL(Mode));
    }
#elif defined __arm__
    // Fast SPI bitbang swiped from LPD8806 library
    // RH: inner loop swiped again from Sd2Card, Adafruit
    *clkport &= ~clkpinmask;
    nop; nop; nop; // RH - originally 3 nops
    if (data & (1<<bit) )
      *mosiport |= mosipinmask;
    else
      *mosiport &= ~mosipinmask;
    *clkport |=  clkpinmask;
    nop; nop; nop; nop; // RH: was just once outside loop - was originally 4 nops
    *clkport &= ~clkpinmask;
#endif
  }
  //----------------------------------------------------------------------------
  inline __attribute__((always_inline))
  void transferBit(uint8_t bit, uint8_t* rxData, uint8_t txData) {
#ifdef __AVR__
    if (MODE_CPHA(Mode)) {
      fastDigitalWrite(SckPin, !MODE_CPOL(Mode));
    }
    fastDigitalWrite(MosiPin, txData & (1 << bit));
    fastDigitalWrite(SckPin,
      MODE_CPHA(Mode) ? MODE_CPOL(Mode) : !MODE_CPOL(Mode));
    if (fastDigitalRead(MisoPin)) *rxData |= 1 << bit;
    if (!MODE_CPHA(Mode)) {
      fastDigitalWrite(SckPin, MODE_CPOL(Mode));
    }
#elif defined __arm__
    // Fast SPI bitbang swiped from LPD8806 library
    // RH: inner loop swiped again from Sd2Card, Adafruit
    *clkport &= ~clkpinmask;
    nop; nop; nop; // RH - originally 3 nops
    if (txData & (1<<bit) )
      *mosiport |= mosipinmask;
    else
      *mosiport &= ~mosipinmask;
    *clkport |=  clkpinmask;
    nop;nop;nop;nop; // RH: was just once outside loop - originally 4 nops

    if ((*misoport) & misopinmask)  *rxData |= 1<<bit;
    *clkport &= ~clkpinmask;
    nop; nop; nop; // RH - originally 3 nops
#endif
  }
  //----------------------------------------------------------------------------
};
#endif  // SoftSPI_h
/** @} */
