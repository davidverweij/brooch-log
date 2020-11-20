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

/* ----------- SD CARD ----------- */
#include "SdFat.h"          // alternative (and faster?) lib for SD
byte SDcard_present;            // sleep and restart? to get the SD card running.
const uint8_t chipSelect = 10;  // chip select for SD card is 10 on the tinyduino
SdFat sd;                       // the file to write the logs to
SdFile file;                    // the file to write the logs to
#define FILE_BASE_NAME "Brooch" // Log file base name.  Must be six characters or less.
#define error(msg) sd.errorHalt(F(msg)) // Error messages stored in flash. (not sure how it's usefull)



#define DEBUG false          // for debugging only - set false for deployment. 

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

  if (DEBUG) Serial.println("Initializing SD card");delay(10);

  /* ----------- SD Card ----------- */
  const uint8_t BASE_NAME_SIZE = sizeof(FILE_BASE_NAME) - 1;
  char fileName[13] = FILE_BASE_NAME "00.csv";                // setting name convention
  if (!sd.begin(chipSelect, SD_SCK_MHZ(50))) {                // check if SD card responds
    sd.initErrorHalt();
    if (DEBUG) Serial.println("too high SD clock speed"); delay(10);
  }

  if (BASE_NAME_SIZE > 6) {               // Find an unused file name.
    error("FILE_BASE_NAME too long");
  }
  while (sd.exists(fileName)) {
    if (fileName[BASE_NAME_SIZE + 1] != '9') {
      fileName[BASE_NAME_SIZE + 1]++;
    } else if (fileName[BASE_NAME_SIZE] != '9') {
      fileName[BASE_NAME_SIZE + 1] = '0';
      fileName[BASE_NAME_SIZE]++;
    } else {
      error("Can't create file name");
    }
  }
  if (!file.open(fileName, O_WRONLY | O_CREAT | O_EXCL)) {
    error("file.open");
    if (DEBUG) Serial.println("Cannot open SD Card"); delay(10);
  } else {
    if (DEBUG) Serial.println("SD card succesfully connected");delay(10);
  }

  file.println(F("on (1) / off (2);unixtimestamp;timestamp"));     // write the .csv header

  RTC.start(); // ensure RTC oscillator is running, if not already

  //      upload the following line uncommented to reset the RTC timer.
  //      upload immediately after WITH THIS LINE COMMENTED to prevent this
  //      being done everytime the board boots!
  
  RTC.readTime();
  if (DEBUG) Serial.print("The time read back from RTC is : ");
  if (DEBUG) Serial.print(printableTime());
  if (DEBUG) Serial.println();

  RTC.enable_interrupt();
}

void loop() {
  if (DEBUG) Serial.println("Sleep"); Serial.flush();delay(10);


  RTC.snooze(standardSnoozeTime);

  if (DEBUG) Serial.flush(); RTC.readTime(); Serial.print("Woke up at "); Serial.print(printableTime()); Serial.println();

  buttonState = digitalRead(buttonPin);
  if (buttonState != previousButtonState) {   //  If true, the button state changed!

    if (DEBUG) Serial.println(" - Found a change in button state! Let's double check."); Serial.flush(); delay(10);

    RTC.snooze(doublecheckSnoozeTime);

    if (DEBUG) Serial.flush(); 

    int tempState = digitalRead(buttonPin);

    if (buttonState == tempState) {   // this means the change is persistent after a short snooze
      String nowString = printableTime();
      
      if (DEBUG) Serial.flush(); RTC.readTime(); Serial.print("Persistent Change at "); Serial.print(nowString); Serial.print(" with Button now at "); Serial.print(buttonState); Serial.println();delay(10);
      
      logBrooch(nowString, tempState); // storing to SD card
      if (DEBUG) Serial.println("Stored occurance on SD card"); delay(10);
    }

    buttonState = tempState;   // whether or not the change is persistent, we can update the values.
    previousButtonState = buttonState;
  }

}

String printableTime() {
  // Print a formatted string of the current date and time.

  String timestring = String(RTC.getMonths());
  timestring += F("/");
  timestring += String(RTC.getDays());
  timestring += F("/");
  timestring += String(RTC.getYears());
  timestring += F("  ");
  timestring += String(RTC.getHours());
  timestring += F(":");
  timestring += String(RTC.getMinutes());
  timestring += F(":");
  timestring += String(RTC.getSeconds());

  return timestring;
}

void logBrooch(String nowString, int tempState) {
  
  file.print(String(tempState));
  file.print(F(";"));
  file.print(String(RTC.date_to_epoch_seconds()));
  file.print(F(";"));
  file.print(nowString);
  file.println();

  if (!file.sync() || file.getWriteError()) {     // Force data to SD and update the directory entry to avoid data loss.
    error("write error");
  }
}
