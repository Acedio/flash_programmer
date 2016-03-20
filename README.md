# Flash Programmer

Simple programmer for the 29F020 flash ROM. Written to burn ROMs for an NES
cart, which you can find more info about
[here](http://acedio.github.io/ReplacinganNESPRGROMwithaFlashROM).

The firmware barely squeezes onto an ATTiny2313. It'll write 256KiB ROMs in
about 6 minutes or so, which is a heck of a lot better than the [previous
implementation](http://github.com/acedio/eeprom-programmer) which came in at
about 4 times that.
