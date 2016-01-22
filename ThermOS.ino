//Thermostat with RTC and Thermostat, both on I2C bus.

#include <Wire.h>
#include <Servo.h>
#include <RTClib.h>

/****** CONFIGURATION *******/

//Hardware definitions
const int i2c_address_thermomoeter = 0x48;
const int servo_pin = 9;

//Temperature config
const int temperatures[4] = {70,62,70,66}; //Same order as "current_mode"
const int temperature_threshold = 2; //Temperature difference from target temp to activate heater, F.
const int temperature_overshoot = 1; //How much to overshoot the temperature when heating.

//Scheduling config
//Sunday = 0
//Hours are 24 hour.
//-1 to disable this mode.
const int sch_wake[7] = {8,6,6,6,6,6,8};
const int sch_away[7] = {-1,8,8,8,8,-1,-1};
const int sch_return[7] = {17,17,17,17,17,17,17};
const int sch_sleep[7] = {22,22,22,22,22,23,23};

//Servo calibration config
const int servo_calibration_minimum_position = 45; //Lower position value for calibration.
const int servo_calibration_minimum_temperature = 50; //Temperature at position as set.
const int servo_calibration_maximum_position = 135; //Maximum position value for calibration.
const int servo_calibration_maximum_temperature = 90;
const int servo_physical_limit_min = 0;
const int servo_physical_limit_max = 180;

/******* GLOBAL VARIABLES ******/
int current_mode = 0; //0 wake, 1 away, 2 return, 3 sleep, 4 error.
boolean mode_override = false; //Set to true if the mode has been overridden.
int target_temp = 70;
int last_target_temp = 70;
Servo ControlServo; //Servo object
RTC_DS1307 RTC;


void setup() {
  ControlServo.attach(servo_pin);
  Wire.begin();
  RTC.begin();
}

void loop() {
  DateTime now = RTC.now();
  float this_temperature = getTemperature();
  last_target_temp = target_temp;
  current_mode = getCurrentMode(now);
  target_temp = getModeTemperature(current_mode);
  
  if(target_temp != last_target_temp)
  {
	//TODO: This is pretty naiive. Should work in theory but because one of the major problems...
	//...with the knob-style thermostats is they're horrendously inconsistent, this will result...
	//...in horrendously inconsistent room temperatures.
   setFurnace(target_temp);
  }
  
  //Probably should output to serial here so we can diagnose how well we're keeping to temperature targets.
  
  //TODO: Need a way to lookahead on the schedule so we can be at temp on time.
  
  delay(10000);
}

void setFurnace(int temperature) {
 //Basically a euphamism for "use the servo."
 int computed_position = map(temperature,servo_calibration_minimum_temperature,servo_calibration_maximum_temperature,servo_calibration_minimum_position,servo_calibration_maximum_position); //Maps temperature to appropriatae position.
 computed_position = constrain(computed_position,servo_physical_limit_min,servo_physical_limit_max); //To prevent physical damage.
 ControlServo.write(computed_position);
}

int getModeTemperature(int mode){
  return temperatures[mode];
}

int getCurrentMode(DateTime this_time) {
 byte this_dayofweek = this_time.DayofWeek;
 byte this_hour = this_time.hour;
 int computed_mode = 4;
 
 //Check schedule based on day of week.
 if(this_hour >= sch_sleep[this_dayofweek] && sch_sleep[this_dayofweek] != -1){
   computed_mode = 3; //If hour is after sleep schedule, go to sleep mode (3)
 }
 else if(this_hour >= sch_return[this_dayofweek] && sch_return[this_dayofweek] != -1){
   computed_mode = 2;
 }
 else if(this_hour >= sch_away[this_dayofweek] && sch_away[this_dayofweek] != -1){
  computed_mode = 1;
 }
 else if(this_hour >= sch_away[this_dayofweek] && sch_away[this_dayofweek] != -1){
   computed_mode = 0;
 }
 else
 {
   computed_mode = 3; //Sleep mode if nothing else fits.
 }
 
 return computed_mode;
}

float getTemperature(){
  Wire.requestFrom(i2c_address_thermometer,2); 

  byte MSB = Wire.read();
  byte LSB = Wire.read();

  //it's a 12bit int, using two's compliment for negative
  int TemperatureSum = ((MSB << 8) | LSB) >> 4; 

  float celsius = TemperatureSum*0.0625;
  //return celsius;
  float farenheit = (1.8 * celsius) + 32
  return farenheit
}
