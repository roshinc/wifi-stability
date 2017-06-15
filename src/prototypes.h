void initialConnect();
void checkNetwork();
int findAndConnect();
void handle_all_the_events(system_event_t event, int param);
void noNetworkLog();
bool have_internet();
bool have_cloud();
void sortAPs(WiFiAccessPoint aps[], int foundAPs);
void logCurrentNetworks();
