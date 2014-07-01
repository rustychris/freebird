import sys
import device
import compass_cal
import datetime

fb=device.FreebirdComm()
if not fb.connect(min_score=2):
    print "Failed to connect!"
    sys.exit(1)

fb.enter_command_mode()

cal=compass_cal.CompassCalibrator(fb)
cal.run()
fn="compass-%s-%s.dat"%(fb.query_serial(),datetime.datetime.now().strftime('%Y%m%dT%H%M'))
cal.save(fn)
