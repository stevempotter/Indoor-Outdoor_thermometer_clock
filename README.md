# Indoor-Outdoor_thermometer_clock
This is an Arduino sketch for a dual thermometer clock. I made mine using an arduino Uno with a 16x2 LCD shield. 
The shield has 5 buttons that can be used to set the hour, minute, date, month, and year. It displays the day of the week also.
These are connected to the Uno via one analog input, so you will need to calibrate the values for your own shield, and edit the code accordingly.
It uses two DS18B20 Onewire digital temperature sensors. You will need to look at the serial monitor to find out the serial number (address) of your 
DS18B20 devices and swap them in for the two I am using.
It also uses the DS3231 real-time clock (RTC) breakout PCB which has a coin cell to remember the date and 
time if it gets unplugged or there is a power outage.
https://learn.adafruit.com/adafruit-ds3231-precision-rtc-breakout
