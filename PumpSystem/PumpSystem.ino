/////////////////////////////////////////////////////////////////
/*
This stand-alone program is part of AquaLife

AquaLife is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

AquaLife is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with AquaLife.  If not, see <http://www.gnu.org/licenses/>.
*/
/////////////////////////////////////////////////////////////////

/*
@Program: PumpSystem
@Version: 0.1 ALPHA
@Author: James Dudeck
@Project: AquaLife
@Description:

This software has been designed to run on the Arduino UNO,
but can be run on any arduino with pin changes. This program
has been designed to control a two pump system. The secondary
pump provides a special, and experimental, purpose. For single
pump systems with manual min/max overflows, you do not need
to wire the sump relay.

While this code may not be directly applicable to your grow
bed, you might find use of individual sections of code for
different parts of your system.
============================================================

For the main basin, we will be using a capacitive sensor
  in order to maintain a minimal water level. The capacitive
  sensor (18g solid copper wire) will be placed in the water,
  with the end of the sensor wire at the desired water level.
  
  Please see the Playground entry for the CapSense Library:
  http://playground.arduino.cc/Main/CapacitiveSensor?from=Main.CapSense
   
===============================<--Raft
    |\                    |
    | \ <- Sensor Wire     <--Overflow Bulkhead For Maximum Water Level
    |  \                  |
    |~~~~~~~~~~~~~~~~~~~~~|<--Minimum Water Level
    |                     |
    | ■ <- Output pump    |
     ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾
In this particular design, I have the output pump feeding a bed
  of bamboo grass. The output pump will only be on for the
  minute prior to the main basin being refilled, as the 
  bamboo basin is only there to suck up any excess nutrients
  that the plants in my grow bed did not use in that hourly
  cycle.
  
The main pump's duty cycle will be 10 minutes, every hour.
  The secondary pumps duty cycle will enable 1 minute prior 
  to the main pump being turned on. The capacitive sensor is
  only used in this design so that I do not over-flow the main
  holding tank.
  
My main holding tank is an actual aquarium, so I am going to
keep all of my modifications top-side :)

I will try to keep this as simple as possible, so that it may
be modified for other systems.
*/

#include <Time.h>
#include <CapacitiveSensor.h>
#include <EEPROM.h>
#include "EEPROMTemplate.h"
/*
==========================
Defines
==========================
*/
#define TIME_HEADER  "T"   // Header tag for serial time sync message
#define TIME_REQUEST  7    // ASCII bell character requests a time sync message 

//Pins to run the realys for the pump system
#define PUMP_MAIN_RELAY_PIN 12
#define PUMP_SUMP_RELAY_PIN 13

//Pins to control the capacitive sensor
#define LEVEL_SENSOR_OUTPUT_PIN 8
#define LEVEL_SENSOR_INPUT_PIN 14 //Analog

//The level at which we will recognize contact
#define LEVEL_SENSE_THRESHOLD 200 //Direct contact

/*
==========================
Classes/Structs
==========================
*/
class Pump
{
  const int pin;
  bool isEnabled;
  
  public:
  Pump(int pin = -1, bool enabled = false) : pin(pin)
  {
    if((this->isEnabled = enabled) == true)
      this->enable();
  }
  
  void enable()
  {
    digitalWrite(this->pin, HIGH);
    this->isEnabled = true;
  }
  
  bool enabled() { return this->isEnabled; }
  
  void disable()
  {
    digitalWrite(this->pin, LOW);
    this->isEnabled = false;
  }
};
/*
==========================
Constants
==========================
*/

//Delay the system at the end of each loop (ms)
const int SYSTEM_DELAY = 1000;

/* System defaults for time keeping etc */

//Set an EEPROM signature to detect if this is the first run
// WARNING !!
// NEVER write byte 0 of the EEPROM
const char EEPROM_SIG = 0x4A;

// Seconds since epoch
// 09/23/14 6:14PM EST
//   To set a different date copy the following output from:
//     (Linux) date +%s
//     (Windows) you're on your own
//     (MAC) try the linux command, or look up the manual for
//           darwin's date command.
const unsigned long DEFAULT_TIME = 1411496104;

//Only set this to true if your arduino will be connected to a pc
//that is running the serial script. Otherwise, time will be
//synced to current time (psudo-sync), so time *will* drift
//without either an RTC chip, hardware or Serial Sync Provider
const bool USE_SERIAL_TIME_SERVER = true;

/*
==========================
Declarations
==========================
*/
Pump pMain(PUMP_MAIN_RELAY_PIN);
Pump pSump(PUMP_SUMP_RELAY_PIN);

CapacitiveSensor cs_4_2 = CapacitiveSensor
                          (
                            LEVEL_SENSOR_OUTPUT_PIN,
                            LEVEL_SENSOR_INPUT_PIN
                          );
                          
time_t lastCycleTime;
time_t lastSyncTime;

bool inCycle;
bool timeSync;

/*
==========================
Functions
==========================
*/

void SetupTime()
{
  //read the first byte of the EEPROM
  char ebyte[1] = { 0x00 };
  EEPROM_read(0,ebyte);
  
  //if this byte does not match our signature,
  //launch with the default time until synced
  if(ebyte[1] != EEPROM_SIG) //Set default time
  {
    setTime(DEFAULT_TIME);
    EEPROM_write(1,(unsigned long)DEFAULT_TIME);
    EEPROM_write(0,EEPROM_SIG);
  }
  
  //Otherwise, set the last known time
  else
  {
    unsigned long last_time = 0;
    EEPROM_read(1,last_time);
    setTime(last_time);
  }
}

bool WaterLevelCheck()
{
  long sense = cs_4_2.capacitiveSensor(30);
  return (sense < LEVEL_SENSE_THRESHOLD);
}

void StartCycle()
{
  if(!inCycle)
  {
    pMain.enable();
    lastCycleTime = now();
    inCycle = true;
    
    //disable sump pump at the beginning of the cycle
    if(pSump.enabled())
      pSump.disable();
  }
}

void EndCycle()
{
  if(inCycle)
  {
    pMain.disable();
    inCycle = false;
  }
}

//We wont actually force a sync when requested.
//We will broadcast the need, and take care of it on the
//next iteration.
time_t RequestTimeSync() 
{ 
  timeSync = true; 
  return 0;
}

void SyncTime()
{
  if(USE_SERIAL_TIME_SERVER)
  {
    if(timeSync)
    {
      Serial.write(TIME_REQUEST);
      timeSync = false;
    }
    
    if(Serial.find(TIME_HEADER))
    {
      unsigned long syncTime = Serial.parseInt();
      
      if(syncTime >= DEFAULT_TIME)
        setTime(syncTime);
    }
  }
}
void UpdateCycle()
{
  //If the sump pump is enabled, check the water level of
  //the basin. Disable if needed.
  if(pSump.enabled())
  {
    if(!WaterLevelCheck())
      pSump.disable();
  }
  
  //check running time, if cycle is in progress
  if(inCycle)
  {
    unsigned char runningMinutes = (minute(now()) - minute(lastCycleTime));
    
    if(runningMinutes >= 10)
      EndCycle();
  }
  //Otherwise, start when the hour has changed
  else
  {
    if(hour(now()) != hour(lastCycleTime))
      StartCycle();
    
    //Otherwise, see if we need to start the sump
    else if((minute(now()) >= 59) && !pSump.enabled())
      pSump.enable();
  }
}

/*
==========================
Setup/Loop
==========================
*/

void setup() 
{
  
  if(USE_SERIAL_TIME_SERVER)
  {
    Serial.begin(9600);
    setSyncProvider(RequestTimeSync);
  }
  
  SetupTime();
  inCycle = false;
  StartCycle();
}

void loop() 
{
  // put your main code here, to run repeatedly:
  UpdateCycle();
  delay(SYSTEM_DELAY);
}
