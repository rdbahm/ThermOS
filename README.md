# ThermOS
A simple thermostat program designed for use with knob-type thermostats. Requires an Arduino, a temperature sensor, and a servo output. (Currently, temperature sensor is only lightly used.)

Features:
- 7 day scheduling for 4 modes: awake, away, return, and sleep.
- Individual setpoints for 4 modes.
- Interfaces with a servo which has been attached to a knob-type thermostat, like one might have in an apartment.
- Uses TMP102 digital temperature sensor for temperature information.
- Uses DS1307 Real-Time Clock to keep accurate time.
- For safety, controls the knob position so a hardware or software failure will keep a safe temperature.
