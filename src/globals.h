SdFat sd; //the sd card
bool initilized = false; // true if sd was initilized
// reset the system after 60 seconds if the application is unresponsive
ApplicationWatchdog wd(60000, System.reset); // system reboot watchdog
