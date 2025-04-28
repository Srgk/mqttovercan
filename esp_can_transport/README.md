# ESP-IDF env setup

* Go to esp-idf/v4.4.8 
* . ./export.sh
* code - to launch IDE

# Unit tests
 'idf.py -T all build' - doesn't work for some reason.
 To test cd to unit-app , idf.py app && idf.py flash && idf.py monitor . Terminal will hang.

# ESPHOME Notes
Setting ISP-IDF options: 
  Setting options through sdkconfig_options in esphome yaml - didn't work (At least for CONFIG_LOG_MAXIMUM_LEVEL)
  `pio run -t menuconfig` in build folder where platform.ini - worked