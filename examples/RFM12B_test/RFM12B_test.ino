//--------------------------------------------------------------------------------------
// Ultra low power test for the Funkyv2; Sends an incrementing value and the VCC readout every 10 seconds
// harizanov.com
// GNU GPL V3
//--------------------------------------------------------------------------------------

 /* 
   I run this sketch with the following Atmega32u4 fuses
   low_fuses=0x7f
   high_fuses=0xd8
   extended_fuses=0xcd
   meaning:
   external crystal 8Mhz, start-up 16K CK+65ms; 
   Divide clock by 8 internally; [CKDIV8=0]  (We will start at 1Mhz since BOD level is 2.2V)
   Boot Reset vector Enabled (default address=$0000); [BOOTRST=0]
   Boot flsh size=2048K words
   Serial program downloading (SPI) enabled; [SPIEN=0]
   BOD=2.2V
*/

#include <avr/power.h>
#include <avr/sleep.h>

#include <JeeLib.h> // https://github.com/jcw/jeelib
#include "pins_arduino.h"

ISR(WDT_vect) { Sleepy::watchdogEvent(); } // interrupt handler for JeeLabs Sleepy power saving

#define myNodeID 27      // RF12 node ID in the range 1-30
#define network 210      // RF12 Network group
#define freq RF12_868MHZ // Frequency of RFM12B module

#define LEDpin 1


//###############################################################
//Data Structure to be sent
//###############################################################

 typedef struct {
  	  int temp;	// Temp variable
  	  int supplyV;	// Supply voltage
 } Payload;

 Payload temptx;


void setup() {
  
   
  // Because of the fuses, we are running @ 1Mhz now.  
  clock_prescale_set(clock_div_2);   //Speed up to 4Mhz so we can talk to the RFM12B over SPI

  pinMode(LEDpin,OUTPUT);
  digitalWrite(LEDpin,HIGH); 

  power_adc_disable();
  power_usart0_disable();
  //power_spi_disable();  /do that a bit later, after we power RFM12b down
  power_twi_disable();
   //Leave timer 0 going for delay() function
  power_timer1_disable();
   // power_timer2_disable();
  power_timer3_disable();
  power_usart1_disable();

  // Datasheet says that to power off the USB interface we have to do 'some' of: 
   //       Detach USB interface 
   //      Disable USB interface 
   //      Disable PLL 
   //      Disable USB pad regulator 

   // Disable the USB interface 
   USBCON &= ~(1 << USBE); 
    
   // Disable the VBUS transition enable bit 
   USBCON &= ~(1 << VBUSTE); 
    
   // Disable the VUSB pad 
   USBCON &= ~(1 << OTGPADE); 
    
   // Freeze the USB clock 
   USBCON &= ~(1 << FRZCLK); 
    
   // Disable USB pad regulator 
   UHWCON &= ~(1 << UVREGE); 
    
   // Clear the IVBUS Transition Interrupt flag 
   USBINT &= ~(1 << VBUSTI); 
    
   // Physically detact USB (by disconnecting internal pull-ups on D+ and D-) 
   UDCON |= (1 << DETACH); 

  digitalWrite(LEDpin,LOW);  


  rf12_initialize(myNodeID,freq,network); // Initialize RFM12 with settings defined above 
  // Adjust low battery voltage to 2.2V
  rf12_control(0xC000);
  rf12_sleep(0);                          // Put the RFM12 to sleep

  power_spi_disable();   

  Sleepy::loseSomeTime(10000);          // Allow some time for power source to recover    
}

void loop() {
  
  digitalWrite(LEDpin,HIGH);  
  power_adc_enable();
  temptx.supplyV = readVcc(); // Get supply voltage
  power_adc_disable();
  digitalWrite(LEDpin,LOW);  
  
  if (temptx.supplyV > 2700) {// Only send if enough "juice" is available
    temptx.temp++;
    rfwrite(); // Send data via RF 
  }

  for(int j = 0; j < 1; j++) {    // Sleep for j minutes
    Sleepy::loseSomeTime(10000); //JeeLabs power save function: enter low power mode for 60 seconds (valid range 16-65000 ms)
  }
}

//--------------------------------------------------------------------------------------------------
// Send payload data via RF
//--------------------------------------------------------------------------------------------------
static void rfwrite(){
      power_spi_enable();
      rf12_sleep(-1);              // Wake up RF module
      while (!rf12_canSend())
        rf12_recvDone();
      rf12_sendStart(0, &temptx, sizeof temptx); 
      rf12_sendWait(2);           // Wait for RF to finish sending while in standby mode
      rf12_sleep(0);              // Put RF module to sleep 
      power_spi_disable();      
}

  
//--------------------------------------------------------------------------------------------------
// Read current supply voltage
//--------------------------------------------------------------------------------------------------
 long readVcc() {
   long result;
   // Read 1.1V reference against Vcc
   ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);  // For ATmega32u4
   Sleepy::loseSomeTime(16);
   ADCSRA |= _BV(ADSC); // Convert
   while (bit_is_set(ADCSRA,ADSC));
   result = ADCL;
   result |= ADCH<<8;
   result = 1126400L / result; // Back-calculate Vcc in mV
   ADCSRA &= ~ bit(ADEN); 
   return result;
} 
//########################################################################################################################

