SdFat sd; //the sd card
bool initilized = false; // true if sd was initilized
bool hlIsWriting = false; // true if HybridLogHandler is Writing
bool sdlIsWriting = false; // true if SDcardLogHandler is writing
// reset the system after 60 seconds if the application is unresponsive
ApplicationWatchdog wd(60000, System.reset); // system reboot watchdog
