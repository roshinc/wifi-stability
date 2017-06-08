/*
* Project wifi-stability
* Description:
* Author:
* Date:
*/
#include "Particle.h"
#include "prototypes.h"
// reset the system after 60 seconds if the application is unresponsive
ApplicationWatchdog wd(60000, System.reset);

STARTUP(System.enableFeature(FEATURE_RETAINED_MEMORY));

// Use primary serial over USB interface for logging output
SerialLogHandler logHandler;

// Needed to make the system not try to connect to WiFi before
//executing setup()
SYSTEM_MODE(SEMI_AUTOMATIC);

// setup() runs once, when the device is first turned on.
// Executes on boot because we changed the system mode
void setup() {
  // Put initialization like pinMode and begin functions here.
  Log.info("Booting %s", System.version().c_str());
  System.on(all_events, handle_all_the_events);
  // Connect to WiFi using the credentials stored, or find a new network.
  initialConnect();
}

// loop() runs over and over again, as quickly as it can execute.
void loop() {

  // Keep-alive
  Particle.process();

  // Check if we still have network
  checkNetwork();
  //Reset the timer
  wd.checkin();
  // Keep-alive
  Particle.process();
}
