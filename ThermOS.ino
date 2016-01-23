//#define debug true

//Thermostat with RTC and Thermostat, both on I2C bus.
//Requires RollingAverage library from www.github.com/rdbahm/rollingaverage

#include <Wire.h>
#include <Servo.h>
#include "RTClib.h"
#include <RollingAverage.h>

/****** CONFIGURATION *******/

//Hardware definitions
const int i2c_address_thermometer = 0x48;
const int servo_pin = 9;

//Temperature config
const int temperatures[4] = {72, 65, 70, 68}; //Same order as "current_mode"
const int temperature_threshold = 2; //Temperature difference from target temp to activate heater, F.
const int temperature_overshoot = 1; //How much to overshoot the temperature when heating.

//Scheduling config
//Sunday = 0
//Hours are 24 hour.
//-1 to disable this mode.
const int sch_wake[7] = {8, 6, 6, 6, 6, 6, 8};
const int sch_away[7] = { -1, 8, 8, 8, 8, -1, -1};
const int sch_return[7] = {17, 17, 17, 17, 17, 17, 17};
const int sch_sleep[7] = {22, 22, 22, 22, 22, 23, 23};

//Servo calibration config
const int servo_calib_min_pos = 45; //Lower position value for calibration.
const int servo_calib_min_temp = 50; //Temperature at position as set.
const int servo_calib_max_pos = 135; //Maximum position value for calibration.
const int servo_calib_max_temp = 90;
const int servo_limit_min = 0; //Physical limit for servo - these settings constrain the output to the servo motor.
const int servo_limit_max = 180;

//Misc config
const int furnace_update_interval = 60000; //Time in MS between evaluating if the furnace power should be toggled.
const int temp_update_interval = 6000; //How frequently to poll for new temperatures.
const float degrees_per_minute = 0.25; //Hardcoded assumption about how fast we can heat a room. Used to calculate when to start heating for a mode change.

/******* GLOBAL VARIABLES ******/
Servo ControlServo; //Servo object
RTC_DS1307 RTC;
RollingAverage Temperature;


void setup() {
  ControlServo.attach(servo_pin);
  Wire.begin();
  RTC.begin();

#ifdef debug
  Serial.begin(9600);
#endif
}

void loop() {
  byte current_mode = 0; //0 wake, 1 away, 2 return, 3 sleep.
  byte next_mode = 1;
  boolean is_mode_preheat = false; //Set to true if we need to preheat the room to reach the temperature in time.
  boolean is_mode_override = false; //Set to true if the mode has been overridden. (For future use: Button?)
  int override_temp = 0; //Override temperatures take priority over mode until the next mode starts.
  int target_temp = 70;
  int last_target_temp = 70;
  float this_temp = 0;
  TimeSpan preheat_time = 0; //Time, in minutes, it will take to heat to the next mode.
  unsigned long int last_temp_update = 0;
  unsigned long int last_furnace_update = 0;

  while (true) {
    if ((millis() - last_furnace_update) >= furnace_update_interval) {
      last_furnace_update = millis();
      this_temp = Temperature.read();
      DateTime now = RTC.now();

      last_target_temp = target_temp;

      current_mode = getMode(now);
      if (is_mode_preheat == true && current_mode == next_mode)
      {
        //Disable preheat mode once we get to the mode we're preheating for.
        is_mode_preheat = false;
      }
      next_mode = getNextMode(current_mode);

      preheat_time = getTimeToHeat(getModeTemperature(next_mode), this_temp, degrees_per_minute);

      if (getMode(now + preheat_time) != current_mode && is_mode_preheat == true) {
        is_mode_preheat = true;
      }

      if (is_mode_preheat == true)
      {
        //If we're using the preheat function, get temperature from the next mode.
        target_temp = getModeTemperature(next_mode);
      }
      else
      {
        //This codepath is normal - occurs if we're within a mode.
        target_temp = getModeTemperature(current_mode);
      }

      if (target_temp != last_target_temp)
      {
        //TODO: This is pretty naive. Should work in theory but because one of the major problems...
        //...with the knob-style thermostats is they're horrendously inconsistent, this will result...
        //...in horrendously inconsistent room temperatures.
        setFurnace(target_temp);
      }

      //Output serial if we're debugging.
#ifdef debug
      Serial.print(now);
      Serial.print(",");
      Serial.print(current_mode);
      Serial.print(",");
      Serial.print(this_temp);
      Serial.print(",");
      Serial.println(target_temp);
#endif

    } //End of furnace update.

    if ((millis() - last_temp_update) >= temp_update_interval) {
      last_temp_update = millis();
      Temperature.add(getTemperature()); //Add the current temperature to our rolling average.
    } //End of temperature update.
  } //End of program loop
} //End of void loop

TimeSpan getTimeToHeat(int target, int current_temp, float heat_rate) {
  //Calculate how long it should take to heat from the current temperature to the target temperature based on the rate of heating.
  //RETURNS: Time in minutes.
  int temperature_difference = target - current_temp;
  if (temperature_difference <= 0) {
    return 0;
  }
  else {
    int minutes = temperature_difference / heat_rate;
    TimeSpan timespan_output = TimeSpan(0, 0, minutes, 0);
    return timespan_output;
  }
}

void setFurnace(int temperature) {
  //Basically a euphemism for "use the servo."
  int computed_pos = map(temperature, servo_calib_min_temp, servo_calib_max_temp, servo_calib_min_pos, servo_calib_max_pos); //Maps temperature to appropriate position.
  computed_pos = constrain(computed_pos, servo_limit_min, servo_limit_max); //To prevent physical damage.
  ControlServo.write(computed_pos);
}

int getModeTemperature(int mode) {
  return temperatures[mode];
}

byte getNextMode(byte mode) {
  byte output_mode = mode + 1;
  if (output_mode > 3) {
    output_mode = 0;
  }
  return output_mode;
}

byte getMode(DateTime this_time) {
  byte this_dayofweek = this_time.dayOfTheWeek();
  byte this_hour = this_time.hour();
  int computed_mode = 4;

  //Check schedule based on day of week.
  if (this_hour >= sch_sleep[this_dayofweek] && sch_sleep[this_dayofweek] != -1) {
    computed_mode = 3; //If hour is after sleep schedule, go to sleep mode (3)
  }
  else if (this_hour >= sch_return[this_dayofweek] && sch_return[this_dayofweek] != -1) {
    computed_mode = 2;
  }
  else if (this_hour >= sch_away[this_dayofweek] && sch_away[this_dayofweek] != -1) {
    computed_mode = 1;
  }
  else if (this_hour >= sch_away[this_dayofweek] && sch_away[this_dayofweek] != -1) {
    computed_mode = 0;
  }
  else
  {
    computed_mode = 3; //Sleep mode if nothing else fits.
  }

  return computed_mode;
}

float getTemperature() {
  Wire.requestFrom(i2c_address_thermometer, 2);

  byte MSB = Wire.read();
  byte LSB = Wire.read();

  //it's a 12bit int, using two's compliment for negative
  int TemperatureSum = ((MSB << 8) | LSB) >> 4;

  float farenheit = (TemperatureSum * 0.1125) + 32;
  return farenheit;
}
