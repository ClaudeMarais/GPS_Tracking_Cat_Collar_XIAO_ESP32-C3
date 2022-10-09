// GPS Logger with XIAO ESP32-C3
// -----------------------------
//
// This is a very simple project that reads GPS data and then writes the data to a MicroSD card
// in a format that can be used to create custom GoogleMaps (https://www.google.com/maps/d/)
//
// LED Status:
//    Off         - No power, check battery
//    Constant on - Critical error, check if MicroSD card was inserted, otherwise perhaps some of the wires are not connected anymore
//    Fast blink  - No valid GPS data. This is not a critical error, since the GPS might still be trying to aquire satelites
//    Slow blink  - Everything is fine, we're getting and logging valid data!
//
// The purpose of the project was to find out where our cat is wondering around at night :-)
// Therefore, I tried to find the smallest and lighest hardware components to go around a cat collar
//
// Hardware                 | Size
// ----------------------------------
// BN-220 GPS               | 22mm x 20mm
// XIAO ESP32-C3            | 21mm x 17.5mm
// SPI MicroSD Card module  | 18mm x 18mm
// 160mAh Lipo battery      | 20mm x 20mm (Lasts about an hour with this setup)
// Red LED 2.5V 20mA        | Small :-)
// 1K ohm resistor          | Very small ;-D (I just need a dim LED blink at night, so 1K works fine)

// Writing to MicroSD card
#include "FS.h"
#include "SD.h"

// Reading GPS
#include <TinyGPSPlus.h>
#include <SoftwareSerial.h>

// Read/Write persistant data using EEPROM
#include <Preferences.h>
Preferences persistedData;

// BN-220 GPS
// Pin 1 - GRD (Black)
// Pin 2 - TX (White) Serial Data Output
// Pin 3 - RX (Green) Serial Data Input
// Pin 4 - 3.3V (Red)

// XIAO ESP32-C3 pins
// https://wiki.seeedstudio.com/XIAO_ESP32C3_Getting_Started/

// UART for connecting GPS
// GPI20  RX
// GPI21  TX

// SPI for connecting MicroSD
// GPIO8  SCK
// GPIO9  MISO
// GPIO10 MOSI

// By default SS (Chip Select) is defined as GPIO20 which is the same as RX. Since we're using RX for the GPS, redefine as GPIO7
#define SS 7

// Connect an LED to GPIO2
#define LED 2

// Path to data file on MicroSD card
String DataFilePath;

// Indicate critical errors with constant lit LED
bool bCriticalError = false;

// Indicate no valid GPS data with fast flasing LED
bool bValidGPSData = false;

// Software serial connection to the GPS device
SoftwareSerial softwareSerial(RX, TX);

// GPS
TinyGPSPlus GPS;
static const uint32_t GPSBaud = 9600;             // Baud rate for BN-220
static const int GPSDataSampleFrequency = 5000;   // Grab data every five seconds

// LED status blinking
static const int BlinkFast = 500;                 // Half a seconds
static const int BlinkSlow = 2000;                // Two seconds

// Optimize for battery life, so don't send any text over Serial if we don't need to
// Comment this define out if not connected to PC
#define DEBUG 1

#ifdef DEBUG
#define DebugPrintln(str) Serial.println(str)
#define DebugPrint(str) Serial.print(str)
#else
#define DebugPrintln(str)
#define DebugPrint(str)
#endif

// Turn LED on/off
static void SetLED(bool bOn)
{
  digitalWrite(LED, bOn ? HIGH : LOW);
}

// Initialize MicroSD card
static bool InitialilzeMicroSD()
{
  DebugPrintln("\nInitialilzeMicroSD");

  if (!SD.begin(SS))
  {
      DebugPrintln("  ERROR: MicroSD card mount failed!");
      return false;
  }

  DebugPrintln("  MicroSD card mount succeeded");

  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE)
  {
    DebugPrintln("  ERROR: No MicroSD card inserted!");
    SD.end();
    return false;
  }

  DebugPrintln("  MicroSD card found");

  return true;
}

// Create a data file on the MicroSD
static bool CreateDataFile()
{
  DebugPrintln("\nCreateDataFile");

  // Construct data file with unique name using a counter in EEPROM
  persistedData.begin("GPSLogger", false);
  unsigned int fileCounter = persistedData.getUInt("FileCounter", 0);
  fileCounter++;
  persistedData.putUInt("FileCounter", fileCounter);
  persistedData.end();

  // Create CSV file to easily import into GoogleMaps
  DataFilePath = "/GPSLoggerData_" + String(fileCounter) + ".csv";

  fs::FS &fs = SD;
  File file = fs.open(DataFilePath.c_str(), FILE_WRITE);

  if (!file)
  {
    DebugPrintln("  ERROR: Failed to create data file!");
    return false;
  }

  // Write the file header
  if (!file.println("Longitude, Latitude, Altitude, Date, Time"))
  {
    DebugPrintln("  ERROR: Failed to write header to data file!");
    file.close();
    return false;
  }

#ifdef DEBUG
  Serial.printf("  Data file created successfully: %s\n", DataFilePath.c_str());
#endif

  return true;
}

// Log GPS data to the data file
static bool LogGPSData()
{
  DebugPrintln("\nLogGPSData");

  // Read data from GPS
  auto date = GPS.date;
  auto time = GPS.time;
  auto longitude = GPS.location.lng();
  auto latitude = GPS.location.lat();
  auto altitude = GPS.altitude.meters();

  // Check if we have valid data
   if (!date.isValid() || !time.isValid() || !GPS.location.isValid())
   {
     DebugPrintln("  ERROR: Invalid GPS data!");
     bValidGPSData = false;
     return false;
   }

  bValidGPSData = true;
 
  fs::FS &fs = SD;
  File file = fs.open(DataFilePath.c_str(), FILE_APPEND);
  if (!file)
  {
    DebugPrintln("  ERROR: Failed to open file for appending data");
    return false;
  }

  String gpsData;
  char stringData[64];

  sprintf(stringData, "%f, %f, %f", longitude, latitude, altitude);
  gpsData = stringData + String(", ");

  sprintf(stringData, "%02d/%02d/%02d", date.month(), date.day(), date.year());
  gpsData += stringData + String(", ");

  sprintf(stringData, "%02d:%02d:%02d", time.hour(), time.minute(), time.second());
  gpsData += stringData + String("\r\n");

  DebugPrint(String("  ") + gpsData);

  if (!file.print(gpsData))
  {
    DebugPrintln("  ERROR: Failed to write data to file");
    file.close();
    return false;
  }

  file.close();

  DebugPrintln("  Valid GPS data successfully written to MicroSD card!");

  return true;
}

// This custom version of delay() ensures that the GPS object is being "fed"
static void SmartDelay(unsigned long delay)
{
  unsigned long start = millis();
  do 
  {
    while (softwareSerial.available())
    {
      GPS.encode(softwareSerial.read());
    }
  } while ((millis() - start) < delay);
}

// Smart delay while blinking the LED
static void DelayWithLEDBlink(unsigned long delay, unsigned long blinkDelay)
{
  DebugPrintln("\nDelayWithLEDBlink");

  unsigned long start = millis();

  do
  {
    SetLED(true);
    SmartDelay(blinkDelay / 2.0);
    SetLED(false);
    SmartDelay(blinkDelay / 2.0);
  } while ((millis() - start) < delay);
}

void setup()
{
  bCriticalError = false;
  bValidGPSData = false;

  // Enable LED to indicate status
  pinMode(LED, OUTPUT);

  // Open hardware serial port to communicate with PC
#ifdef DEBUG
  {
    Serial.begin(115200);
    Serial.println("\nExecuting GPSLogger for ESP32 C3");
    Serial.println("\nHardware serial port connected!");
  }
#endif

  // Open software serial port to communicate with GPS
  {
    softwareSerial.begin(GPSBaud);
    DebugPrintln("Software serial port connected!");
  }

  if (!InitialilzeMicroSD())
  {
    bCriticalError = true;
    return;
  }

  if (!CreateDataFile())
  {
    bCriticalError = true;
    return;
  }

  // Let's wait a while to give the GPS some time to start up
  SmartDelay(5000);
}

void loop()
{
  // If there was a critical error, e.g. not finding a MicroSD card, then light up LED
  if (bCriticalError)
  {
    DebugPrintln("  Critical Error!");
    SetLED(true);
    SmartDelay(5000);

    bCriticalError = false;

    // Let's try to resolve critical erros, perhaps we just forgot to insert a Micro SD card
    if (!InitialilzeMicroSD())
    {
      bCriticalError = true;
      return;
    }

    if (!CreateDataFile())
    {
      bCriticalError = true;
      return;
    }
  }

  // Grab data only N milliseconds, and blink LED while waiting
  DelayWithLEDBlink(GPSDataSampleFrequency, bValidGPSData ? BlinkSlow : BlinkFast);

  // Log the GPS data
  LogGPSData();

  // If GPS didn't give data for 5 seconds, something is probably wrong
  if (millis() > 5000 && GPS.charsProcessed() < 10)
  {
    DebugPrintln("ERROR: No GPS data received. Check wiring.");
  }
}
