/*
  This code is used for logging connects and disconnects of a brooch
  the code will sleep when possible, and awake and store these (dis)connects on an SD
  card. It uses an RTC (realtimeclock) to store the date and time of this happening.

  A lot is based on
  - https://thekurks.net/blog/2018/2/5/wakeup-rtc-datalogger (Data logger example)
  - http://www.gammon.com.au/forum/?id=11497 (Power Consumption)
  - SdFat library (Fast and reliable writing to SD - and Memory consumption)

  Original by Nantia Koulidou. Edit by David Verweij

  NOTES:

  set DEVUG to 'true' if you want to have a serial output! Set to false for deployment (saves energy and time)

  Note that each Serial.print (if DEBUG is true) is followed by a delay - to give the processor time to print it..
  Also, To conserve memory, we this sketch uses F("string"); to (serial)write text
  This makes the string be read from static memory, instead of dynamic (and saves on memory)

*/

#include <Wire.h>     // lib for RTC
#include <RTClib.h>   // lib for RTC
#include <SPI.h>      // lib for SPI communication
#include "SdFat.h"    // alternative (and faster?) lib for SD

#include <avr/sleep.h>  // include code to allow (deep) sleep mode
#include <avr/power.h>  // include code to allow (deep) sleep mode

#define DEBUG true   // for debugging only - set false for deployment. 

/* ----------- SD CARD ----------- */
byte SDcard_present;            // sleep and restart? to get the SD card running.
const uint8_t chipSelect = 10;  // chip select for SD card is 10 on the tinyduino
SdFat sd;                       // the file to write the logs to
SdFile file;                    // the file to write the logs to
#define FILE_BASE_NAME "Brooch" // Log file base name.  Must be six characters or less.
#define error(msg) sd.errorHalt(F(msg)) // Error messages stored in flash. (not sure how it's usefull)

/* ----------- Real Time Clock ----------- */
RTC_DS1307 RTC;

/* ----------- Interrupt ----------- */
const int     broochInterrupt = 3;     // the number of the brooch pin
volatile byte interruptType = 0;       // 0 is nothing, 1 is connect, 2 is disconnect   // volatile because in interrupt
unsigned long startTime = 0;           // we use a startTime and waitToSleep to wait a bit before going to sleep. Minimize this for power consumption
unsigned long waitToSleep = 2000;       // 2000 = 2 seconds // doesn't overrun, using an unsigned long. see http://www.gammon.com.au/millis

/* ----------- Setup: runs once ----------- */
void setup() {

  if (DEBUG) Serial.begin(9600);       // if Debugging, use Serial Monitor to observe.
  startTime = millis();

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
  }

  file.println(F("on (1) / off (2);unixtimestamp;timestamp"));     // write the .csv header


  /* ----------- RTC (realtimeclock) ----------- */
  Wire.begin();
  RTC.begin();

  if (!RTC.isrunning()) {
    if (DEBUG) Serial.println("RTC is NOT running!");
    RTC.adjust(DateTime(__DATE__, __TIME__));
    /* !!!!! this will 'reset' the RTC time to the time of uploading the code the arduino
      Even though this is wrong, we rather have keep track time, and that we can adjust
      than not keep time at all */
  }

  // upload the following line uncommented to reset the RTC timer.
  // Upload immediately after WITH THIS LINE COMMENTED to prevent this
  // being done everytime the board boots!

  // RTC.adjust(DateTime(__DATE__, __TIME__));

  /* ----------- Brooch Wires input ----------- */
  pinMode(broochInterrupt, INPUT_PULLUP);   // brooch button/pin
  attachInterrupt(digitalPinToInterrupt(broochInterrupt), interrupt, CHANGE);     // 0 is pin 2 - this is how all the examples code their interrupts

}

/* ----------- Loop - is executed over and over again ----------- */
void loop() {

  /* ----------- Check if there was an interrupt ----------- */
  if (interruptType > 0)
  {
    logBrooch();            // write to SD card
    interruptType = 0;      // reset interrupt variable
    startTime = millis();   // reset timer
  }
  else if (millis () - startTime >= waitToSleep) {  // if a time has passed without interrupts, go to sleep
    sleep();
  }
}

/* ----------- Brooch (dis)connected! this is automatically called. Cannot contain many operations! ----------- */
void interrupt() {
  if (DEBUG) Serial.print(F("INTERRUPT!")); delay(10);

  sleep_disable();      // to prevent the CPU to go to sleep for now, see https://playground.arduino.cc/Learning/ArduinoSleepCode/

  /* ----------- Determine if it was a dis-, or connect ----------- */
  if (digitalRead(broochInterrupt) == HIGH) interruptType = 1;  // Brooch is connected
  else interruptType = 2;                                       // Brooch is disconnected
}

/* ----------- Log this (dis)connect to the SD card ----------- */
void logBrooch() {
  DateTime now = RTC.now();
  file.print(String(interruptType));
  file.print(F(";"));
  file.print(String(now.unixtime()));
  file.print(F(";"));
  file.print(String(now.year())); file.print(F("-"));
  file.print(String(now.month())); file.print(F("-"));
  file.print(String(now.day())); file.print(F(" "));
  file.print(String(now.hour())); file.print(F(":"));
  file.print(String(now.minute())); file.print(F(":"));
  file.print(String(now.second()));
  file.println();

  if (DEBUG) Serial.print(F("wrote brooch.")); delay(10);

  if (!file.sync() || file.getWriteError()) {     // Force data to SD and update the directory entry to avoid data loss.
    error("write error");
  }
}

/* ----------- Sleep Procedure ----------- */
/*  code from https://playground.arduino.cc/Learning/ArduinoSleepCode/ and https://thekurks.net/blog/2018/1/24/guide-to-arduino-sleep-mode */
void sleep()
{
  if (DEBUG) Serial.println("To sleep"); delay(10);
  
  sleep_enable();                         // allow sleep method
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);    // Set sleep to full power down.  Only external interrupts can wake the CPU!
  cli();                                  // clear interrupt 'flags'
  sleep_bod_disable();                    // Turn of 'Brown-Out-Detection' while asleep (save some power);
  power_adc_disable();                    // Turn off the ADC while asleep. (save some power)
  sei();                                  // set interrupt 'flags'
  sleep_cpu();                            // When awake, disable sleep mode and turn on all devices.

  // CPU is now asleep and program execution completely halts!
  // Once awake, execution will resume at this point.
  // You could also power down the SD card module, but this might not be worth the effort (see https://forum.arduino.cc/index.php?topic=149504.0)

  /* ARDUINO WAKES UP HERE IF INTERRUPT */

  if (DEBUG) Serial.println("Woke up!"); delay(10);
  power_all_enable();                     // enable all arduino components (power on!)
}
