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

//Adress of id
int id_addr = 20;
int id; // 4 bytes

// Use sdCard for logging output.
//SDcardLogHandler logHandler1;
// Use primary serial over USB interface for logging output
//SerialLogHandler logHandler2;
// Use SDCard and cloud as logging output.
//HybridLogHandler logHandler;

//STARTUP(System.enableFeature(FEATURE_RETAINED_MEMORY));

int led1 = D7; // Instead of writing D7 over and over again, we'll write led2

OneWire ds = OneWire(D4);  // 1-wire signal on pin D4

unsigned long lastUpdate = 0;
int lastSendTemp = 0;
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
  //LazyInit();

  //Start off the log with a boot msg.
  Log.info("Booting %s", System.version().c_str());

  //Call handle_all_the_events on every system event
  System.on(all_events, handle_all_the_events);

  //FOR TESTING
  // Connects to a network secured with WPA2 credentials.
  //WiFi.setCredentials("OpenWrt", "helloworld");

  // Connect to WiFi using the credentials stored, or find a new network.
  initialConnect();

  //If this device has an id of 4 store it in memory.
  //Only do this once.
  //int id = 4; // 4 bytes
  //EEPROM.put(id_addr, id);
  //Get the id of this device.
  EEPROM.get(id_addr, id);
  String id_stri = String::format("ny_t%d", id); //Create the line to log
  Particle.publish("id_d", id_stri, PRIVATE);


  // variable name max length is 12 characters long

  //Alive varible
  Particle.variable("areyoualive", message);
  Particle.variable("issdgood", initilized);

  Particle.function("safeShut", cloudCalls);

  //Sd SPI_HALF_SPEED
  pinMode(led1, OUTPUT);

  //lastSendTemp
  lastSendTemp = Time.minute();
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
  //logHandler.loop();
  //Keep-alives
  checkIns();

  //sd.sync();

  //Maker Kit tutorial
  //Shortend version of actual code see url below for fu
  //https://docs.particle.io/tutorials/projects/maker-kit/#tutorial-4-temperature-logger

  //Send in 10min intervals
  if(abs(Time.minute()-lastSendTemp) != 10)
  {
    return;
  }
  byte i;
  byte present = 0;
  byte data[12];
  byte addr[8];
  float celsius, fahrenheit;



  //ROM = 28 FF 4 75 54 15 3 6A
  //Chip = DS18B20
  addr[0] = 0x28;
  addr[1] = 0xFF;
  addr[2] = 0x4;
  addr[3] = 0x75;
  addr[4] = 0x54;
  addr[5] = 0x15;
  addr[6] = 0x3;
  addr[7] = 0x6A;


  // second the CRC is checked, on fail,
  // print error and just return to try again

  if (OneWire::crc8(addr, 7) != addr[7]) {
    Serial.println("CRC is not valid!");
    return;
  }

  // we have a good address at this point
  // we will check if its the correct chip or just return
  if (addr[0] != 0x28)
  {
    return;
  }

  // this device has temp so let's read it

  ds.reset();               // first clear the 1-wire bus
  ds.select(addr);          // now select the device we just found
  // ds.write(0x44, 1);     // tell it to start a conversion, with parasite power on at the end
  ds.write(0x44, 0);        // or start conversion in powered mode (bus finishes low)

  // just wait a second while the conversion takes place
  // different chips have different conversion times, check the specs, 1 sec is worse case + 250ms
  // you could also communicate with other devices if you like but you would need
  // to already know their address to select them.
  checkIns();
  delay(1000);     // maybe 750ms is enough, maybe not, wait 1 sec for conversion

  // we might do a ds.depower() (parasite) here, but the reset will take care of it.

  // first make sure current values are in the scratch pad

  present = ds.reset();
  ds.select(addr);
  ds.write(0xB8,0);         // Recall Memory 0
  ds.write(0x00,0);         // Recall Memory 0

  // now read the scratch pad

  present = ds.reset();
  ds.select(addr);
  ds.write(0xBE,0);         // Read Scratchpad


  // transfer and print the values
  checkIns();
  Serial.print("  Data = ");
  Serial.print(present, HEX);
  Serial.print(" ");
  for ( i = 0; i < 9; i++) {           // we need 9 bytes
    data[i] = ds.read();
    Serial.print(data[i], HEX);
    Serial.print(" ");
  }

  // Convert the data to actual temperature
  // because the result is a 16 bit signed integer, it should
  // be stored to an "int16_t" type, which is always 16 bits
  // even when compiled on a 32 bit processor.
  int16_t raw = (data[1] << 8) | data[0];
  byte cfg = (data[4] & 0x60);


  // at lower res, the low bits are undefined, so let's zero them
  if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
  if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
  if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
  // default is 12 bit resolution, 750 ms conversion time
  celsius = (float)raw * 0.0625;

  checkIns();
  // remove random errors
  if((((celsius <= 0 && celsius > -1) && lastTemp > 5)) || celsius > 125) {
    celsius = lastTemp;
  }

  fahrenheit = celsius * 1.8 + 32.0;
  lastTemp = celsius;

  // now that we have the readings, we can publish them to the cloud
  String temperature = String(fahrenheit); // store temp in "temperature" string
  delay(15000); // 15 second delay
  Particle.publish("temperature", temperature, PRIVATE); // publish to cloud
  lastSendTemp = Time.minute();
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
    else
    {
      initilized = true;
    }
  }
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
