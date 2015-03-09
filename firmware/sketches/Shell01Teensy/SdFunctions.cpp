/*
 * [RH] - copied/adapted from the SdFat example.
 * -- change the toplevel control structure to be called from the Shell.
 * -- expose other functions as necessary
 *
 * This sketch will format an SD or SDHC card.
 * Warning all data will be deleted!
 *
 * For SD/SDHC cards larger than 64 MB this
 * sketch attempts to match the format
 * generated by SDFormatter available here:
 *
 * http://www.sdcard.org/consumers/formatter/
 *
 * For smaller cards this sketch uses FAT16
 * and SDFormatter uses FAT12.
 */
#include "freebird_config.h"
#include <Arduino.h>
#include "logger.h"

// Print extra info for debug if DEBUG_PRINT is nonzero
#include <SdFat.h>
#if DEBUG_PRINT
#include <SdFatUtil.h>
#endif  // DEBUG_PRINT

#include "SdFunctions.h"

// Change the value of chipSelect if your hardware does
// not use the default value, SS.  Common values are:
// Arduino Ethernet shield: pin 4
// Sparkfun SD shield: pin 8
// Adafruit SD shields and modules: pin 10
const uint8_t chipSelect = SD_PIN_CS;

// Change spiSpeed to SPI_FULL_SPEED for better performance
// Use SPI_QUARTER_SPEED for even slower SPI bus speed
const uint8_t spiSpeed = SD_SPEED;

#include "serialmux.h"

Sd2Card card; // for formatting
SdFat sd;     // for logging
SdFile myFile;// for logging

uint32_t cardSizeBlocks;
uint16_t cardCapacityMB;

// cache for SD block
cache_t cache;

// MBR information
uint8_t partType;
uint32_t relSector;
uint32_t partSize;

// Fake disk geometry
uint8_t numberOfHeads;
uint8_t sectorsPerTrack;

// FAT parameters
uint16_t reservedSectors;
uint8_t sectorsPerCluster;
uint32_t fatStart;
uint32_t fatSize;
uint32_t dataStart;

// constants for file system structure
uint16_t const BU16 = 128;
uint16_t const BU32 = 8192;

//  strings needed in file system structures
char noName[] = "FREEBIRD   ";
char fat16str[] = "FAT16   ";
char fat32str[] = "FAT32   ";
//------------------------------------------------------------------------------
#define sdError(msg) sdError_P(PSTR(msg))

void sdError_P(const char* str) {
  cout << pstr("error: ");
  cout << pgm(str) << endl;
  if (card.errorCode()) {
    cout << pstr("SD error: ") << hex << int(card.errorCode());
    cout << ',' << int(card.errorData()) << dec << endl;
  }
  while (1);
}
//------------------------------------------------------------------------------
#if DEBUG_PRINT
void debugPrint() {
  cout << pstr("FreeRam: ") << FreeRam() << endl;
  cout << pstr("partStart: ") << relSector << endl;
  cout << pstr("partSize: ") << partSize << endl;
  cout << pstr("reserved: ") << reservedSectors << endl;
  cout << pstr("fatStart: ") << fatStart << endl;
  cout << pstr("fatSize: ") << fatSize << endl;
  cout << pstr("dataStart: ") << dataStart << endl;
  cout << pstr("clusterCount: ");
  cout << ((relSector + partSize - dataStart)/sectorsPerCluster) << endl;
  cout << endl;
  cout << pstr("Heads: ") << int(numberOfHeads) << endl;
  cout << pstr("Sectors: ") << int(sectorsPerTrack) << endl;
  cout << pstr("Cylinders: ");
  cout << cardSizeBlocks/(numberOfHeads*sectorsPerTrack) << endl;
}
#endif  // DEBUG_PRINT
//------------------------------------------------------------------------------
// write cached block to the card
uint8_t writeCache(uint32_t lbn) {
  return card.writeBlock(lbn, cache.data);
}
//------------------------------------------------------------------------------
// initialize appropriate sizes for SD capacity
void initSizes() {
  if (cardCapacityMB <= 6) {
    sdError("Card is too small.");
  } else if (cardCapacityMB <= 16) {
    sectorsPerCluster = 2;
  } else if (cardCapacityMB <= 32) {
    sectorsPerCluster = 4;
  } else if (cardCapacityMB <= 64) {
    sectorsPerCluster = 8;
  } else if (cardCapacityMB <= 128) {
    sectorsPerCluster = 16;
  } else if (cardCapacityMB <= 1024) {
    sectorsPerCluster = 32;
  } else if (cardCapacityMB <= 32768) {
    sectorsPerCluster = 64;
  } else {
    // SDXC cards
    sectorsPerCluster = 128;
  }

  cout << pstr("Blocks/Cluster: ") << int(sectorsPerCluster) << endl;
  // set fake disk geometry
  sectorsPerTrack = cardCapacityMB <= 256 ? 32 : 63;

  if (cardCapacityMB <= 16) {
    numberOfHeads = 2;
  } else if (cardCapacityMB <= 32) {
    numberOfHeads = 4;
  } else if (cardCapacityMB <= 128) {
    numberOfHeads = 8;
  } else if (cardCapacityMB <= 504) {
    numberOfHeads = 16;
  } else if (cardCapacityMB <= 1008) {
    numberOfHeads = 32;
  } else if (cardCapacityMB <= 2016) {
    numberOfHeads = 64;
  } else if (cardCapacityMB <= 4032) {
    numberOfHeads = 128;
  } else {
    numberOfHeads = 255;
  }
}
//------------------------------------------------------------------------------
// zero cache and optionally set the sector signature
void clearCache(uint8_t addSig) {
  memset(&cache, 0, sizeof(cache));
  if (addSig) {
    cache.mbr.mbrSig0 = BOOTSIG0;
    cache.mbr.mbrSig1 = BOOTSIG1;
  }
}
//------------------------------------------------------------------------------
// zero FAT and root dir area on SD
void clearFatDir(uint32_t bgn, uint32_t count) {
  clearCache(false);
  if (!card.writeStart(bgn, count)) {
    sdError("Clear FAT/DIR writeStart failed");
  }
  for (uint32_t i = 0; i < count; i++) {
    if ((i & 0XFF) == 0) cout << '.';
    if (!card.writeData(cache.data)) {
      sdError("Clear FAT/DIR writeData failed");
    }
  }
  if (!card.writeStop()) {
    sdError("Clear FAT/DIR writeStop failed");
  }
  cout << endl;
}
//------------------------------------------------------------------------------
// return cylinder number for a logical block number
uint16_t lbnToCylinder(uint32_t lbn) {
  return lbn / (numberOfHeads * sectorsPerTrack);
}
//------------------------------------------------------------------------------
// return head number for a logical block number
uint8_t lbnToHead(uint32_t lbn) {
  return (lbn % (numberOfHeads * sectorsPerTrack)) / sectorsPerTrack;
}
//------------------------------------------------------------------------------
// return sector number for a logical block number
uint8_t lbnToSector(uint32_t lbn) {
  return (lbn % sectorsPerTrack) + 1;
}
//------------------------------------------------------------------------------
// format and write the Master Boot Record
void writeMbr() {
  clearCache(true);
  part_t* p = cache.mbr.part;
  p->boot = 0;
  uint16_t c = lbnToCylinder(relSector);
  if (c > 1023) sdError("MBR CHS");
  p->beginCylinderHigh = c >> 8;
  p->beginCylinderLow = c & 0XFF;
  p->beginHead = lbnToHead(relSector);
  p->beginSector = lbnToSector(relSector);
  p->type = partType;
  uint32_t endLbn = relSector + partSize - 1;
  c = lbnToCylinder(endLbn);
  if (c <= 1023) {
    p->endCylinderHigh = c >> 8;
    p->endCylinderLow = c & 0XFF;
    p->endHead = lbnToHead(endLbn);
    p->endSector = lbnToSector(endLbn);
  } else {
    // Too big flag, c = 1023, h = 254, s = 63
    p->endCylinderHigh = 3;
    p->endCylinderLow = 255;
    p->endHead = 254;
    p->endSector = 63;
  }
  p->firstSector = relSector;
  p->totalSectors = partSize;
  if (!writeCache(0)) sdError("write MBR");
}
//------------------------------------------------------------------------------
// generate serial number from card size and micros since boot
uint32_t volSerialNumber() {
  return (cardSizeBlocks << 8) + micros();
}
//------------------------------------------------------------------------------
// format the SD as FAT16
void makeFat16() {
  uint32_t nc;
  for (dataStart = 2 * BU16;; dataStart += BU16) {
    nc = (cardSizeBlocks - dataStart)/sectorsPerCluster;
    fatSize = (nc + 2 + 255)/256;
    uint32_t r = BU16 + 1 + 2 * fatSize + 32;
    if (dataStart < r) continue;
    relSector = dataStart - r + BU16;
    break;
  }
  // check valid cluster count for FAT16 volume
  if (nc < 4085 || nc >= 65525) sdError("Bad cluster count");
  reservedSectors = 1;
  fatStart = relSector + reservedSectors;
  partSize = nc * sectorsPerCluster + 2 * fatSize + reservedSectors + 32;
  if (partSize < 32680) {
    partType = 0X01;
  } else if (partSize < 65536) {
    partType = 0X04;
  } else {
    partType = 0X06;
  }
  // write MBR
  writeMbr();
  clearCache(true);
  fat_boot_t* pb = &cache.fbs;
  pb->jump[0] = 0XEB;
  pb->jump[1] = 0X00;
  pb->jump[2] = 0X90;
  for (uint8_t i = 0; i < sizeof(pb->oemId); i++) {
    pb->oemId[i] = ' ';
  }
  pb->bytesPerSector = 512;
  pb->sectorsPerCluster = sectorsPerCluster;
  pb->reservedSectorCount = reservedSectors;
  pb->fatCount = 2;
  pb->rootDirEntryCount = 512;
  pb->mediaType = 0XF8;
  pb->sectorsPerFat16 = fatSize;
  pb->sectorsPerTrack = sectorsPerTrack;
  pb->headCount = numberOfHeads;
  pb->hidddenSectors = relSector;
  pb->totalSectors32 = partSize;
  pb->driveNumber = 0X80;
  pb->bootSignature = EXTENDED_BOOT_SIG;
  pb->volumeSerialNumber = volSerialNumber();
  memcpy(pb->volumeLabel, noName, sizeof(pb->volumeLabel));
  memcpy(pb->fileSystemType, fat16str, sizeof(pb->fileSystemType));
  // write partition boot sector
  if (!writeCache(relSector)) {
    sdError("FAT16 write PBS failed");
  }
  // clear FAT and root directory
  clearFatDir(fatStart, dataStart - fatStart);
  clearCache(false);
  cache.fat16[0] = 0XFFF8;
  cache.fat16[1] = 0XFFFF;
  // write first block of FAT and backup for reserved clusters
  if (!writeCache(fatStart)
    || !writeCache(fatStart + fatSize)) {
    sdError("FAT16 reserve failed");
  }
}
//------------------------------------------------------------------------------
// format the SD as FAT32
void makeFat32() {
  uint32_t nc;
  relSector = BU32;
  for (dataStart = 2 * BU32;; dataStart += BU32) {
    nc = (cardSizeBlocks - dataStart)/sectorsPerCluster;
    fatSize = (nc + 2 + 127)/128;
    uint32_t r = relSector + 9 + 2 * fatSize;
    if (dataStart >= r) break;
  }
  // error if too few clusters in FAT32 volume
  if (nc < 65525) sdError("Bad cluster count");
  reservedSectors = dataStart - relSector - 2 * fatSize;
  fatStart = relSector + reservedSectors;
  partSize = nc * sectorsPerCluster + dataStart - relSector;
  // type depends on address of end sector
  // max CHS has lbn = 16450560 = 1024*255*63
  if ((relSector + partSize) <= 16450560) {
    // FAT32
    partType = 0X0B;
  } else {
    // FAT32 with INT 13
    partType = 0X0C;
  }
  writeMbr();
  clearCache(true);

  fat32_boot_t* pb = &cache.fbs32;
  pb->jump[0] = 0XEB;
  pb->jump[1] = 0X00;
  pb->jump[2] = 0X90;
  for (uint8_t i = 0; i < sizeof(pb->oemId); i++) {
    pb->oemId[i] = ' ';
  }
  pb->bytesPerSector = 512;
  pb->sectorsPerCluster = sectorsPerCluster;
  pb->reservedSectorCount = reservedSectors;
  pb->fatCount = 2;
  pb->mediaType = 0XF8;
  pb->sectorsPerTrack = sectorsPerTrack;
  pb->headCount = numberOfHeads;
  pb->hidddenSectors = relSector;
  pb->totalSectors32 = partSize;
  pb->sectorsPerFat32 = fatSize;
  pb->fat32RootCluster = 2;
  pb->fat32FSInfo = 1;
  pb->fat32BackBootBlock = 6;
  pb->driveNumber = 0X80;
  pb->bootSignature = EXTENDED_BOOT_SIG;
  pb->volumeSerialNumber = volSerialNumber();
  memcpy(pb->volumeLabel, noName, sizeof(pb->volumeLabel));
  memcpy(pb->fileSystemType, fat32str, sizeof(pb->fileSystemType));
  // write partition boot sector and backup
  if (!writeCache(relSector)
    || !writeCache(relSector + 6)) {
    sdError("FAT32 write PBS failed");
  }
  clearCache(true);
  // write extra boot area and backup
  if (!writeCache(relSector + 2)
    || !writeCache(relSector + 8)) {
    sdError("FAT32 PBS ext failed");
  }
  fat32_fsinfo_t* pf = &cache.fsinfo;
  pf->leadSignature = FSINFO_LEAD_SIG;
  pf->structSignature = FSINFO_STRUCT_SIG;
  pf->freeCount = 0XFFFFFFFF;
  pf->nextFree = 0XFFFFFFFF;
  // write FSINFO sector and backup
  if (!writeCache(relSector + 1)
    || !writeCache(relSector + 7)) {
    sdError("FAT32 FSINFO failed");
  }
  clearFatDir(fatStart, 2 * fatSize + sectorsPerCluster);
  clearCache(false);
  cache.fat32[0] = 0x0FFFFFF8;
  cache.fat32[1] = 0x0FFFFFFF;
  cache.fat32[2] = 0x0FFFFFFF;
  // write first block of FAT and backup for reserved clusters
  if (!writeCache(fatStart)
    || !writeCache(fatStart + fatSize)) {
    sdError("FAT32 reserve failed");
  }
}
//------------------------------------------------------------------------------
// flash erase all data
uint32_t const ERASE_SIZE = 262144L;
void eraseCard() {
  cout << endl << pstr("Erasing\n");
  uint32_t firstBlock = 0;
  uint32_t lastBlock;
  uint16_t n = 0;

  do {
    lastBlock = firstBlock + ERASE_SIZE - 1;
    if (lastBlock >= cardSizeBlocks) lastBlock = cardSizeBlocks - 1;
    if (!card.erase(firstBlock, lastBlock)) sdError("erase failed");
    cout << '.';
    if ((n++)%32 == 31) cout << endl;
    firstBlock += ERASE_SIZE;
  } while (firstBlock < cardSizeBlocks);
  cout << endl;

  if (!card.readBlock(0, cache.data)) sdError("readBlock");
  cout << hex << showbase << setfill('0') << internal;
  cout << pstr("All data set to ") << setw(4) << int(cache.data[0]) << endl;
  cout << dec << noshowbase << setfill(' ') << right;
  cout << pstr("Erase done\n");
}
//------------------------------------------------------------------------------
void formatCard() {
  cout << endl;
  cout << pstr("Formatting\n");
  initSizes();
  if (card.type() != SD_CARD_TYPE_SDHC) {
    cout << pstr("FAT16\n");
    makeFat16();
  } else {
    cout << pstr("FAT32\n");
    makeFat32();
  }
#if DEBUG_PRINT
  debugPrint();
#endif  // DEBUG_PRINT
  cout << pstr("Format done\n");
}

//------------------------------------------------------------------------------
// setup() removed since we're using this as a library

void Storage::format(char c) {
#ifdef DISABLE_STORE
  return;
#endif
  // options for c
  //  'E' erase only
  //  'F' erase and format
  //  'Q' quick format only

  //  Soft spi options are configured in SdFat/SdFatConfig.h
  if (!card.init(SD_SPEED, SD_PIN_CS)) {
    mySerial.print(
     "\nSD initialization failure!\n"
     "Is the SD card inserted correctly?\n"
     "Is chip select correct at the top of this sketch?\n");
    sdError("card.init failed");
  }
  cardSizeBlocks = card.cardSize();
  if (cardSizeBlocks == 0) sdError("cardSize");
  cardCapacityMB = (cardSizeBlocks + 2047)/2048;

  mySerial.print("Card Size: ");
  mySerial.print(cardCapacityMB);
  mySerial.println(" MB, (MB = 1,048,576 bytes)");

  if (c == 'E' || c == 'F') {
    eraseCard();
  }
  if (c == 'F' || c == 'Q') {
    formatCard();
  }
}


/***************/
    
/**** SD logging code, stolen from fat16lib's AnalogIsrLogger ****/
/*  mainly to get efficient block writes */

// tradeoff of latency tolerance versus RAM usage.
// at 10kHz logging, 12 got some overruns.
const uint8_t BUFFER_BLOCK_COUNT = 30; 
const uint16_t DATA_DIM = 504;  

#define FLAG_TYPE_MASK 1
#define FLAG_TYPE_DATA 0
#define FLAG_TYPE_TEXT 1
#define FLAG_OVERRUN 2

struct block_t {
  uint32_t unixtime; 
  uint16_t ticks;
  uint8_t frame_count;
  uint8_t flags;

  uint8_t data[DATA_DIM];
};

// queues of 512 byte SD blocks
const uint8_t QUEUE_DIM = 32;  // Must be a power of two!

block_t* isrBuf;
uint16_t isrBuf_pos; // byte offset into data attribute

uint16_t isrOver = 0;

block_t* emptyQueue[QUEUE_DIM];
uint8_t emptyHead;
uint8_t emptyTail;

block_t* fullQueue[QUEUE_DIM];
uint8_t fullHead;
uint8_t fullTail;


// allocate buffer space
// note that this is always used, regardless of whether we're sampling or 
// not.  If more functions are added which want some RAM when not 
// sampling, this will have to be changed
uint8_t block[512 * BUFFER_BLOCK_COUNT];

inline uint8_t queueNext(uint8_t ht) {return (ht + 1) & (QUEUE_DIM -1);}

void Storage::begin(void) {
  // this is called once on boot up
#ifndef DISABLE_STORE
  // And initialize the SD interface and open a file
  if (!sd.begin(SD_PIN_CS, SD_SPEED)) {
    // rather than halt, go into a loop repeating the message 
    // to make it easier to catch on a serial console
    while(1) {
      sd.initErrorPrint();
      delay(500);
    }
  }
#endif
}
// This is called before sampling begins, setting up the buffers
// and in the future possibly pre-allocating files.
void Storage::setup(void) {
  // initialize queues
  emptyHead = emptyTail = 0;
  fullHead = fullTail = 0;
  
  // initialize ISR
  isrBuf = 0;
  isrOver = 0;

  // possible to use the SdFat buffer for a block, 
  // but try keeping the logic here independent of that level
  // of internals, and just pay the price of some copying
  
  // put rest of buffers in empty queue
  // relies on fullQueue and emptyQueue being initialized to 0
  // by the compiler.
  for (uint8_t i = 0; i < BUFFER_BLOCK_COUNT; i++) {
    emptyQueue[emptyHead] = (block_t*)(block + 512 * i);
    emptyHead = queueNext(emptyHead);
  }

  if( ! log_to_serial ) {
#ifndef DISABLE_STORE
    open_next_file();
#endif
  }

  overruns = 0;
  frame_count = 0;
}

void Storage::open_next_file(void) {
  set_next_active_filename();
  if (!myFile.open(active_filename, O_RDWR | O_CREAT)) {
    sd.errorHalt("opening output file for write failed");
  } 
  DateTime dt=logger.now();
  myFile.timestamp(T_ACCESS|T_CREATE|T_WRITE,
                   dt.year(),dt.month(),dt.day(),dt.hour(),dt.minute(),dt.second());
  sync_counter=0;
}

/* 
  Find the first filename of the form dataNNNN.bin which doesn't
  exist, starting at data0000.bin
 */
void Storage::set_next_active_filename(void) {
#ifdef DISABLE_STORE
  strcpy(active_filename,"--DISABLED--");
  return;
#else
  strcpy(active_filename,DATAFILETEMPLATE);

  //kludgy way of formatting strings
  for(active_filename[4]='0';active_filename[4]<='9';active_filename[4]++) {
    for(active_filename[5]='0';active_filename[5]<='9';active_filename[5]++) {
      for(active_filename[6]='0';active_filename[6]<='9';active_filename[6]++) {
        for(active_filename[7]='0';active_filename[7]<='9';active_filename[7]++) {
          if( ! sd.exists(active_filename) ) {
            mySerial.print("Logging to ");
            mySerial.println(active_filename);
            return;
          }
        }
      }
    }
  }
#endif
}

// this is called periodically to flush full buffers to SD
void Storage::loop(void) {
  // Loop a finite number of times - 
  // if we're swamped, better to occasionally come up for air
  // and see if there is input waiting to stop the process
  for( uint16_t write_loops=0; 
       (fullHead != fullTail) && write_loops <  BUFFER_BLOCK_COUNT;
       write_loops++ ) {
    // block to write
    block_t* block = fullQueue[fullTail];
    sync_counter++;

    if( log_to_serial ) {
      for(int fidx=0;fidx<block->frame_count;fidx++) {
        mySerial.print(STREAM_START_LINE);
        for(int i=0;i<frame_bytes;i++){
          uint8_t byte=block->data[fidx*frame_bytes+i];
          // unfortunately, Serial drops leading 0, so manually pad each byte to 2 hex digits.
          if( byte<0x10 ) 
            mySerial.print("0");
          mySerial.print(byte,HEX);
        }
        mySerial.println();
      }
    } else {
#ifndef DISABLE_STORE
      if (!myFile.write((void*)block,sizeof(block_t))) {
        sd.errorHalt("failed to write");
      }
#endif
    }
    frame_count += block->frame_count;
    // check for overrun - doesn't count them, just flags
    // that there were some.
    if (block->flags & FLAG_OVERRUN) {
      overruns += 1;
    }
    // move block to empty queue
    emptyQueue[emptyHead] = block;
    emptyHead = queueNext(emptyHead);
    fullTail = queueNext(fullTail);

#ifndef DISABLE_STORE
    if ( !log_to_serial && (sync_counter > sync_interval_blocks) ) {
      myFile.sync();
      sync_counter=0;
    }
#endif
  }
}

// call this when done with sampling, and the sample ISR
// is no longer running.
void Storage::cleanup(void) {
  // on exit from sample loop
  if (isrBuf != 0) {
    close_block();
  }
  loop(); // write any straggling data
#ifndef DISABLE_STORE
  if ( !log_to_serial )
    myFile.close();
#endif
}


/** call this when a sample has been converted - will queue it in the
    right buffer.  safe for calling inside ISR */
void Storage::store_frame(uint8_t *frame) {
  // experiment to get low-latency serial output
  // if there is no backlog of blocks to be written, then
  // go ahead and flush this block so it can be written
  // sooner
  if ( isrBuf && log_to_serial && (fullHead == fullTail) ) {
    close_block();
  }
  // Get an appropriate block

  // switch to a data block if necessary
  if( isrBuf && ( (isrBuf->flags&FLAG_TYPE_MASK) == FLAG_TYPE_TEXT) ) {
    close_block();
  }
  if ( !isrBuf ) {
    open_block(FLAG_TYPE_DATA);
    if(!isrBuf) return;
  }

  memcpy(&(isrBuf->data[isrBuf_pos]),
         frame,
         frame_bytes);
  isrBuf_pos+=frame_bytes;
  isrBuf->frame_count++;

  // if no room for another frame, mark this one full
  if (DATA_DIM < isrBuf_pos+frame_bytes ) {
    close_block();
  }
}

// only call when isrBuf==0 !
void Storage::open_block(uint8_t flags) {
  if (emptyHead != emptyTail) {
    // remove buffer from empty queue
    isrBuf = emptyQueue[emptyTail];
    emptyTail = queueNext(emptyTail);
    // initialize block:
    isrBuf->frame_count = 0; // no frames
    isrBuf->unixtime=logger.unixtime;
    isrBuf_pos=0;
#ifdef RTC_ENABLE
    isrBuf->ticks=logger.rtc_pulse_count;
#endif
    isrBuf->flags=flags;
  } else {
    // no buffers - count overrun
    if (isrOver < 0XFF) isrOver++;
  }
}

// only call when isrBuf!=0
void Storage::close_block(void){
  // put buffer isrIn full queue
  if( isrBuf_pos < DATA_DIM ) {
    memset(isrBuf->data+isrBuf_pos,0,DATA_DIM-isrBuf_pos);
  }

  fullQueue[fullHead] = isrBuf;
  fullHead = queueNext(fullHead);
    
  //set buffer needed and clear overruns
  isrBuf = 0;
  isrBuf_pos=0;
  isrOver = 0;
}

size_t Storage::write(uint8_t b) {
  // switch to a text block if necessary
  if( isrBuf && (isrBuf->flags&FLAG_TYPE_MASK == FLAG_TYPE_DATA) ) {
    close_block();
  }

  if ( !isrBuf ) 
    open_block(FLAG_TYPE_TEXT);
  if ( !isrBuf ) return 0;

  isrBuf->data[isrBuf_pos++] = b;
  // last byte should be left 0 as null termination for the
  // string
  if( isrBuf_pos>=DATA_DIM-1 ) {
    isrBuf_pos=DATA_DIM-1; // just in case isrBuf_pos got crazy
    close_block();
  }
}

uint8_t Storage::send_data(const char *filename,uint32_t start,uint32_t bytes) {
  // send a portion of a file over the serial link
  // the data are hex-encoded, and
  // after the data, an additional 8-bit checksum is sent, also hex-encoded.
  

  // returns 0 on success
  uint8_t checksum=0;
  
  if (!myFile.open(filename, O_READ )) {
    mySerial.print("Failed to open ");
    mySerial.println(filename);
    return 1;
  }
  myFile.seekSet(start);

  if ( bytes==0 ) {
    bytes=myFile.fileSize();
  }

  uint8_t c;

  for(uint32_t i=0;i<bytes;i++) {
    myFile.read(&c,1);
    if( c<0xF ) mySerial.write('0');
    mySerial.print(c,HEX);
    if((i>0) && ((i & 0x3F) == 0x3F) ) 
      mySerial.println("");
    checksum+=c;
  }
  mySerial.println("");
  if( checksum<0xF ) mySerial.write('0');
  mySerial.println(checksum,HEX);
  
  myFile.close();
  return 0;
}
