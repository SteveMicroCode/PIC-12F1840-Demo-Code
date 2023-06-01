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

I have ported the TM1637 display driver code originally written for the 12F675
to the 12F1840 here. See also my project https://github.com/SteveMicroCode/PIC12F675-TM1637-Display-Code.
It is again a standalone C source file for XC8 though should port pretty easily to other compilers.
The 12F1840 code demonstrates setup of a 32MHz max clock speed with bit timing for the serial data
unaffected as it is achieved using the XC8 delay_us macro. An incrementing count 0-9999 is displayed. Due 
to the increased resources of this chip only approx 13% program memory is now required for driving the display 
so that significant application program and memory space remains available.

My analogue to digital conversion code written for the PIC12F675 has been ported to the PIC 12F1840 and enhanced.
The 12F1840 is a much improved chip for ADC work. In particular it has a fixed voltage reference (FVR) with 3 
optional reference voltages 2^10 (1024mV) .. 2^12 (4096mV). This simplifies ratiometric to mV conversions and coding
overhead is reduced cf the 12F675. Note that ADC setup on the 12F1840 is not a simple port of 12F675 code, there
is an extra register ADCON1 and the bitfields for ADC setup are modified. ADC reads in the example code are
at 1 second intervals with Timer1 configured for 50ms interrupt generation via a suitable preload. Timer1 setup
is as for the 12F875 though note that T1CON bit 7 is now significant and should not be accidental misconfigured.
Up to 4 analogue inputs can be configured as described in the code and the port read is switched on the fly as 
required. There is also a description of how to select the FVR voltage reference in the code comments. The example
configures 2 analogue ports and by default reads AN0 on pin 7 using a 4.096V reference. For this example only
approx 20% of program memory is used, given the increased resources of the 12F1840 and better ADC code efficiency.
There is therefore far greater free space available for application development when using the ADC and TM1637 display.
