#ifndef __MAIN_H__
#define __MAIN_H__
#include <LittleFS.h>
#include <Scheduler.h>
#include "Variables.h"
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <WiFiManager.h>
#include <WiFiUdp.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <Ticker.h>
// #include <AsyncMqttClient.h>
#include <OpenTherm.h>
#include <ArduinoJson.h>

#include "MainTask.h"
#include "OpenThermTask.h"

//Входные и выходные контакты OpenTherm, подключены к 4 и 5 контактам платы
const int inPin = 4;  //D2
const int outPin = 5; //D1

#define BUILTIN_LED 2     // D4 Встроенный LED

extern bool debug; 
//flag for saving data
extern bool shouldSaveConfig; 

#define DEBUG  \
    if (debug) \
    Serial

#define INFO Serial

#define WARN Serial

extern char host[80];
extern char mqtt_server[80];
extern char mqtt_port[6];
extern char mqtt_user[34];
extern char mqtt_password[34];

extern OpenTherm ot;
extern PubSubClient client;
extern MainTaskClass  MainTask;
extern OTHandleTask  OtHandler;
extern WiFiEventHandler wifiConnectHandler;
extern WiFiEventHandler wifiDisconnectHandler;
extern Ticker wifiReconnectTimer;
#endif
/* Новый код добавляется из скетчей  */