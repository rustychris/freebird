#ifndef __SDFUNCTIONS_H__
#define __SDFUNCTIONS_H__

// options for c
//  'E' erase only
//  'F' erase and format
//  'Q' quick format only

#include <Print.h>

#include <SdFat.h>
extern SdFat sd;



class Storage : public Print {
public:
  enum SdStatus {DISABLED,NOCARD,ENABLED};
  SdStatus status;

  uint16_t frame_bytes;
  Storage(void) { log_to_serial=false; frame_bytes=2; sync_interval_blocks=30; }

  virtual size_t write(uint8_t b);

  // called once on system startup
  void begin(void);

  void format(char c);

  // these are more like sub-setup, sub-loop, etc.
  // to be called from the sample loop
  void setup(void);
  void loop(void);
  void cleanup(void);

  void info(Print &);

  void store_frame(uint8_t *frame);
  void open_block(uint8_t flags);
  void close_block(void);

  void open_next_file(void);
  void set_next_active_filename(void);
  // DATAnnnn.BIN
  char active_filename[13];

  uint8_t send_data(const char *filename,uint32_t start=0,uint32_t bytes=0);

  // incremented by the number of samples the
  // ISR couldn't write because there were no more free buffer blocks.
  uint32_t overruns;
  uint32_t frame_count; // running count of frames actually written out 
  bool log_to_serial;// write samples to Serial instead of SD
  
  // when to call sync:
  uint16_t sync_interval_blocks,sync_counter;
};

#endif // __SDFUNCTIONS_H__
