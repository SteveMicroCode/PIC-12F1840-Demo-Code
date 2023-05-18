# PIC-12F1840-Demo-Code
A collection of code for the PIC 12F1840, more power than the 12F675 in the same space!
Having coded a little for the PIC 12F675 in C it was obvious that it's brain power is relatively limited.
The overhead when using C/Microchip XC8 really requires something with a little more power for most projects.
Research indicated that the PIC 12F1840 is far more capable, it has more program memory and
some useful peripherals including hardware serial. When purchased on Mouser it cost less than my 
stock of 12F675s, rendering that part pretty redundant. It is essentially pin compatible with the 675 albeit
with additional functions on many pins. 12F675 code requires modification as the i/o is now named PORTA and 
TRIS becomes TRISA. Oscillator configuration is a little different and requires setting OSCCON to configure
a chosen internal clock rate. All pretty simple though.

The first example code posted is a tiny "Hello World" program for Microchip XC8 that proves that the 12F1840 is alive. 
It demonstrates configuration for the much higher max clock rate of 32 MHz possible with this 12F chip.
