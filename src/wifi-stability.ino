/*
* Project wifi-stability
* Description: Testing of the stablity of wifi on a photon.
*/
#include "Particle.h"
#include "SdFat.h"
#include <OneWire.h>
#include "prototypes.h"
#include "globals.h"
#include "constants.h"
#include "SdLog.h"
#include "HybridLog.h"

// Needed to make the system not try to connect to WiFi before
//executing setup()
SYSTEM_MODE(SEMI_AUTOMATIC);


// Use sdCard for logging output.
SDcardLogHandler logHandler1;
// Use primary serial over USB interface for logging output
//SerialLogHandler logHandler2;
// Use SDCard and cloud as logging output.
HybridLogHandler logHandler;

//STARTUP(System.enableFeature(FEATURE_RETAINED_MEMORY));

OneWire ds = OneWire(D4);  // 1-wire signal on pin D4

unsigned long lastUpdate = 0;

float lastTemp;

// setup() runs once, when the device is first turned on.
// Executes on boot because we changed the system mode
void setup() {
  //while(!Serial.isConnected())
  //{
  //     Particle.process();
  //}

  //Alive message
  char *message = "yes i am.";

  // Initilized sd
  LazyInit();

  //Start off the log with a boot msg.
  Log.info("Booting %s", System.version().c_str());

  //Call handle_all_the_events on every system event
  System.on(all_events, handle_all_the_events);

  //FOR TESTING
  // Connects to a network secured with WPA2 credentials.
  //WiFi.setCredentials("OpenWrt", "helloworld");

  // Connect to WiFi using the credentials stored, or find a new network.
  initialConnect();

  // variable name max length is 12 characters long
  //Alive varible
  Particle.variable("areyoualive", message);
}

// loop() runs over and over again, as quickly as it can execute.
void loop() {

  // Keep-alive
  Particle.process();
  //Check network health.
  checkNetwork();

  //Keep-alives
  checkIns();
  //Let HybridLogHandler do it sending
  logHandler.loop();
  //Keep-alives
  checkIns();

  //Maker Kit tutorial
  //Shortend version of actual code see url below for full code.
  //https://docs.particle.io/tutorials/projects/maker-kit/#tutorial-4-temperature-logger
  byte i;
  byte present = 0;
  byte type_s;
  byte data[12];
  byte addr[8];
  float celsius, fahrenheit;


  //Chip = DS18B20
  addr[0] = 0x28;
  addr[1] = 0xFF;
  addr[2] = 0x97;
  addr[3] = 0xFE;
  addr[4] = 0x53;
  addr[5] = 0x15;
  addr[6] = 0x2;
  addr[7] = 0xFA;

  checkIns();

  // second the CRC is checked, on fail,
  // print error and just return to try again

  if (OneWire::crc8(addr, 7) != addr[7]) {
    Serial.println("CRC is not valid!");
    return;
  }

  checkIns();
  // we have a good address at this point
  // this device has temp so let's read it

  ds.reset();               // first clear the 1-wire bus
  ds.select(addr);          // now select the device
  // ds.write(0x44, 1);     // tell it to start a conversion, with parasite power on at the end
  ds.write(0x44, 0);        // or start conversion in powered mode (bus finishes low)

  // just wait a second while the conversion takes place
  // different chips have different conversion times, check the specs, 1 sec is worse case + 250ms
  // you could also communicate with other devices if you like but you would need
  // to already know their address to select them.
  wd.checkin();
  delay(1000);     // maybe 750ms is enough, maybe not, wait 1 sec for conversion
  checkIns();
  // we might do a ds.depower() (parasite) here, but the reset will take care of it.

  // first make sure current values are in the scratch pad

  present = ds.reset();
  ds.select(addr);
  ds.write(0xB8,0);         // Recall Memory 0
  ds.write(0x00,0);         // Recall Memory 0

  checkIns();

  // now read the scratch pad
  present = ds.reset();
  ds.select(addr);
  ds.write(0xBE,0);         // Read Scratchpad
  if (type_s == 2) {
    ds.write(0x00,0);       // The DS2438 needs a page# to read
  }

  checkIns();

  // transfer and print the values
  Serial.print("  Data = ");
  Serial.print(present, HEX);
  Serial.print(" ");
  for ( i = 0; i < 9; i++) {           // we need 9 bytes
    data[i] = ds.read();
    Serial.print(data[i], HEX);
    Serial.print(" ");
  }
  Serial.print(" CRC=");
  Serial.print(OneWire::crc8(data, 8), HEX);
  Serial.println();

  checkIns();

  // Convert the data to actual temperature
  // because the result is a 16 bit signed integer, it should
  // be stored to an "int16_t" type, which is always 16 bits
  // even when compiled on a 32 bit processor.
  int16_t raw = (data[1] << 8) | data[0];
  if (type_s == 2) raw = (data[2] << 8) | data[1];
  byte cfg = (data[4] & 0x60);
  checkIns();
  switch (type_s) {
    case 1:
    checkIns();
    raw = raw << 3; // 9 bit resolution default
    if (data[7] == 0x10) {
      // "count remain" gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
    celsius = (float)raw * 0.0625;
    checkIns();
    break;
    case 0:
    checkIns();
    // at lower res, the low bits are undefined, so let's zero them
    if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
    if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
    if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
    // default is 12 bit resolution, 750 ms conversion time
    celsius = (float)raw * 0.0625;
    checkIns();
    break;

    case 2:
    checkIns();
    data[1] = (data[1] >> 3) & 0x1f;
    if (data[2] > 127) {
      celsius = (float)data[2] - ((float)data[1] * .03125);
      }else{
        celsius = (float)data[2] + ((float)data[1] * .03125);
      }
    }
    checkIns();
    // remove random errors
    if((((celsius <= 0 && celsius > -1) && lastTemp > 5)) || celsius > 125) {
      celsius = lastTemp;
    }
    checkIns();
    fahrenheit = celsius * 1.8 + 32.0;
    lastTemp = celsius;
    Serial.print("  Temperature = ");
    Serial.print(celsius);
    Serial.print(" Celsius, ");
    Serial.print(fahrenheit);
    Serial.println(" Fahrenheit");

    checkIns();
    checkNetwork();

    // now that we have the readings, we can publish them to the cloud
    String temperature = String(fahrenheit); // store temp in "temperature" string
    Particle.publish("temperature", temperature, PRIVATE); // publish to cloud
    delay(5000); // 5 second delay

    checkIns();
  }

  void checkIns()
  {
    //Reset the timer
    wd.checkin();
    // Keep-alive
    Particle.process();
  }

  void LazyInit()
  {
    if(!initilized)
    {
      // Initialize SdFat or print a detailed error message and halt
      // Use half speed like the native library.
      // Change to SPI_FULL_SPEED for more performance(was SPI_HALF_SPEED).
      if (!sd.begin(chipSelect, SPI_HALF_SPEED))
      {
        sd.initErrorPrint();
      }
    }
    initilized = true;
  }
int cloudCalls(String useless)
{
  shutdown();
  return 1;
}

void shutdown(){
    while(hlIsWriting || sdlIsWriting)
    {
      checkIns();
    }
    while(1)
    {
      digitalWrite(led1, HIGH);

      // We'll leave it on for 1 second...
      delay(1000);

      // Then we'll turn it off...
      digitalWrite(led1, LOW);

      // Wait 1 second...
      delay(1000);
    }

}
