MATLAB support scripts
---

**fb_parse.m** decodes the binary data files stored on the freebird logger into a Matlab struct.  Just copy the DATAnnnn.BIN file from the Freebird SD card to your PC, and pass the filename to this command.  Note that this
does *not* include any calibration or de-emphasis.  The result is a *counts* field, and a corresponding *volts* field.  

**fb_comm.m** can send and receive commands to the freebird over a serial connection.  Precise 
clock synchronization is the main use case.  Manually setting the clock can be accurate to within a second
or two, but this programmatic approach gets the offset down to about 60ms.

**fb_common.m** provides a few constants used by the above scripts.
