MATLAB support scripts
---

**fb_parse.m** decodes the binary data files stored on the freebird logger into a Matlab struct.

**fb_comm.m** can send and receive commands to the freebird over a serial connection.  Precise 
clock synchronization is the main use case.  Manually setting the clock can be accurate to within a second
or two, but this programmatic approach gets the offset down to about 60ms.

**fb_common.m** provides a few constants used by the above scripts.

