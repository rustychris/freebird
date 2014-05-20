import datafile
reload(datafile)
df=datafile.freebird_file_factory("../../../../../ct_river/data/2014-05/tioga_winch/freebird/20140514/afternoon/DATA0009.BIN")

df.read_all()
