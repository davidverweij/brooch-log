/*
  This code will periodically wake up the processor to check whether a pin is connected or not.
  If connected, it wakes up sooner to double check if this value is stable. If stable,
  it will write this as a data entry into an SD card.

  This approach ('polling') is preferred to interrupting on (dis)connects regarding power consumption.

  Based on TinyCircuits example code for the RTC (by Ben Rose, Hunter Hykes) and prior code from Nantia Koulidou.

  Modified 30 October 2020
  By David Verweij

  set DEBUG to 'true' if you want to have a serial output! Set to false for deployment (saves energy and time)

*/

#include "DSRTCLib.h"
#include <Wire.h>           // to communicate with RTC
#include <avr/power.h>      // to go to sleep
#include <avr/sleep.h>      // to go to sleep

#define DEBUG true          // for debugging only - set false for deployment. 

int INT_PIN = 3;             // INTerrupt pin from the RTC. On Arduino Uno, this should be mapped to digital pin 2 or pin 3, which support external interrupts
int int_number = 1;          // On Arduino Uno, INT0 corresponds to pin 2, and INT1 to pin 3
const int buttonPin = 4;     // the number of the pushbutton pin
int buttonState = 2;         // is the button connected or disconnected
int previousButtonState = 2; // keeping track of the previous state (we only record if it changes)
                             // it is set to 2, so we always record the first state upon boot.

// standard time between checking the brooch connections.
// The higher, the less power is used, but also the less granular the data.

int standardSnoozeTime = 10;   // NOTE!  set to 120 for deployment
int doublecheckSnoozeTime = 2; // NOTE!  set to 10 for deployment


/* ----------- Real Time Clock ----------- */

//use this initialization to enable interrupts
DS1339 RTC = DS1339(INT_PIN, int_number);
//INT_PIN is INPUT for Interrupt
//int_number enables a software pullup on RTC Interrupt pin



void setup() {

  //INTERRUPTS
  digitalWrite(INT_PIN, HIGH);
  pinMode(buttonPin, INPUT_PULLUP);

  //enable deep sleeping
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();

  if (DEBUG) Serial.flush();     //clear serial line
  if (DEBUG) Serial.begin(9600); //start serial comms
  if (DEBUG) Serial.println ("Welcome to debugging logger.");
  if (DEBUG) Serial.flush(); 

  RTC.start(); // ensure RTC oscillator is running, if not already

  RTC.readTime();
  if (DEBUG) Serial.print("The time read back from RTC is : ");
  if (DEBUG) printTime();
  if (DEBUG) Serial.println();

  RTC.enable_interrupt();
}

void loop() {
  if (DEBUG) Serial.println("Sleep"); Serial.flush();delay(10);


  RTC.snooze(standardSnoozeTime);

  if (DEBUG) Serial.flush(); RTC.readTime(); Serial.print("Woke up at "); printTime(); Serial.println();

  buttonState = digitalRead(buttonPin);
  if (buttonState != previousButtonState) {   //  If true, the button state changed!

    if (DEBUG) Serial.println(" - Found a change in button state! Let's double check."); Serial.flush(); delay(10);

    RTC.snooze(doublecheckSnoozeTime);

    if (DEBUG) Serial.flush(); 

    int tempState = digitalRead(buttonPin);

    if (buttonState == tempState) {   // this means the change is persistent after a short snooze
      if (DEBUG) Serial.flush(); RTC.readTime(); Serial.print("Persistent Change at "); printTime(); Serial.print(" with Button now at "); Serial.print(buttonState); Serial.println();
      // store to SD card here!
    }

    buttonState = tempState;   // whether or not the change is persistent, we can update the values.
    previousButtonState = buttonState;
  }

}




void printTime() {
  // Print a formatted string of the current date and time.

  Serial.print(int(RTC.getMonths()));
  Serial.print("/");
  Serial.print(int(RTC.getDays()));
  Serial.print("/");
  Serial.print(RTC.getYears());
  Serial.print("  ");
  Serial.print(int(RTC.getHours()));
  Serial.print(":");
  Serial.print(int(RTC.getMinutes()));
  Serial.print(":");
  Serial.print(int(RTC.getSeconds()));
}
