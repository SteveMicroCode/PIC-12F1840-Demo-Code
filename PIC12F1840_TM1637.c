// ---------------------------------------------------------------------
// Microchip 12F1840/TM1637 display code adapted by Steve Williams
// for Microchip's MPLAB XC8 compiler. The TM1637 routines were originally
// written by electro-dan for the BoostC compiler as part of project: 
// https://github.com/electro-dan/PIC12F_TM1637_Thermometer. That project used
// a PIC12F675 and I have ported the original code for the PIC12F1840 here. 
// The code produces an incrementing count on a 4 digit TM1637 display module.
// No warranty is implied and the code is for test use at users own risk. 
// 
// Hardware configuration for the PIC 12F1840:
// RA0 = OUT: N/C
// RA1 = OUT: N/C
// RA2 = OUT: N/C
// RA3 = OUT: N/C
// RA4 = IN/OUT: TM1637 DIO
// RA5 = IN/OUT: TM1637 CLK
// -----------------------------------------------------------------------


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

// Set the TM1637 module data and clock pins:
#define trisConfiguration 0b00110000; // This config ONLY TM1637 RA4/5 pins are inputs, TM1637 module pullups will take high
#define tm1637dio RA4                 // Set the i/o ports names for TM1637 data and clock here
#define tm1637dioTrisBit 4            // This is the bit shift to set TRIS for GP4
#define tm1637clk RA5
#define tm1637clkTrisBit 5

//Variables:

const uint8_t tm1637ByteSetData = 0x40;        // 0x40 [01000000] = Indicate command to display data
const uint8_t tm1637ByteSetAddr = 0xC0;        // 0xC0 [11000000] = Start address write out all display bytes 
const uint8_t tm1637ByteSetOn = 0x88;          // 0x88 [10001000] = Display ON, plus brightness
const uint8_t tm1637ByteSetOff = 0x80;         // 0x80 [10000000] = Display OFF 
const uint8_t tm1637MaxDigits = 4;
const uint8_t tm1637RightDigit = tm1637MaxDigits - 1;
                                               // Used to output the segment data for numbers 0..9 :
const uint8_t tm1637DisplayNumtoSeg[] = {0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f};
uint8_t tm1637Brightness = 5;                           // Range 0 to 7
uint8_t tm1637Data[] = {0, 0, 0, 0}; //Digit numeric data to display,array elements are for digits 0..3
uint8_t decimalPointPos = 99;        //Flag for decimal point (digits counted from left),if > MaxDigits dp off
uint8_t zeroBlanking = 1;            //If set true blanks leading zeros

//Function prototypes:
void initialise(void);
void tm1637StartCondition(void);
void tm1637StopCondition(void);
uint8_t tm1637ByteWrite(uint8_t bWrite);
void tm1637UpdateDisplay(void);
void tm1637DisplayOn(void);
void tm1637DisplayOff(void);
uint8_t getDigits(uint16_t number);   //Extracts decimal digits from integer, populates tm1637Data array


void main(void)
{
  uint16_t displayedInt=0;  // Beware 65K limit if larger than 4 digit display,consider using uint32_t
  uint16_t ctr = 0;
  initialise();             // Will initialise the 12F1840 with 32MHz clock, TRIS configured,ADC disabled
  __delay_ms(100);
  getDigits(displayedInt);
  tm1637UpdateDisplay();
  while(1)
    {
      displayedInt ++;
      if (displayedInt>9999)
            displayedInt = 0;
      if (getDigits(displayedInt))
      { 
        __delay_ms(1000);
        tm1637UpdateDisplay();
      }
    }
}

/*********************************************************************************************
 tm1637UpdateDisplay()
 Publish the tm1637Data array to the display
*********************************************************************************************/
void tm1637UpdateDisplay()
{   
    uint8_t tm1637DigitSegs = 0;
    uint8_t ctr;
    uint8_t stopBlanking = !zeroBlanking;            // Allow blanking of leading zeros if flag set
            
    // Write 0x40 [01000000] to indicate command to display data - [Write data to display register]:
    tm1637StartCondition();
    tm1637ByteWrite(tm1637ByteSetData);
    tm1637StopCondition();

    // Specify the display address 0xC0 [11000000] then write out all 4 bytes:
    tm1637StartCondition();
    tm1637ByteWrite(tm1637ByteSetAddr);
    for (ctr = 0; ctr < tm1637MaxDigits; ctr ++)
    {
        tm1637DigitSegs = tm1637DisplayNumtoSeg[tm1637Data[ctr]];
        if (!stopBlanking && (tm1637Data[ctr]==0))  // Blank leading zeros if stop blanking flag not set
            {
               if (ctr < tm1637RightDigit)          // Never blank the rightmost digit
                  tm1637DigitSegs = 0;              // Segments set 0x00 gives blanked display numeral
            }
        else
        {
           stopBlanking = 1;                    // Stop blanking if have reached a non-zero digit
           if (ctr==decimalPointPos)            // Flag for presence of decimal point, digits 0..3
           {                                    // No dp display if decimalPointPos is set > Maxdigits
               tm1637DigitSegs |= 0b10000000;   // High bit of segment data is decimal point, set to display
           }
        }
        tm1637ByteWrite(tm1637DigitSegs);       // Finally write out the segment data for each digit
    }
    tm1637StopCondition();

    // Write 0x80 [10001000] - Display ON, plus brightness
    tm1637StartCondition();
    tm1637ByteWrite((tm1637ByteSetOn + tm1637Brightness));
    tm1637StopCondition();
}


/*********************************************************************************************
 tm1637DisplayOn()
 Send display on command
*********************************************************************************************/
void tm1637DisplayOn(void)
{
    tm1637StartCondition();
    tm1637ByteWrite((tm1637ByteSetOn + tm1637Brightness));
    tm1637StopCondition();
}


/*********************************************************************************************
 tm1637DisplayOff()
 Send display off command
*********************************************************************************************/
void tm1637DisplayOff(void)
{
    tm1637StartCondition();
    tm1637ByteWrite(tm1637ByteSetOff);
    tm1637StopCondition();
}

/*********************************************************************************************
 tm1637StartCondition()
 Send the start condition
*********************************************************************************************/
void tm1637StartCondition(void) 
{
    TRISA &= ~(1<<tm1637dioTrisBit);   //Clear data tris bit
    tm1637dio = 0;                     //Data output set low
    __delay_us(100);
}


/*********************************************************************************************
 tm1637StopCondition()
 Send the stop condition
*********************************************************************************************/
void tm1637StopCondition() 
{
    TRISA &= ~(1<<tm1637dioTrisBit);    // Clear data tris bit
    tm1637dio = 0;                      // Data low
    __delay_us(100);
    TRISA |= 1<<tm1637clkTrisBit;       // Set tris to release clk
    __delay_us(100);
    // Release data
    TRISA |= 1<<tm1637dioTrisBit;       // Set tris to release data
    __delay_us(100);
}


/*********************************************************************************************
 tm1637ByteWrite(char bWrite)
 Write one byte
*********************************************************************************************/
uint8_t tm1637ByteWrite(uint8_t bWrite) {
    for (uint8_t i = 0; i < 8; i++) {
        // Clock low
        TRISA &= ~(1<<tm1637clkTrisBit);       // Clear clk tris bit
        tm1637clk = 0;
        __delay_us(100);
        
        // Test bit of byte, data high or low:
        if ((bWrite & 0x01) > 0) {
            TRISA |= 1<<tm1637dioTrisBit;       // Set data tris 
        } else {
            TRISA &= ~(1<<tm1637dioTrisBit);    // Clear data tris bit
            tm1637dio = 0;
        }
        __delay_us(100);

        // Shift bits to the left:
        bWrite = (bWrite >> 1);
        TRISA |= 1<<tm1637clkTrisBit;           // Set tris so clk goes high
        __delay_us(100);
    }

    // Wait for ack, send clock low:
    TRISA &= ~(1<<tm1637clkTrisBit);       // Clear clk tris bit
    tm1637clk = 0;
    TRISA |= 1<<tm1637dioTrisBit;          // Set data tris, makes input
    tm1637dio = 0;
    __delay_us(100);
    
    TRISA |= 1<<tm1637clkTrisBit;          // Set tris so clk goes high
    __delay_us(100);
    uint8_t tm1637ack = tm1637dio;
    if (!tm1637ack)
    {
        TRISA &= ~(1<<tm1637dioTrisBit);   // Clear data tris bit
        tm1637dio = 0;
    }
    __delay_us(100);
    TRISA &= ~(1<<tm1637clkTrisBit);       // Clear clk tris bit, set clock low
    tm1637clk = 0;
    __delay_us(100);

    return 1;
}


/*********************************************************************************************
  Function called once only to initialise variables and
  setup the PIC registers
*********************************************************************************************/
void initialise()
{ 
    OSCCON = 0b11110000;     // SPLLEN (b7) set = 4x PLL enable (nb config setting will override)
    PORTA = 0;               // OSCCON IRCF=1110 (b6..3), gives 8MHz clock, x4 with PLL = 32 MHz. SCS=00(1..0)
    TRISA = trisConfiguration;      // Clear the PORTA outputs then set TRIS
    ANSELA = 0;                     // Configure A/D inputs as digital I/O
    CM1CON0 = 7;                    // Comparator off
    OPTION_REG = 0b10001000;        // Set bit 7, disable pullups, plus bit 3, prescaler not assigned Timer0
}


/*************************************************************************************************
 getDigits extracts decimal digit numbers from an integer for the display, note max displayed value is 
 9999 for 4 digit display, truncation of larger numbers. Larger displays: note maximum 65K as coded with 
 16 bit parameter - probable need to declare number as uint32_t if coding for a 6 digit display  
 ************************************************************************************************/

uint8_t getDigits(uint16_t number)
{ 
    int8_t ctr = (tm1637RightDigit);            // Start processing for the rightmost displayed digit
    for (uint8_t ctr2 = 0; ctr2 < tm1637MaxDigits; ctr2++)
    {
        tm1637Data[ctr2]=0;      //Initialise the display data array with 0s
    }
    while(number > 0)            //Do if number greater than 0, ie. until all number's digits processed
    {
        if (ctr >= 0)
        {
           uint16_t modulus = number % 10;      // Split last digit from number
           tm1637Data[ctr] = (uint8_t)modulus;  // Update display character array
           number = number / 10;                // Divide number by 10
           ctr --;                              // Decrement digit counter to process number from right to left
        }
        else
        {
           number = 0;                          // Stop processing if have exceeded display's no of digits
        }
    }
    return 1;
}