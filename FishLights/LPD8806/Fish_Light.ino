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
@Program: Fish Lighting - LPD8806 Variant
@Version: 0.1 ALPHA
@Author: James Dudeck
@Project: AquaLife
@Description:

This software has been designed to run on the Arduino UNO,
but can be run on most Arduinos with no changes. This software
has been designed to work with LED strips that have embedded
chips that are compliant with the LPD8806 chip.

The Color class includes the logic of a 3rd party to convert
a color temperature, in Kelvin, to the related RGB values.
This code was converted from VB.

I personally used this design to emulate the light intensities
that the fish I was raising would likely see at the depths
they would naturally be found in.

  Two words of caution:
  1 (Arduino) - The TimeTriggers are saved in the EEPROM.
                Be sure to not over-flow your EEPROM, as
                the size will vary from model to model.
                
  2 (Lights) -  This setup uses the strips with the LPD8806
                (or compliant) chipset. This chipset has a
                7-bit resolution, which gives us 127 steps.
                With the LPD8806 variation of this light,
                if you try to increment or decrement with
                too high of a resolution (number of places
                beyond the decimal), it may do nothing.
*/

#include <LPD8806.h>
#include <SPI.h>
#include <Time.h>
#include <EEPROM.h>
#include "EEPROMTemplate.h"
#include "vector.h"

/*
==========================
Defines
==========================
*/
#define TIME_HEADER  "T"   // Header tag for serial time sync message
#define TIME_REQUEST  7    // ASCII bell character requests a time sync message 

/*
==========================
Classes/Structs
==========================
*/

/*
  The purpose for using the time trigger struct,
  is so that you can have total control over over
  the lights at specific times of the day, while 
  maintaining standard timer functionality. If 
  you want standard timer functionality, simply
  add a TimeTrigger for your on event, and one
  for your off event. On the other hand, if you
  wish to emulate sunrise, add a TimeTrigger, until
  noon, that increases brightness (0.1-0.9), then,
  repeate in the opposite direction until night-fall.
*/
struct TimeTrigger
{
  unsigned char tHour;
  unsigned char tMinute;
  double tIntensity;
  bool triggered;
};

struct Color
{
  unsigned char red;
  unsigned char green;
  unsigned char blue;
  
  Color(unsigned char red, unsigned char green, unsigned char blue)
  {
    this->red = red;
    this->green = green;
    this->blue = blue;
  }
  
  Color()
  {
    this->red = 0;
    this->green = 0;
    this->blue = 0;
  }
  
  //Convert Kelvin to RGB values
  // This is a direct conversion of VB code listed here:
  //   http://www.tannerhelland.com/4435/convert-temperature-rgb-algorithm-code/
  static Color FromKelvin(int value)
  {
    //Keep value between 1000K and 40000K
    value = (int)(value < 1000 ? 1000 : value);
    value = (int)(value > 40000 ? 40000 : value);
    
    //All calculations require tmpKelvin/100, so only do the conversion once
    float tmpKelvin = (value/100), tmpCalc = 0;
    long long conTmp = 0;
    char r = 0, b = 0, g = 0;
    
    //Red
    if(tmpKelvin <= 66)
      r = 255;
    else
    {
      //Note: the R-squared value for this approximation is .988
      tmpCalc = tmpKelvin - 60;
      tmpCalc = 329.698727446 * pow(tmpCalc, (float)(-0.1332047592));
      
      r = (int)tmpCalc;
      r = (char)(r < 0 ? 0 : r);
      r = (char)(r > 255 ? 255 : r);
    }
    
    //Green
    if(tmpKelvin <= 66)
    {
      //Note: the R-squared value for this approximation is .996
      tmpCalc = tmpKelvin;
      tmpCalc = 99.4708025861 * log(tmpCalc) - 161.1195681661;
      
      g = (int)tmpCalc;
      g = (char)(g < 0 ? 0 : g);
      g = (char)(g > 255 ? 255 : g);
    }
    else
    {
      //Note: the R-squared value for this approximation is .987
      tmpCalc = tmpKelvin - 60;
      tmpCalc = 288.1221695283 * pow(tmpCalc, -0.0755148492);
      
      g = (int)tmpCalc;
      g = (char)(g < 0 ? 0 : g);
      g = (char)(g > 255 ? 255 : g);
    }
    
    //Blue
    if(tmpKelvin >= 66)
      b = 255;
    else if(tmpKelvin <= 19)
      b = 0;
    else
    {
      //Note: the R-squared value for this approximation is .998
      tmpCalc = tmpKelvin - 10;
      tmpCalc = 138.5177312231 * log(tmpCalc) - 305.0447927307;
      
      b = (int)tmpCalc;
      b = (char)(b < 0 ? 0 : b);
      b = (char)(b > 255 ? 255 : b);  
    }
    
    return Color(r,g,b);
  }
};

struct Configuration
{
  Color color;  
  time_t lastTimeSync;
  time_t lastTriggerReset;
  Vector<TimeTrigger> triggers;
} configuration;

/*
==========================
Constants
==========================
*/
const char EEPROM_SIG = 0x4A;
const unsigned long DEFAULT_TIME = 1411496104;
const bool USE_SERIAL_TIME_SERVER = true;

/*
==========================
Declarations
==========================
*/
LPD8806 strip = LPD8806(150);

time_t lastUpdate = 0;
time_t t = 0;

bool timeSync;

/*
==========================
Functions
==========================
*/

//Either load the last known time or set the default time
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

//Handle the time sync, when we receive the token
void SyncTime()
{
  if(USE_SERIAL_TIME_SERVER)
  {
    if(timeSync)
    {
      if(Serial.find(TIME_HEADER))
      {
        unsigned long syncTime = Serial.parseInt();
        
        if(syncTime >= DEFAULT_TIME)
          setTime(syncTime);
          
        EEPROM_write(1, syncTime);
        timeSync = false;
      }
    }
  }
}
//We wont actually force a sync when requested.
//We will broadcast the need, and take care of it on the
//next iteration.
time_t RequestTimeSync() 
{
  Serial.write(TIME_REQUEST);
  timeSync = true; 
  return 0;
}

//Sorts through the triggers, looking for events that
//have not been fired.
bool CheckTriggers()
{
  t = now();
  
  //Check for trigger reset
  if(day(t) != day(configuration.lastTriggerReset))
    ResetTriggers();
  
  int count = configuration.triggers.size();
  for(int i = 0; i < count; i++)
  {
    //If we are within 10 minutes past
    // an even that was not triggered,
    // we should trigger it.
    if(configuration.triggers[i].tHour == hour(t))
    {
      if(configuration.triggers[i].tMinute == minute(t)
        || ((minute(t) - configuration.triggers[i].tMinute) >= 10)) 
      {
        if(!configuration.triggers[i].triggered)
        {
          //Calculate output based on intensity
          unsigned char red = 0, green = 0, blue = 0;
          red = (unsigned char)(configuration.color.red * configuration.triggers[i].tIntensity);
          green = (unsigned char)(configuration.color.green * configuration.triggers[i].tIntensity);
          blue = (unsigned char)(configuration.color.blue * configuration.triggers[i].tIntensity);
          
          SetLights(red, green, blue);
          configuration.triggers[i].triggered = true;
        }
      }
    }
  }
}

//Sets the kights to the specified color and displays the changes
void SetLights(unsigned char red, unsigned char green, unsigned char blue)
{
    uint32_t color_tmp = strip.Color(red, green, blue);
    
    for (int i=0; i < strip.numPixels(); i++)
      strip.setPixelColor(i, color_tmp);
      
    strip.show();
}

void ResetTriggers()
{
  int count = configuration.triggers.size();
  
  for(int i = 0; i < count; i++)
  {
    configuration.triggers[i].triggered = false;
  }
}

/*
==========================
Setup/Loop
==========================
*/
void setup() {
  //Turn lights off, just in case
  SetLights(0,0,0);
  
  //Setup the time
  if(USE_SERIAL_TIME_SERVER)
  {
    Serial.begin(9600);
    setSyncProvider(RequestTimeSync);
  }
  SetupTime();
  
  //Choose a color - 3500K for me
  configuration.color = Color::FromKelvin(3500);
  
  //Add our on and off triggers
  TimeTrigger on, off;
  on.tHour = 6;
  on.tMinute = 30;
  on.tIntensity = 1.0;
  
  off.tHour = 0;
  off.tMinute = 30;
  off.tIntensity = 0.0;
  
  //Register the triggers
  configuration.triggers.push_back(on);
  configuration.triggers.push_back(off);
  
  //Lastly, reset the triggers
  ResetTriggers();
}

void loop() {
  SyncTime();
  CheckTriggers();
}

