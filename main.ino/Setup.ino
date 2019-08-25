

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}


//==============================================================
//                  Подключаемся к сети WiFi
//==============================================================
void setup_wifi() {
 Serial.println();
  WiFi.mode(WIFI_STA);
  //clean FS, for testing
  //SPIFFS.format();

  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonDocument json(JSON_OBJECT_SIZE(20));
        deserializeJson(json,buf.get());
            serializeJson(json,Serial);
        if (!json.isNull()) {
          Serial.println("\nparsed json");
          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);
          strcpy(mqtt_user, json["mqtt_user"]);
          strcpy(mqtt_password, json["mqtt_password"]);
          strcpy(host, json["hostname"]|"opentherm");
          vars.mqttTopicPrefix.value = json["mqtt_prefix"]|String("opentherm");
          vars.heater_mode.value = json["mode"]|0;
          vars.heat_temp_set.value = json["heater_temp"]|30.0;
          vars.dhw_temp_set.value = json["dhw_temp"]|50.0;          
          vars.house_temp_compsenation.value = json["house_temp_comp"]|true;  
          vars.enableOutsideTemperatureCompensation.value = json["outside_temp_comp"]|true;  
        } else {
          Serial.println("failed to load json config");
        }
        configFile.close();
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //Конец чтения конфигурации
  
  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_hostname("hostname", "Specify hostname", host, 80);
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 80);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 6);
  WiFiManagerParameter custom_mqtt_user("user", "mqtt username", mqtt_user, 32);
  WiFiManagerParameter custom_mqtt_password("passwd", "mqtt password", mqtt_password, 32);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  
  //add all your parameters here
  wifiManager.addParameter(&custom_hostname);
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_mqtt_user);
  wifiManager.addParameter(&custom_mqtt_password);

  if (!wifiManager.autoConnect()) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("WiFi connected...");

  //read updated parameters
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(host, custom_hostname.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(mqtt_user, custom_mqtt_user.getValue());
  strcpy(mqtt_password, custom_mqtt_password.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonDocument json(1024);

    json["mqtt_server"] = mqtt_server;
    json["hostname"] = host;
    json["mqtt_port"] = mqtt_port;
    json["mqtt_user"] = mqtt_user;
    json["mqtt_password"] = mqtt_password;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    serializeJson(json,Serial);
    serializeJson(json,configFile);
    configFile.close();
    //end save
  }

  Serial.println("local ip");
  Serial.println(WiFi.localIP());
}
//==============================================================
//                  Функция SETUP
//==============================================================


void setup() {
  pinMode(BUILTIN_LED, OUTPUT);
  pinMode(EXTERNAL_LED, OUTPUT);
  pinMode(EXTERNAL_LED_1, OUTPUT);
  pinMode(EXTERNAL_LED_2, OUTPUT);
  pinMode(EXTERNAL_LED_3, OUTPUT);
  digitalWrite(BUILTIN_LED, LOW);
  digitalWrite(EXTERNAL_LED, LOW);
  digitalWrite(EXTERNAL_LED_1, LOW);
  digitalWrite(EXTERNAL_LED_2, LOW);
  digitalWrite(EXTERNAL_LED_3, LOW);
  Serial.begin(115200);
  setup_wifi();
  
  // Запускаем основные потоки - в одном обрабатываем сообщения OpenTherm
  // Второй поток отвечает за обслуживание всего остального 
  Scheduler.start(&OtHandler);
  Scheduler.start(&MainTask);

  Scheduler.begin();
}



//==============================================================
//                     Функция LOOP
//==============================================================
void loop() {
// Empty placeholder - never reached.

}
