#include "main.h"

OpenTherm ot(inPin, outPin);
WiFiClient espClient;
PubSubClient client(espClient);
MainTaskClass  MainTask;
OTHandleTask  OtHandler;
WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;
Ticker wifiReconnectTimer;

char host[80] = "opentherm";
char mqtt_server[80];
char mqtt_port[6] = "1883";
char mqtt_user[34];
char mqtt_password[34];


bool debug = false;
bool shouldSaveConfig = false; 