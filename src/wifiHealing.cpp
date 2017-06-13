#include "Particle.h"
#include "prototypes.h"

//We are going to ping this to see if we have Internet
IPAddress googleDNS(8,8,8,8);
const char *cloudHost = "api.particle.io";
bool findAndConnectRunning = false;
extern ApplicationWatchdog wd;
/*
Assuming all WiFi networks don't require authentication.
Note:
- Currently this may get stuck in a loop if there are 20+ invalid APs with no auth.
- No google dns (8,8,8,8) is synonyms to no Internet
- If cloud is lost, the we would go into a loop, which is handled in handleCloudEvent. (UNTESTED)
*/

/*
* Attempts to connect to WiFi using the credentials we already have.
*/
void initialConnect()
{
  // Turn WiFi on.
  WiFi.on();
  //WiFi.setCredentials("OpenWrt", "helloworld");
  // Connect to WiFi using the credentials stored and don't go into listening mode
  /*
  Note: WiFi.connect would only go into listening mode if there are no credentials stored,
  which is unlikely to happen, WIFI_CONNECT_SKIP_LISTEN is here just to make it explicit.
  */
  WiFi.connect(WIFI_CONNECT_SKIP_LISTEN);

  // WiFi can't connect with current credentials, find new ones.
  if(!WiFi.ready())
  {
    findAndConnect();
  }

  // if we don't have Internet go find an AP that does.
  if(!have_internet())
  {
    findAndConnect();
  }

  // Connect to the cloud.
  Particle.connect();
  // Keep-alive (probably redundant here)
  Particle.process();

  //WiFi connected, have Internet but no cloud, find another connection ( moved to handleCloudEvent())
  /*
  May not ever get called, as Particle.connect() would probably result in a loop if not connectible.
  However the only reason it would not be connectible would be that the cloud is down. In this instance the best thing to do
  might be to retry until succession. On the other hand what if we looses Internet after check for Internet,
  SEE handleCloudEvent().
  */
  if(!Particle.connected())
  {   // Get rid of the network we are connected to
    WiFi.disconnect();
    // Find a new network
    findAndConnect();
  }
  // Keep-alive
  Particle.process();
}

/* checkNetwork()
* Check if the connection is still alive.
*/
void checkNetwork()
{
  wd.checkin();
  //if we are connected to a WiFi network.
  if(WiFi.ready())
  {
    wd.checkin();
    //if we have Internet.
    if(have_internet())
    {
      wd.checkin();
      //if the cloud can't be reached.
      // *We might have just been ideal too long try to connect to the same network -> seems to just
      //hang so not going to do this.
      if(have_cloud())
      {
        wd.checkin();
        //if we don't have a connection to the cloud.
        if(Particle.connected())
        {
          wd.checkin();
          // Keep-alive
          Particle.process();
          return;
        }
      }
    }
  }
  //if we reach here then one of the previous checks have failed, so we have
  //to find new ones.
  findAndConnect();
  wd.checkin();
  // Keep-alive
  Particle.process();

}

/*
* Finds and tries to connect a new WiFi network. If there are more than 20 invalid open APs this will (might) fail.
*/
int findAndConnect()
{
  if(findAndConnectRunning)
  {
    return -2;
  }
  wd.checkin();
  findAndConnectRunning = true;
  Log.info("findAndConnect called.");
  // Turn WiFi on. photon may turn wifi off if connection fails twice and may not power it back on,
  // not definitive and might be redundant if it does.
  WiFi.on();
  //Array to store APs returned by our scan.
  WiFiAccessPoint aps[20];
  // Used for limiting the times we scan and attempt to connect before hard resetting the WiFi
  int whileRun = 0;
  // The WiFi is not connected, or we have no Internet, or the cloud is not connected.
  while(1)
  {
    wd.checkin();
    WiFi.on();

    // On the third run, reset the WiFi.
    if(whileRun > 3)
    {
      Log.info("findAndConnect while loop ran 3 times, reseting wifi.");
      WiFi.off();
      delay(100); //100ms
      WiFi.on();
      whileRun = 0; // Reset the counter
    }
    //if (whileRunReset >) {
    //  /* code */
    //}
    // Forget the networks we know.
    WiFi.clearCredentials();
    // Scan for 20 networks (at most 20, 'found' holds the # of the actual APs found) and put it into aps array.
    int found = WiFi.scan(aps, 20);
    Log.info("findAndConnect found %d APs", found);
    // Sort the array to have the strongest first.
    sortAPs(aps, found);
    // For every APS found, try to connect to it.
    for (int i=0; i<found; i++)
    {
      wd.checkin();
      // Get the 'i'th ap from the array
      WiFiAccessPoint& ap = aps[i];
      // Because we assume no auth on APs, filter by security protocol
      if (ap.security == 0)
      {
        Log.info("Trying to connect to ");

        Log.info("SSID: %s", ap.ssid);
        Log.info("Security: %d", ap.security);
        Log.info("Channel: %d", ap.channel);
        Log.info("RSSI: %d", ap.rssi);

        // Set this ap as a credential
        WiFi.setCredentials(ap.ssid);
        //Don't go into listning mode
        WiFi.connect(WIFI_CONNECT_SKIP_LISTEN);
        wd.checkin();
        // if the network is connectible
        if(WiFi.ready())
        {
          Log.info("WiFi is good!");
          wd.checkin();
          //Check if we have Internet
          if (have_internet())
          {
            Log.info("Internet is good!");
            wd.checkin();
            //Connected to Wifi and we have Internet, try to connect to cloud
            Particle.connect();
            //WiFi connected but and we have Internet.
            if(Particle.connected())
            {
              Log.info("Cloud is good!");
              wd.checkin();
              // Keep-alive (probably redundant here)
              Particle.process();
              findAndConnectRunning = false;
              return 0;
            }
            else
            {
              Log.error("Cloud is Bad!");
              wd.checkin();
            }
          }
          else
          {
            Log.error("Internet is bad!");
            wd.checkin();
            noNetworkLog();
          }
        }
        else
        {
          Log.error("WiFi is bad!");
          wd.checkin();
          noNetworkLog();
        }

        // WiFi could not connect or WiFi connected but no internet or both are
        // fine and no cloud,
        // find another connection

        // Get rid of the network we are connected to, if any
        WiFi.disconnect();
        //Forget the networks we know.
        WiFi.clearCredentials();
        wd.checkin();
        Log.error("Could not connect to %s, moving on...", ap.ssid);
      }// End of if
    }// End of for
    whileRun++;
    wd.checkin();
    Serial.println("while loop");
  }// End of while
  findAndConnectRunning = false;
  return -1;
}
/*
Called whenever a system event is raised, used primarly for logging.
References:
- https://github.com/spark/firmware/blob/develop/system/inc/system_event.h
- https://docs.particle.io/reference/firmware/photon/#system-events-reference
*/
void handle_all_the_events(system_event_t event, int param)
{
  //network status events
  if (event == network_status)
  {
    if(param==network_status_powering_off)
    {
      Log.info("network_status_powering_off");
    }
    if(param==network_status_off)
    {
      Log.info("network_status_off");
    }
    if(param==network_status_powering_on)
    {
      Log.info("network_status_powering_on");
    }
    if(param==network_status_on)
    {
      Log.info("network_status_on");
    }
    if(param==network_status_connecting)
    {
      Log.info("network_status_connecting");
    }
    if(param==network_status_connected)
    {
      Log.info("network_status_connected");
    }
    if(param==network_status_disconnecting)
    {
      Log.info("network_status_disconnecting");
    }
    if(param==network_status_disconnected)
    {
      Log.info("network_status_disconnected");
    }
  }


  //Cloud events
  if(event == cloud_status)
  {
    if(param==cloud_status_disconnected)
    {
      Log.warn("cloud_status_disconnected");
      //Log.error("calling find and connect from cs %s", findAndConnectRunning);
      //findAndConnect();
    }
    //Already logged by system.
    //if(param==cloud_status_connecting)
    //{
    //  Log.info("cloud_status_connecting");
    //}
    //if(param==cloud_status_connected)
    //{
    //  Log.info("cloud_status_connected");
    //}
    if(param==cloud_status_disconnecting)
    {
      Log.info("cloud_status_disconnecting");
    }
  }


  //Network credentials events
  if (event==network_credentials)
  {
    if(param==network_credentials_added)
    {
      Log.info("network_credentials_added");
    }
    if(param == network_credentials_cleared)
    {
      Log.info("network_credentials_cleared");
    }
  }
  if (event==firmware_update)
  {
    if(param== firmware_update_failed)
    {
      Log.info("firmware_update_failed");
    }
    if(param==firmware_update_begin)
    {
      Log.info("firmware_update_begin");
    }
    if(param==firmware_update_complete)
    {
      Log.info("firmware_update_complete");
    }
  }

  if (event==time_changed)
  {
    if(param==time_changed_manually)
    {
      Log.info("time_changed_manually");
    }
    if(param==time_changed_sync)
    {
      Log.info("time_changed_sync");
    }
  }
  if (event==reset) {
    Log.info("reset");
  }

  //Serial.println(WiFi.localIP());
  //Serial.println(WiFi.dhcpServerIP());
}

// Called when there network contivitty fails
// logs a dns, ping request and also the signal strength of the current wifi.
void noNetworkLog()
{
  IPAddress addr;
  Log.warn("=== noNetworkLog start ===");
  Log.info("ssid %s", WiFi.SSID());
  Log.info("rssi %d", WiFi.RSSI());
  wd.checkin();
  Log.info("ping %s %d" , googleDNS.toString().c_str(), WiFi.ping(googleDNS, 1));
  addr = WiFi.resolve(cloudHost);
  Log.info("dns %s %s", cloudHost, addr.toString().c_str());
  wd.checkin();
  Log.warn("=== noNetworkLog end ===");
  wd.checkin();
  // api.particle.io doesn't respond to ping, so this would always fail
  // logBuffer.queue(true, "ping,%s,%d", addr.toString().c_str(), WiFi.ping(addr, 1));
}

/*
* Check if there is Internet by pinging 8.8.8.8
* Returns true if there is Internet; false if not.
*/
bool have_internet()
{
  wd.checkin();
  // Should return the number of hops.
  if (WiFi.ping(googleDNS, 5) == 0)
  {
    wd.checkin();
    Log.error("googleDNS was not pingable.");
    return false;
  }
  wd.checkin();
  return true;

}

/*
* See if we have cloud by resolving "api.particle.io"
* Returns true if we can resolve it; false otherwise.
*/
bool have_cloud()
{
  IPAddress ip = WiFi.resolve(cloudHost);
  wd.checkin();
  if(ip)
  {
    wd.checkin();
    return true;
  }
  else
  {
    wd.checkin();
    Log.error("The cloud could not resolved.");
    return false;
  }
}

/*
* Sort the list of APs to be strongest first.
* Just a simple bubble sort, -1dB (strong) to -127 (weak)
*/
void sortAPs(WiFiAccessPoint aps[], int foundAPs)
{
  int i, j, flag = 1;    // set flag to 1 to start first pass
  WiFiAccessPoint temp;             // holding variable
  for(i = 1; (i <= foundAPs) && flag; i++)
  {
    wd.checkin();
    flag = 0;
    for (j=0; j < (foundAPs -1); j++)
    {
      wd.checkin();
      if (aps[j+1].rssi > aps[j].rssi)
      {
        temp = aps[j];             // swap
        aps[j] = aps[j+1];
        aps[j+1] = temp;
        flag = 1;               //swap occurred.
      }
    }
  }
}
