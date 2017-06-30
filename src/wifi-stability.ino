/*
* Project: Temprature Sensor
* Description: Reporting temprature reliably.
*/
#include "Particle.h"
#include <OneWire.h>
#include "prototypes.h"

ApplicationWatchdog wd(60000, System.reset); // system reboot watchdog

// Needed to make the system not try to connect to WiFi before
//executing setup()
SYSTEM_MODE(SEMI_AUTOMATIC);

//Adress of id
int id_addr = 20;
int id; // 4 bytes

// Use primary serial over USB interface for logging output
//SerialLogHandler logHandler2;

OneWire ds = OneWire(D4);  // 1-wire signal on pin D4

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
  const char *message = "yes i am.";

  //Start off the log with a boot msg.
  //Log.info("Booting %s", System.version().c_str());

  //Call handle_all_the_events on every system event
  //System.on(all_events, handle_all_the_events);

  //FOR TESTING
  // Connects to a network secured with WPA2 credentials.
  //WiFi.setCredentials("OpenWrt", "helloworld");

  // Connect to WiFi using the credentials stored, or find a new network.
  initialConnect();

  //If this device has an id of 4 store it in memory.
  //Only do this once.
  //int put_id = 22; // 4 bytes
  //EEPROM.put(id_addr, put_id);
  //Get the id of this device.
  EEPROM.get(id_addr, id);
  String id_stri = String::format("ny_t%d", id); //Create the line to log
  Particle.publish("id_d", id_stri, PRIVATE);


  // variable name max length is 12 characters long

  //Alive varible
  Particle.variable("areyoualive", message);

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

  //Maker Kit tutorial
  //Shortend version of actual code; see url below for full code
  //https://docs.particle.io/tutorials/projects/maker-kit/#tutorial-4-temperature-logger

  //Send in 10min intervals
  if(abs(Time.minute()-lastSendTemp) < 10)
  {
    return;
  }

  byte i;
  byte present = 0;
  byte data[12];
  byte addr[8];
  float celsius, fahrenheit;



  //Chip = DS18B20
  if ( !ds.search(addr)) {
     Serial.println("No more addresses.");
     Serial.println();
     ds.reset_search();
     checkIns();
     delay(250);
     return;
   }


  // second the CRC is checked, on fail,
  // print error and just return to try again

  if (OneWire::crc8(addr, 7) != addr[7]) {
    Serial.println("CRC is not valid!");
    checkIns();
    return;
  }
  checkIns();
  // we have a good address at this point
  // we will check if its the correct chip or just return
  if (addr[0] != 0x28)
  {
    return;
  }
  checkIns();
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
