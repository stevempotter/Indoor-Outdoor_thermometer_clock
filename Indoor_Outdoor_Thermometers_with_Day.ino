// Steve M. Potter Indoor-outdoor dual thermometer with real-time clock and day of week.

// Set time with the UDLRS buttons on LCD. Poke rightmost LCD button to enter or leave set mode.
// L and R cycle to next or prev parameter. U increment value, D decrement value, S[elect] save and exit set mode.
// Uses DS3231 RTC clock module. 
// plug its SCL line into A5, SDA into A4
// Those are the default GPIO for I2C.
// The module works fine off 3.3V, so plug its Vcc into 3.3V and GND into GND.
// It also works fine with no battery, but needs that to remember the time when not powered by an adaptor or USB.
// There is a diode on the RTC board that prevents the 3V CR2032 coin cell from powering the Arduino.
// Derived from...Example testing sketch for various DHT humidity/temperature sensors
// Written by ladyada, public domain
// And by Steve M. Potter Nov 2, 2022  steve at stevempotter.tech
// I found that the DHT11 was pretty unreliable with humidity, and slow with temp and hum. I will just go with the DS18B20 for temperature only.

#include <Wire.h>
//#include <LCD.h>
#include <LiquidCrystal.h>
//#include "RTClib.h"  // Real Time Clock library
#include <MD_DS3231.h> // Real Time Clock library that includes Day of the Week calculation (ENABLE_DOW)
//#include <OneWire.h>
#include <DallasTemperature.h>


 /*  This program demonstrates button detection, LCD text/number printing,
  and LCD backlight control on the Linksprite Freetronics LCD & Keypad Shield, connected to an Arduino UNO board.
 
  Pins used by LCD & Keypad Shield:
 
    A0: Buttons, analog input from voltage ladder
    D4: LCD bit 4
    D5: LCD bit 5
    D6: LCD bit 6
    D7: LCD bit 7
    D8: LCD RS
    D9: LCD E
    D10: LCD Backlight (high = on, also has pullup high so default is on)
 
*/

#define ONE_WIRE_BUS A1

#define TEMPERATURE_PRECISION 11  // 9: 94 ms conversion time

// Pins in use
#define BUTTON_ADC_PIN           A0  // A0 is the button ADC input
#define LCD_BACKLIGHT_PIN        10  // D10 controls LCD backlight
// ADC readings expected for the 5 buttons on the ADC input
#define RIGHT_10BIT_ADC          21  // right   // Empirically determined voltages on pin A0, scaled to 1024 DU
#define UP_10BIT_ADC            144  // up     // No button = 5V or 1023
#define DOWN_10BIT_ADC          330  // down
#define LEFT_10BIT_ADC          506  // left
#define SELECT_10BIT_ADC        739  // select
#define BUTTON_VRANGE            10  // Half-range of voltages (in DU) for valid button sensing window
//return values for ReadButtons()
#define BUTTON_NONE               0  // 
#define BUTTON_RIGHT              1  // 
#define BUTTON_UP                 2  // 
#define BUTTON_DOWN               3  // 
#define BUTTON_LEFT               4  // 
#define BUTTON_SELECT             5  // 
#define ADJHOUR                   0
#define ADJMIN                    1
#define ADJDATE                   2
#define ADJMONTH                  3
#define ADJYEAR                   4
#define ENABLE_DOW                1 // enables day of week support
#define FIRST_ROW                 0
#define SECOND_ROW                1
/*--------------------------------------------------------------------------------------
  Variables
--------------------------------------------------------------------------------------*/
bool buttonJustPressed  = false;         //this will be true after a ReadButtons() call if triggered
byte button = BUTTON_NONE;
bool SettingTime = false;
byte buttonWas = BUTTON_NONE;   //used by ReadButtons() as a way to debounce these analog buttons.
int debounceMS = 50; // Need to hold buttons down for 1/20 of a second, to get two readings ina row the same.
int Adjusting = ADJHOUR; // default first thing to adjust is the hour
const String AdjustingStr[] = {"  Current Hour?", "Current Minute?", "  Current Date?", "Current Month?", "  Current Year?"};
long LastReadTime = millis(); // keeps track of last update of LCD when not setting time.
// For TimeSet mode:

/*--------------------------------------------------------------------------------------
  Init the LCD library with the LCD pins to be used
--------------------------------------------------------------------------------------*/
LiquidCrystal lcd( 8, 9, 4, 5, 6, 7 );   //Pins for the freetronics 16x2 LCD shield. LCD: ( RS, E, LCD-D4, LCD-D5, LCD-D6, LCD-D7 )

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);

// arrays to hold device addresses. Use serial monitor to read addresses of a new DS18B20 device.
DeviceAddress outsideThermometer = { 0x28, 0xFF, 0xB0, 0xD2, 0x81, 0x16, 0x03, 0x6C };  // The longer wire
DeviceAddress insideThermometer = {0x28, 0xFF, 0xD1, 0x28, 0x80, 0x16, 0x05, 0xAC };    // the shorter wire

float tempC;

void setup() 
{
   Wire.begin();  
   pinMode( BUTTON_ADC_PIN, INPUT );      //button voltage input
   pinMode( LCD_BACKLIGHT_PIN, OUTPUT );  //lcd backlight control 
   digitalWrite( LCD_BACKLIGHT_PIN, HIGH );  
   lcd.begin( 16, 2 ); // 16 columns and 2 rows of characters.
   lcd.setCursor( 0, FIRST_ROW );   //top left
   lcd.print( "Indoor-Outdoor" );
   lcd.setCursor( 0, SECOND_ROW );   //bottom left
   lcd.print( "Thermometers" );
   Serial.begin(9600);
   Serial.println("Inside/Outside thermometers by Steve M. Potter");
   sensors.begin(); // for DS18B20 temp sensors
   Serial.print("Locating devices: Found ");
   Serial.print(sensors.getDeviceCount(), DEC);    // locate DS18B20 devices on the bus
   Serial.println(" DS18B20 devices.");
   // show the addresses we found on the bus
   Serial.print("Device 0 Address: ");
   printAddress(insideThermometer);
   Serial.println();
   Serial.print("Device 1 Address: ");
   printAddress(outsideThermometer);
   Serial.println();
   RTC.control(DS3231_TCONV, DS3231_ON); // not sure what this does.
   RTC.control(DS3231_12H, DS3231_OFF);  // 24 hour clock
   /*
   RTC.yyyy = 2022;
   RTC.mm = 11;
   RTC.dd = 2;
   RTC.h = 14;
   RTC.m = 30;
   RTC.s = 0;
   RTC.dow = 4; 
   RTC.setCentury(20);
   RTC.writeTime(); // stores the current time and date into the RTC chip. Uncomment this to put the above values into the RTC on startup.
   // If you leave this commented out, and if the battery in the RTC is good, it will just remember what time it was after a power outage.
   */
   sensors.setResolution(insideThermometer, TEMPERATURE_PRECISION);
   sensors.setResolution(outsideThermometer, TEMPERATURE_PRECISION);

}

//////////////////////////////  main loop  ////////////////////////////////////////////////////////////////////////////

void loop() 
{
   //get the latest button pressed, also the buttonJustPressed, buttonJustReleased flags
  button = ReadButtons();
  
  if ( (buttonJustPressed) && (button == BUTTON_SELECT) && (SettingTime == false))  // Go to SetTime().
  {
   SetTime();
  }
  Serial.print("."); // just to see how fast it is looping.
   
  // Update LCD every second.
  if ((millis() - LastReadTime) > 1000)
  {
    sensors.requestTemperatures();
    Serial.print(" RTC temp ");
    Serial.println(RTC.readTempRegister());
    lcd.clear();
    lcd.setCursor(0,FIRST_ROW);
    printTemperature(outsideThermometer);
    lcd.print("Out: ");
    lcd.print(tempC, 1);
    printTemperature(insideThermometer);
    lcd.print("In: ");
    lcd.print(tempC, 1);
    RTC.readTime();
    printTime();
    LastReadTime = millis();
  }
} // end main loop
/////////////////////////////////////////////////////////////////////////////////////////////////////////



// function to print the temperature for a DS18B20 device
void printTemperature(DeviceAddress deviceAddress)
{
  tempC = sensors.getTempC(deviceAddress);
  Serial.print(" ");
  Serial.print(deviceAddress[7]); // show the last number in the address. "108" is Outside, "172" is Inside.
  Serial.print(" Temp C: ");
  Serial.print(tempC);
}

// function to print a device address to serial monitor
void printAddress(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    // zero pad the address if necessary
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}

////////////////////////////   BUTTONS   ///////////////////////////////////////////////////////
byte ReadButtons()
{
   button = BUTTON_NONE;   // return no button pressed if the below checks don't apply.
   
   //read the button ADC pin voltage

   unsigned int buttonVoltage = analogRead( BUTTON_ADC_PIN );     //sense if the voltage falls within valid voltage windows

   if( buttonVoltage < ( RIGHT_10BIT_ADC + BUTTON_VRANGE ) )
    {
        button = BUTTON_RIGHT;
        if (buttonWas == button) buttonJustPressed = true; // Button actions only get triggered if button held down for 100ms    
    }
    else if(   buttonVoltage >= ( UP_10BIT_ADC - BUTTON_VRANGE )
            && buttonVoltage <= ( UP_10BIT_ADC + BUTTON_VRANGE ) )
    {
        button = BUTTON_UP;
        if (buttonWas == button) buttonJustPressed = true;
    }
    else if(   buttonVoltage >= ( DOWN_10BIT_ADC - BUTTON_VRANGE )
            && buttonVoltage <= ( DOWN_10BIT_ADC + BUTTON_VRANGE ) )
    {
        button = BUTTON_DOWN;
        if (buttonWas == button) buttonJustPressed = true;
    }
    else if(   buttonVoltage >= ( LEFT_10BIT_ADC - BUTTON_VRANGE )
            && buttonVoltage <= ( LEFT_10BIT_ADC + BUTTON_VRANGE ) )
    {
        button = BUTTON_LEFT;
        if (buttonWas == button) buttonJustPressed = true;
    }
    else if(   buttonVoltage >= ( SELECT_10BIT_ADC - BUTTON_VRANGE )
            && buttonVoltage <= ( SELECT_10BIT_ADC + BUTTON_VRANGE ) )
    {
        button = BUTTON_SELECT;
        if (buttonWas == button) buttonJustPressed = true;
    }

   buttonWas = button;    //save the latest button value, for change event detection next time round
   delay(debounceMS);
   return( button );
}  // end ReadButtons

void p2dig(uint8_t v)
// print 2 digits with leading zero
{
  if (v < 10) lcd.print("0");
  lcd.print(v);
  Serial.print(v);
}

/////////////////////   PRINT CURRENT TIME    ////////////////////////////////

void printTime(void)
// Print the day of the week and current time [and date] to the LCD display
{
  lcd.setCursor(0,SECOND_ROW);
  lcd.print(dow2String(RTC.dow));
  lcd.print(" ");  
  p2dig(RTC.h);
  lcd.print(":");
  p2dig(RTC.m);
  lcd.print(":");
  p2dig(RTC.s);
  if (RTC.status(DS3231_12H) == DS3231_ON)
    lcd.print(RTC.pm ? " pm" : " am");
}

////////////////  SET TIME & DATE  //////////////////////////////////////////

void SetTime() // Enter if SelectFlags are false and any button is pressed.
// Cycle through the things to set with left and right buttons: 
// Hour, Min, Date, Month, Year.
// public vars are: RTC.h RTC.m RTC.dd RTC.mm RTC.yyyy 

{
 SettingTime = true;  // Stay here until the Select button is pressed.
 button = BUTTON_NONE;
 buttonJustPressed = false; // use up the button press that got us into SetTime.
 lcd.clear();
 lcd.setCursor(0, FIRST_ROW);
 lcd.print("RTC temp: ");
 lcd.print(RTC.readTempRegister());
 lcd.setCursor(0,SECOND_ROW);
 lcd.print("Set:Hr-Min-Dt-M-Y");
 delay(2000);
 lcd.clear();
 lcd.setCursor(0,FIRST_ROW); 
 lcd.print(AdjustingStr[Adjusting]);
 do // Loop until Select pressed.
  {   
   while (buttonJustPressed == false) // keep looking for a button press.
   {
     ReadButtons();
     Serial.println("R"); // just to see it looping.
   }
   
   // A button was pressed:
   
   if (button == BUTTON_RIGHT) // increment parameter type.
    { 
      Adjusting++; // Move to the next parameter.
      if (Adjusting > 4) Adjusting = 0; // Go back to Hour
      lcd.clear();
      lcd.setCursor(0,FIRST_ROW); 
      lcd.print(AdjustingStr[Adjusting]);
      buttonJustPressed = false;
    }
   if (button == BUTTON_LEFT)  // go back to previous parameter
    { 
      Adjusting--;
      if (Adjusting < 0) Adjusting = 4; // go back to Year
      lcd.clear();
      lcd.setCursor(0,FIRST_ROW); 
      lcd.print(AdjustingStr[Adjusting]);
      buttonJustPressed = false;
    }


     switch (Adjusting)
     {
      case ADJHOUR:
       lcd.setCursor(5,SECOND_ROW);
       p2dig(RTC.h);
       delay(100);
       if (button == BUTTON_UP) RTC.h++;
       if(RTC.h > 23) RTC.h = 0;
       if (button == BUTTON_DOWN) RTC.h--;
       if(RTC.h < 0) RTC.h = 23;
       lcd.setCursor(5,SECOND_ROW);
       p2dig(RTC.h);
       buttonJustPressed = false;      
      break;
  
      case ADJMIN:
       lcd.setCursor(5,SECOND_ROW);
       p2dig(RTC.m);
       delay(100);
       if (button == BUTTON_UP) RTC.m++;
       if(RTC.m > 60) RTC.m = 0;
       if (button == BUTTON_DOWN) RTC.m--;
       if(RTC.m < 0) RTC.m = 60;
       lcd.setCursor(5,SECOND_ROW);
       p2dig(RTC.m);
       buttonJustPressed = false;      
      break;
  
      case ADJDATE:
       lcd.setCursor(5,SECOND_ROW);
       p2dig(RTC.dd);
       delay(100);
       if (button == BUTTON_UP) RTC.dd++;
       if(RTC.dd > 31) RTC.dd = 1;
       if (button == BUTTON_DOWN) RTC.dd--;
       if(RTC.dd < 1) RTC.dd = 31;
       lcd.setCursor(5,SECOND_ROW);
       p2dig(RTC.dd);
       buttonJustPressed = false;      
      break;
  
      case ADJMONTH:
       lcd.setCursor(5,SECOND_ROW);
       p2dig(RTC.mm);
       delay(100);
       if (button == BUTTON_UP) RTC.mm++;
       if(RTC.mm > 12) RTC.mm = 1;
       if (button == BUTTON_DOWN) RTC.mm--;
       if(RTC.mm < 1) RTC.mm = 12;
       lcd.setCursor(5,SECOND_ROW);
       p2dig(RTC.mm);
       buttonJustPressed = false;      
      break;
  
      case ADJYEAR:
       lcd.setCursor(5,SECOND_ROW);
       lcd.print(RTC.yyyy);
       delay(100);
       if (button == BUTTON_UP) RTC.yyyy++;
       if (button == BUTTON_DOWN) RTC.yyyy--;
       lcd.setCursor(5,SECOND_ROW);
       lcd.print(RTC.yyyy);
      buttonJustPressed = false;       
      break;
     } // end switch Adjusting

        
   if ( (button == BUTTON_SELECT) ) // exit setting stuff. 
     {
      SettingTime = false;
      RTC.writeTime();
      lcd.clear();
      Serial.println("Leaving SetTime");
      buttonJustPressed = false;
     }


  } while ( SettingTime == true ); // end while setting time

} // end SetTime



/////////////  Create day of week string   //////////////////////////

const char *dow2String(uint8_t code)
// Day of week to string. DOW 1=Sunday, 0 is undefined
{
  static const char *str[] = { "---", "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
 // static const char *str[] = {"DOW=0", "Sat", "Sun", "Mon", "Tue", "Wed", "Thu", "Fri",}; // ?For leap years after Feb 29
  // calcDoW(uint16_t yyyy, uint8_t mm, uint8_t dd) 
  // https://en.wikipedia.org/wiki/Determination_of_the_day_of_the_week
// This algorithm good for dates  yyyy > 1752 and  1 <= mm <= 12
// Returns dow  01 - 07, 01 = Sunday
  if (code > 7) code = 0;
  return(str[code]);
}  // end dow string
