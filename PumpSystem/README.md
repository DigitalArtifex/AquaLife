Pump System
===========

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