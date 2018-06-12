# ROM-el 2364 Programmer

This is an Arduino sketch and companion Python application to read from and write to ROM-el 2364 devices that (as of June 2018) ship with the AT49F512 EEPROM chip. It should, however,also work with the AT49F001 EEPROM that was originally shipped on the ROM-el 2364.

The Python script was written and run under Linux, and may need some tweaks to run in Windows environments, such as changing the serial port in the init_arduino() routine from /dev/ttyACM0 to, i.e. COM1.
