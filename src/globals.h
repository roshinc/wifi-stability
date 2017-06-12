SdFat sd;
bool initilized = false;
// reset the system after 60 seconds if the application is unresponsive
ApplicationWatchdog wd(60000, System.reset);
