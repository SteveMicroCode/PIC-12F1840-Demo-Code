// --------------------------------------------
// Simple test code for the PIC 12F1840 which flashes an LED 
// LED is on pin 5, RA2, connected via 560R resistor
// Standard Pickit3 ICSP connections, MCLR to +5V via 10K resistor
// Author: Steve Williams 18/05/2023
// --------------------------------------------

#include <xc.h>                   // Must include xc.h for all PICs when using xc8 compiler

// PIC12F1840 Configuration Bit Settings:

// CONFIG1
#pragma config FOSC = INTOSC    // Oscillator Selection (INTOSC oscillator: I/O function on CLKIN pin)
#pragma config WDTE = OFF       // Watchdog Timer Enable (WDT disabled)
#pragma config PWRTE = OFF      // Power-up Timer Enable (PWRT disabled)
#pragma config MCLRE = ON       // MCLR Pin Function Select (MCLR/VPP pin function is MCLR)
#pragma config CP = OFF         // Flash Program Memory Code Protection (Program memory code protection is disabled)
#pragma config CPD = OFF        // Data Memory Code Protection (Data memory code protection is disabled)
#pragma config BOREN = ON       // Brown-out Reset Enable (Brown-out Reset enabled)
#pragma config CLKOUTEN = OFF   // Clock Out Enable (CLKOUT function is disabled. I/O or oscillator function on the CLKOUT pin)
#pragma config IESO = OFF       // Internal/External Switchover (Internal/External Switchover mode is disabled)
#pragma config FCMEN = OFF      // Fail-Safe Clock Monitor Enable (Fail-Safe Clock Monitor is disabled)
// CONFIG2
#pragma config WRT = OFF        // Flash Memory Self-Write Protection (Write protection off)
#pragma config PLLEN = ON       // PLL Enable (4x PLL enabled)
#pragma config STVREN = ON      // Stack Overflow/Underflow Reset Enable (Stack Overflow or Underflow will cause a Reset)
#pragma config BORV = LO        // Brown-out Reset Voltage Selection (Brown-out Reset Voltage (Vbor), low trip point selected.)
#pragma config DEBUG = OFF      // In-Circuit Debugger Mode (In-Circuit Debugger disabled, ICSPCLK and ICSPDAT are general purpose I/O pins)
#pragma config LVP = OFF        // Low-Voltage Programming Enable (High-voltage on MCLR/VPP must be used for programming)

 // Single line config setup can be used: #pragma config FOSC=INTOSC,etc,....

#define _XTAL_FREQ 32000000      // Define clock frequency used by xc8 __delay(time) functions

void main(void)
{                            // First set OSCCON to get 32 MHz maximum clock rate using the 4x PLL multiplier
    OSCCON = 0b11110000;     // SPLLEN = 4x PLL enable(b7,nb config overrides), IRCF=1110 (b6..3), SCS=00(1..0))
    PORTA = 0;               // Clear the outputs first
    TRISA = 0b11111011;      // Set TRIS, make RA2 an output to drive LED, bit 2 is cleared
    while(1)
    {
      __delay_ms(1000);            // Output alternating short(0.1 sec) then long(1sec) LED flashes
      RA2 = 1;                     // to indicate PIC is alive ... "Hello World"
      __delay_ms(100);             // This delay is 100 msec, uses inline compiler function
      RA2 = 0;
      __delay_ms(500);             // 0.5 sec
      RA2 = 1;
      __delay_ms(1000);            // 1 sec
      RA2 = 0;
    }
}

