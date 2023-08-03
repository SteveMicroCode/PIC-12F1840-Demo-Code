
// ---------------------------------------------------------------------
// The PIC12F1840/TM1637 display code was adapted by Steve Williams
// for Microchip's MPLAB XC8 compiler. The TM1637 routines were originally
// written by electro-dan for the BoostC compiler as part of project: 
// https://github.com/electro-dan/PIC12F_TM1637_Thermometer. That project used
// a PIC12F675 and I have ported the original code for the PIC12F1840 here. 
// 
// This code demonstrates use of the PICF1840 analogue to digital convertor (ADC)
// with display of results on the TM1637 display. It can read one of the 4 
// available analogue inputs AN0..3. Enable a pin as analogue input by setting it's
// bit eg.for 2 ports: ADCinputConfig = 0b00000011. The voltage reference used is configurable
// and this code uses the hardware fixed voltage reference (FVR). The FVR offers 3 voltages
// according to the span required, set uint8_t ADCrefSelect to configure. Pre-configured ADC
// channels may be selected on the fly in code, uint8_t ADCchannel controls.
//
// No warranty is implied and the code is for test use at users own risk. 
// 
// Hardware configuration for the PIC 12F1840:
// RA0 = AN0 analogue input 0
// RA1 = AN1 analogue input 1
// RA2 = OUT: LED via 560R resistor
// RA3 = OUT: N/C
// RA4 = IN/OUT: TM1637 DIO
// RA5 = IN/OUT: TM1637 CLK
// -----------------------------------------------------------------------


#include <xc.h>

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
#define trisConfiguration 0b00110000; // TM1637 GP4/5 pins are inputs, TM1637 module pullups 
                                      // will take these high. Configuration here of display pins ONLY 
#define tm1637dio RA4                 // Set the i/o ports names for TM1637 data and clock here
#define tm1637dioTrisBit 4            // This is the bit shift to set TRIS for GP4
#define tm1637clk RA5
#define tm1637clkTrisBit 5

//Timer1 definitions:
#define T1PRESCALE 0x03                // 2 bits control, 01 = 1:2 used for 8 MHz clk, 11 = 1:8 for 32 MHz
#define TIMER1ON 0x01                  // Used to set bit0 T1CON = Timer1 ON
// Timer1 setup values for 50ms interrupt using a preload:
#define TIMER1LOWBYTE 0x60             // 50000 cycles @ 1:8 prescale == 50ms.
#define TIMER1HIGHBYTE 0x3C            // Preload no delays = 65536-50000 = 15536 = 0x3C60

//General global variables:
volatile uint8_t timer1Flag = 0;               // Flag is set by Timer 1 ISR every 50ms
uint8_t ADCreadcounter = 0;                    // Counts intervals for ADC task in 50ms increments
uint8_t ADCreadStatus = 0;                     // Stage of ADC conversion task, 0 = not started
uint8_t LEDcounter = 0;                        // Used to time non-blocking LED flash in 50ms increments
uint8_t LEDonTime = 0;                         // If true LED flash routine is called, flashes N x 50ms 

//ADC definitions:
#define NOCONVERSION 0
#define STARTADCREAD 1
#define CONVERTING 2    

//ADC variables:
const uint8_t ADCinputConfig = 0b00000011; // Bit 0..4 set enables analogue input in PORTA, 
                                           // and is used to set both TRISA and ANSELA, here AN0..1 enabled


//Display variables:
const uint8_t tm1637ByteSetData = 0x40;        // 0x40 [01000000] = Indicate command to display data
const uint8_t tm1637ByteSetAddr = 0xC0;        // 0xC0 [11000000] = Start address write out all display bytes 
const uint8_t tm1637ByteSetOn = 0x88;          // 0x88 [10001000] = Display ON, plus brightness
const uint8_t tm1637ByteSetOff = 0x80;         // 0x80 [10000000] = Display OFF 
const uint8_t tm1637MaxDigits = 4;
const uint8_t tm1637RightDigit = tm1637MaxDigits - 1;
// Used to output the segment data for numbers 0..9 :
const uint8_t tm1637DisplayNumtoSeg[] = {0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f};
uint8_t tm1637Brightness = 5;         // Range 0 to 7
uint8_t tm1637Data[] = {0, 0, 0, 0};  //Digit numeric data to display,array elements are for digits 0..3
uint8_t decimalPointPos = 99;         //Flag for decimal point (digits counted from left),if > MaxDigits dp off// Digit flag for decimal point (digits counted from left),if > MaxDigits dp off
uint8_t zeroBlanking = 0;             // If set true blanks leading zeros
uint8_t numDisplayedDigits = 3;       // Limits total displayed digits, used after rounding a decimal value

// ISR Handles Timer1 interrupt:
void __interrupt() ISR(void);  // Note XC8 interrupt function setup syntax using __interrupt() + myisr()
void initialise12F1840(void);
void initialise12F1840ADC(uint8_t ADCrefSelect,uint8_t ADCchannel);      //Initialises the ADC
void setADCchannel(uint8_t ADCchannel);        // Sets the ADC channel in use, 0..3 are valid values
void LEDflash(void);
uint16_t readADC(uint8_t ADCrefSelect);        // Returns ADC Vin in mV, ie 5000 max if Vref if Vref = 5V
void tm1637StartCondition(void);
void tm1637StopCondition(void);
uint8_t tm1637ByteWrite(uint8_t bWrite);
void tm1637UpdateDisplay(void);
void tm1637DisplayOn(void);
void tm1637DisplayOff(void);
uint8_t getDigits(uint16_t number);   //Extracts decimal digits from integer, populates tm1637Data array
void roundDigits(void);


void main(void)
{
  uint8_t ADCchannel = 0;        // Active ADC channel, AN0 = 0..AN3 = 3, nb only 0 and 1 set up in this code
  const uint8_t ADCrefSelect = 0x03;  // Used to set FVR ADC ref volts ADFVR bits 1..0,nb ADC read/mV calc also uses
                                      // Valid values are 01=0x01: 1.024V, 10=0x02: 2.048V, 11=0x03: 4.096V
  uint16_t displayedInt=0;       // Beware 65K limit if larger than 4 digit display,consider using uint32_t
  uint16_t ctr = 0;
  
  _delay(100);
  initialise12F1840();
  initialise12F1840ADC(ADCrefSelect, ADCchannel);
  zeroBlanking = 0;              // Don't blank leading zeros
  decimalPointPos = 0;           // Display 0-5000mV as n.nnn volts, digit 0 = leftmost
  getDigits(displayedInt);
  tm1637UpdateDisplay();         // Display zero then start timed conversions, updating display as completed
  ADCreadcounter = 0;            // Start with timing counts at zero, both ADC read and timer1 flags
  timer1Flag = 0; 
  T1CON |= TIMER1ON;
  while(1)
    {
      if (timer1Flag)
        {
           ADCreadcounter ++;                    // Update task interval and LED timing flags
           LEDcounter ++;                           
           timer1Flag = 0;                       // Clear the 50ms timing flag
        }
      
      if (ADCreadcounter >= 20)                  // Start ADC read process every 50ms x 20 = 1 sec
        { 
           ADCreadcounter = 0;
           ADCreadStatus = STARTADCREAD;         // Setting to 1 = start of ADC read 
           LEDcounter = 0;                // Zero the LED time counter, note counts 50ms increments
           LEDonTime = 1;                 // Sets up a 500ms LED flash
        }
      
      switch (ADCreadStatus)             // The ADC read/display task is managed by ADCreadStatus control flag
      {
              case NOCONVERSION:
                  break;
                  
              case STARTADCREAD:                 // nb. must only start ADC conversions after Taq since last
                  setADCchannel(ADCchannel);     // Channels 0..3 available if configured during initialisation
                  ADCON0 |= 0x02;                        // Set GO/DONE, bit 1, to start conversion
                  ADCreadStatus = CONVERTING;
                  break;
                  
              case CONVERTING:                   // Polls GO/DONE for completed conversion,COULD ADD TIMEOUT?
                  if (!(ADCON0 & 0x02))
                  {   // Get the raw ratiometric ADC data converted to Vin in mV:
                      displayedInt = readADC(ADCrefSelect);       // Returns value as uint16_t
                      getDigits(displayedInt);   // Extract digit data from integer into 4x uint8_t array 
                      roundDigits();             // Apply rounding to the array data if <4 digits displayed
                      tm1637UpdateDisplay();
                      ADCreadStatus = NOCONVERSION;  // Consider adding a timed delay before reset this flag
                  }
                  break;
      }
             
      if (LEDonTime)                              // Call the LED flash function if a count is set
          LEDflash();   
    }                       //while(1)
}                           //main



//***************************************************************************************
// Interrupt service routine:
//***************************************************************************************

void ISR(void)
{
    if (PIR1 & 0x01)                  // Check Timer1 interrupt flag bit 0 is set
    {
        PIR1 &= 0xFE;                 // Clear interrupt flag bit 0
                                      // Reset Timer1 preload for 50ms overflow/interrupt(nb running timer)
        TMR1H = TIMER1HIGHBYTE;       // Note some timing inaccuracy due to interrupt latency + reload time
        TMR1L = TIMER1LOWBYTE;        // Correction was therefore applied to low byte to improve accuracy 
        timer1Flag = 1;               
        
    }
}

//*******************************************************************************************
//Functions: 
//*******************************************************************************************

void LEDflash(void)
{
    if (LEDcounter <= LEDonTime)
    {
        RA2 = 1;                      // LED on
    }
    else
    {
        RA2 = 0;                      // LED off
        LEDonTime = 0;                // Stop the flash
    }
}

//********************************************************************************************
// readADC() reads the 10 bit ratiometric ADCvalue (Vin/Vref). Integer arithmetic is then used 
// for conversion to a 16 bit unsigned integer value which is Vin in mV.
// This method suitable for 10 bit ADC conversions only. Conversion is very simple using FVR as Vref.
// The 12F1840 internal voltage ref (FVR)provides 3 choices of Vref, note powers of 2, simplifies integer maths
// Valid values for ADCrefSelect are 0x01,02,03.  Calculation:  ADCmV = VrefmV*ADCval/1024
// To divide by 1024 bitshift R x10, if Vref=1024 mV first a 10 bit left bitshift, hence net zero shift
// For Vref 4096mV left <<12, then R bitshift >>10, net bitshift <<2
// For Vcc 5V as ref use: uint32_t ADCmV = (uint32_t)5000 * (uint32_t)ADCval;  Note this multiply increases
// memory usage and requires use of uint32_t to store result of multiplication
// Follow this with ADCmV >>= 10;   to divide by 1024, result is the whole mV part of conversion, some data loss
//********************************************************************************************

uint16_t readADC(uint8_t ADCrefSelect)  // Returns a 16 bit unsigned integer, Vin in mV
{                                       // Very simple integer maths if use FVR as Vref, ie. 2^10mV .. 2^12mV
    ADCrefSelect --;                    // Valid values 0x 01,02,03, if we decrement this calculates a net bitshift
    uint16_t ADCval = ADRESL;           // ADC result is a 10 bit number, read lower 8 bits
    ADCval |= (uint16_t)ADRESH << 8;    // Get bits 8/9 of the result,storing as as 16 bit integer
    uint16_t ADCmV = ADCval << ADCrefSelect; // Apply the net bitshift 0->2 for mV conversion, zero for 1024mV             
    return(ADCmV);                      // Result OK as 16 bit integer as value in mV is less than 2^16(65536)  
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
        if (ctr>(numDisplayedDigits-1))
            tm1637DigitSegs = 0;             // Segments set 0x00 blanks,limits displayed digits left to right
        
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
    TRISA &= ~(1<<tm1637dioTrisBit);  //Clear data tris bit
    tm1637dio = 0;                     //Data output set low
    __delay_us(100);
}


/*********************************************************************************************
 tm1637StopCondition()
 Send the stop condition
*********************************************************************************************/
void tm1637StopCondition() 
{
    TRISA &= ~(1<<tm1637dioTrisBit);   // Clear data tris bit
    tm1637dio = 0;                      // Data low
    __delay_us(100);
    TRISA |= 1<<tm1637clkTrisBit;      // Set tris to release clk
    //tm1637clk = 1;
    __delay_us(100);
    // Release data
    TRISA |= 1<<tm1637dioTrisBit;      // Set tris to release data
    __delay_us(100);
}


/*********************************************************************************************
 tm1637ByteWrite(char bWrite)
 Write one byte
*********************************************************************************************/
uint8_t tm1637ByteWrite(uint8_t bWrite) {
    for (uint8_t i = 0; i < 8; i++) {
        // Clock low
        TRISA &= ~(1<<tm1637clkTrisBit);   // Clear clk tris bit
        tm1637clk = 0;
        __delay_us(100);
        
        // Test bit of byte, data high or low:
        if ((bWrite & 0x01) > 0) {
            TRISA |= 1<<tm1637dioTrisBit;      // Set data tris 
        } else {
            TRISA &= ~(1<<tm1637dioTrisBit);   // Clear data tris bit
            tm1637dio = 0;
        }
        __delay_us(100);

        // Shift bits to the left:
        bWrite = (bWrite >> 1);
        TRISA |= 1<<tm1637clkTrisBit;      // Set tris so clk goes high
        __delay_us(100);
    }

    // Wait for ack, send clock low:
    TRISA &= ~(1<<tm1637clkTrisBit);      // Clear clk tris bit
    tm1637clk = 0;
    TRISA |= 1<<tm1637dioTrisBit;         // Set data tris, makes input
    tm1637dio = 0;
    __delay_us(100);
    
    TRISA |= 1<<tm1637clkTrisBit;         // Set tris so clk goes high
    __delay_us(100);
    uint8_t tm1637ack = tm1637dio;
    if (!tm1637ack)
    {
        TRISA &= ~(1<<tm1637dioTrisBit);  // Clear data tris bit
        tm1637dio = 0;
    }
    __delay_us(100);
    TRISA &= ~(1<<tm1637clkTrisBit);      // Clear clk tris bit, set clock low
    tm1637clk = 0;
    __delay_us(100);

    return 1;
}


/*********************************************************************************************
  Function called once only to initialise variables and
  setup the PIC registers
*********************************************************************************************/
void initialise12F1840()
{   
    OSCCON = 0b11110000;     // SPLLEN (b7) set = 4x PLL enable (nb config setting will override)
    PORTA = 0;               // OSCCON IRCF=1110 (b6..3), gives 8MHz clock, x4 with PLL = 32 MHz. SCS=00(1..0)
    TRISA = trisConfiguration;     // All pins set as digital outputs other than AN4/5(TM1637)
    TRISA |= ADCinputConfig;       // Setting bit 0..4 sets digital i/o 0..3 to input(high impedance)
    CM1CON0 = 7;                   // Comparator off
    OPTION_REG = 0b10001000;       // Set bit 7, disable pullups, plus bit 3, prescaler not assigned Timer0
    
    // TIMER1 setup with interrupt:
    T1CON = 0;                     // Clear T1 control bits
    T1CON |= (T1PRESCALE<<4);      // Bits 4-5 set prescale, 01 = 1:2
    T1CON |= 0x04;                 // Bit 2 set enables disables external clock input 
    TMR1L = TIMER1LOWBYTE;         // Set Timer1 preload for 1ms overflow/interrupt
    TMR1H = TIMER1HIGHBYTE; 
    PIE1 = 0x01;                   // Timer 1 interrupt enable bit 1 set, other interrupts disabled
    PIR1 &= 0xFE;                  // Clear Timer1 interrupt flag bit 0
    INTCON |= 0xC0;                // Enable interrupts, general - bit 7 plus peripheral - bit 6 
}

/************************************************************************
 * ADC initialisation for the PIC12F1840, ADCrefSelect sets Vref
 ************************************************************************/
void initialise12F1840ADC(uint8_t ADCrefSelect,uint8_t ADCchannel)
{
    // ADC setup, note there are significant configuration register differences cf. 12F675 :
    FVRCON = 0x80;                // Fixed voltage reference on, use ADCON1 to select the ref source used
    FVRCON |= ADCrefSelect;       // Set FVR ADC ref volts, ADFVR bits 1..0, 01=1.024V, 10=2.048V, 11=4.096V
    ANSELA = ADCinputConfig;      // Bit 0..4 set enables analogue input, note b3 unimplemented, b4 for AN3
    ADCON0 = 0x01;                // ADC turned on (bit 0)
    ADCON0 |= ADCchannel<<2;      // Set the active ADC channel, bits 2..6 are CHS, 0 = AN0 ..3 = AN3
    ADCON1 = 0xA3; // ADFM b7 set = R justified. ADCS = 010, Tad = Fosc/32 = 1.0us @32MHz.Vref = Vdd, internal ref
}


/*************************************************************************************************
 * setADCchannel() sets the ADC channel in use, AN0..AN3. Channel = 0..3. Must configure i/o pin as an
 * analogue input before using the channel, TRIS and ANSELA bits are set using uint8_t ADCinputConfig
 * ***********************************************************************************************/
void setADCchannel(uint8_t ADCchannel)
{
   ADCON0 &= 0b10000011;         // First clear the channel select bits 2..6
   ADCON0 |= ADCchannel<<2;      // Set the active ADC channel, bits 2..6 are CHS, 0 = AN0 ..3 = AN3 
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

//*****************************************************************************************
// roundDigits applies decimal rounding to digit data stored in tm1637Data array
// processing the digit data from right (least significant) to left. As written 
// the function can only round for removal of single rightmost digit. Could be developed.
//*****************************************************************************************

void roundDigits(void)
{
    int8_t digit = tm1637RightDigit;        // Current digit being processed, 0..3 L->R
    uint8_t carry = 0;                      // Carry is set to add 1 to previous digit
    for (digit = tm1637RightDigit; digit >= 0; digit -- )
    {
        if (digit == tm1637RightDigit)
        {
            if (tm1637Data[digit]>5)                  // Round up if rightmost digit >5 by setting carry
                carry = 1;
            tm1637Data[digit] = 0;                    // Processed digit is set to zero
        }
        
        if (digit < tm1637RightDigit)
        {
            tm1637Data[digit] += carry;               // Add any carry back from previous rounding
            if (tm1637Data[digit]>9)
            {
                tm1637Data[digit]=0;
                carry = 1;                            // Set a carry back to previous digit
            }
            else
                carry = 0;
        }
    }
}

