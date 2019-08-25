
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

unsigned long MQTT_polling_interval = 30000;
unsigned long mqtt_ts = 0,mqtt_new_ts = 0;

class MainTaskClass: public Task {

  protected:

void static callback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0';
  String strTopic = String(topic);
  String strPayload = String((char*)payload);
  DEBUG.println("MQTT received = " + strPayload);
  DynamicJsonDocument json(JSON_OBJECT_SIZE(20));
  deserializeJson(json,strPayload);
  if (!json.isNull()) {
    if(json.containsKey("mode"))
    {
          vars.heater_mode.value = json["mode"]|0;
          shouldSaveConfig = true;
    }
    if(json.containsKey("heater_temp"))
    {
          vars.heat_temp_set.value = json["heater_temp"]|30.0;
          shouldSaveConfig = true;
    }
    if(json.containsKey("dhw_temp"))
    {
          vars.dhw_temp_set.value = json["dhw_temp"]|50.0;
          shouldSaveConfig = true;
    }   
    if(json.containsKey("house_temp_comp"))      
    {
          vars.house_temp_compsenation.value = json["house_temp_comp"]|true;  
          shouldSaveConfig = true;
    }
     if(json.containsKey("outside_temp_comp"))   
     {
          vars.enableOutsideTemperatureCompensation.value = json["outside_temp_comp"]|true; 
          shouldSaveConfig = true;
     } 
     if(json.containsKey("house_comp"))   
          vars.house_temp.value = json["house_temp"]|21.0;  
     if(json.containsKey("status"))   
          handleUpdateToMQTT(true); 
     if(json.containsKey("debug"))       
          debug = json["debug"]|false;      
     if(json.containsKey("reset"))       
          vars.BLOR.value = json["reset"]|false;  
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Соединяемся с  MQTT сервером ...");
    // Attempt to connect
    if (client.connect("opentherm", mqtt_user, mqtt_password)) {
      Serial.println("ok");

      // после подключения публикуем объявление...


      // ... и перезаписываем

      client.subscribe((vars.mqttTopicPrefix.value+"/cmnd").c_str());
//      client.subscribe("spdhw");
    } else {
      Serial.print("Ошибка, rc =");
      Serial.print(client.state());
      Serial.println(" Попробуем снова через 5 секунд");

      // Подождать 5 сек. перед повторной попыткой

      delay(5000);
    }
  }
}

void checkAndSaveConfig()
{
    //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonDocument json(JSON_OBJECT_SIZE(20));

    json["mqtt_server"] = mqtt_server;
    json["hostname"] = host;
    json["mqtt_port"] = mqtt_port;
    json["mqtt_user"] = mqtt_user;
    json["mqtt_password"] = mqtt_password;
    json["mqtt_prefix"] = vars.mqttTopicPrefix.value;
    json["mode"] = vars.heater_mode.value;
    json["heater_temp"] = vars.heat_temp_set.value;
    json["dhw_temp"] = vars.dhw_temp_set.value;
    json["house_temp_comp"] = vars.house_temp_compsenation.value;  
    json["outside_temp_comp"] = vars.enableOutsideTemperatureCompensation.value;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    serializeJson(json,Serial);
    serializeJson(json,configFile);
    configFile.close();
    shouldSaveConfig = false;
    //end save
  }
}

  void static handleUpdateToMQTT(bool now)
  {
    mqtt_new_ts = millis();
    if((mqtt_new_ts - mqtt_ts > MQTT_polling_interval) || now) {
      // publisish all MQTT stuff
      mqtt_ts = mqtt_new_ts;
      String payload;
      DynamicJsonDocument json(JSON_OBJECT_SIZE(20));
      json["mode"] = vars.heater_mode.value;
      json["heater_temp_set"] = vars.heat_temp_set.value;
      json["dhw_temp_set"] = vars.dhw_temp_set.value;
      json["heater_temp"] = vars.heat_temp.value;
      json["dhw_temp"] = vars.dhw_temp.value;
      json["house_temp_comp"] = vars.house_temp_compsenation.value;  
      json["outside_temp_comp"] = vars.enableOutsideTemperatureCompensation.value;
      json["outside_temp"] = vars.outside_temp.value;
      json["fault"] = vars.isFault.value;
      json["fault_code"] = vars.fault_code.value;
      json["flame"] = vars.isFlameOn.value;
      json["heating"] = vars.isHeatingEnabled.value;
      json["dhw"] = vars.isDHWenabled.value;
      json["cooling"] = vars.isCoolingEnabled.value;
      json["diagnostic"] = vars.isDiagnostic.value;
      serializeJson(json,payload);
      int msg_size = payload.length();
      DEBUG.println((vars.mqttTopicPrefix.value+"/status").c_str());
      DEBUG.println(payload);
      DEBUG.println(String("Payload length = ") + msg_size );
      bool result = client.beginPublish((vars.mqttTopicPrefix.value+"/status").c_str(),msg_size,false);
      int bytes_left = msg_size;
      int offset = 0;
      byte  * str = (byte *)payload.c_str();
      while(bytes_left > 0 || result != false) {
        int remaining = bytes_left > 64 ? 64 : bytes_left;
        result = client.write(str+offset,remaining);
        bytes_left -= remaining;
        offset += remaining;  
      }
      if(result)
         result = client.endPublish();

      DEBUG.println("Send message was " + result ? "successful" : "not successful");
    }
  }

  static String heaterMode()
  {
    switch(vars.heater_mode.value) {
      case 0:
          return "выключен";
          break;     
      case 1:
          return "лето";
          break;
      case 2:
          return "зима";
          break; 
      default:
          return "";          
    }
    return "";
  }
   static void handleRoot() {
     String reply = "Режим работы = " + heaterMode();
     reply += String("\nГорелка включена = ") + vars.isFlameOn.value;
     reply += String("\nОшибка котла  = ") + vars.isFault.value;
     reply += String("\nТемпература котла = ") + vars.heat_temp.value;
     reply += String("\nТемпература ГВС = ") + vars.dhw_temp.value;
     reply += String("\nТемпература на улице = ") + vars.outside_temp.value;
     reply += String("\nТемпература в доме = ") + vars.house_temp.value;     
     reply += String("\nУстановка котла = ") + vars.heat_temp_set.value;
     reply += String("\nУстановка ГВС = ") + vars.dhw_temp_set.value;
     httpServer.sendHeader("Content-Type", "text/plain; charset=utf-8");
     httpServer.send(200, "text/plain", reply);
   }
  
   void setup() {

    MDNS.begin(host);

    httpUpdater.setup(&httpServer);
    httpServer.on("/",handleRoot);
    httpServer.begin();

  MDNS.addService("http", "tcp", 80);
  Serial.printf("HTTPUpdateServer ready! Open http://%s.local/update in your browser\n", host);
    client.setServer(mqtt_server, String(mqtt_port).toInt());   // подключаемся к MQTT
    client.setCallback(callback);     
   }

   void loop() {
      // MQTT Loop
  if (!client.connected()) {
    reconnect();
  }
  checkAndSaveConfig();
  handleUpdateToMQTT(false);
  client.loop();
  httpServer.handleClient();
  MDNS.update();
  }
  
} MainTask;
