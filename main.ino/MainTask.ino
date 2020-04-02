
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

// unsigned long MQTT_polling_interval = 30000;
unsigned long mqtt_ts = 0, mqtt_new_ts = 0, lastReconnectAttempt = 0;
#include <netif/etharp.h>

void forceARP() {
  struct netif *netif = netif_list;
  while (netif)
  {
    etharp_gratuitous(netif);
    netif = netif->next;
  }
}

class MainTaskClass : public Task
{

protected:
  bool static handleIncomingJson(String payload)
  {
    StaticJsonDocument<JSON_OBJECT_SIZE(22)> json;
    deserializeJson(json, payload);
    bool needWrite = false;
    if (!json.isNull())
    {
      if (json.containsKey("mode"))
      {
        vars.mode.value = json["mode"] | 0;
        needWrite = true;
      }
      if (json.containsKey("heater_temp"))
      {
        vars.heat_temp_set.value = json["heater_temp"] | 30.0;
        needWrite = true;
      }
      if (json.containsKey("dhw_temp"))
      {
        vars.dhw_temp_set.value = json["dhw_temp"] | 50.0;
        needWrite = true;
      }
      if (json.containsKey("house_temp_comp"))
      {
        vars.house_temp_compsenation.value = json["house_temp_comp"] | true;
        needWrite = true;
      }
      if (json.containsKey("outside_temp_comp"))
      {
        vars.enableOutsideTemperatureCompensation.value = json["outside_temp_comp"] | true;
        needWrite = true;
      }
      if (json.containsKey("heater_enable"))
      {
        vars.enableCentralHeating.value = json["heater_enable"] | false;
        needWrite = true;
      }
      if (json.containsKey("curve_ratio"))
      {
        vars.iv_k.value = json["curve_ratio"] | 1.5f;
        needWrite = true;
      }
      if (json.containsKey("monitor"))
      {
        vars.monitor_only.value = json["monitor"] | false;
        needWrite = true;
      }
      if (json.containsKey("recirculation"))    
      {
        vars.post_recirculation.value = json["recirculation"] | false;
        needWrite = true;
      }
      if (json.containsKey("poll_interval"))
      {
        vars.MQTT_polling_interval.value = json["poll_interval"] | 30000;
        if(vars.MQTT_polling_interval.value < 1000) 
          vars.MQTT_polling_interval.value  = 1000;
        if(vars.MQTT_polling_interval.value > 120000) 
          vars.MQTT_polling_interval.value  = 120000;
        needWrite = true;
      }
      if (json.containsKey("house_temp"))
        vars.house_temp.value = json["house_temp"] | 23.0;
      if (json.containsKey("status"))
        handleUpdateToMQTT(true);
      if (json.containsKey("debug"))
        debug = json["debug"] | false;
      if (json.containsKey("dump"))
        vars.dump_request.value = json["dump"] | false;
      if (json.containsKey("reboot"))
        {
          ESP.restart();
        }
      if (needWrite)
      {
        vars.write();
      }
    }
    else
    {
      return false;
    }
    return true;
  }

  void static callback(char *topic, byte *payload, unsigned int length)
  {
    payload[length] = '\0';
    String strTopic = String(topic);
    String strPayload = String((char *)payload);
    DEBUG.println("MQTT received = " + strPayload);
    handleUpdateToMQTT(handleIncomingJson(strPayload));
  }

  void reconnect()
  {

    long now = millis();
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      // Attempt to reconnect

      Serial.print("Соединяемся с  MQTT сервером ...");
      // Attempt to connect
      if (client.connect("opentherm", mqtt_user, mqtt_password))
      {
        Serial.println("ok");
        // после подключения публикуем объявление...
        // ... и перезаписываем
        lastReconnectAttempt = 0;
        client.subscribe((vars.mqttTopicPrefix.value + "/cmnd").c_str());
        //      client.subscribe("spdhw");
      }
      else
      {
        Serial.print("Ошибка, rc =");
        Serial.print(client.state());
        Serial.println(" Попробуем снова через 5 секунд");
        forceARP();
      }
    }
  }

  void checkAndSaveConfig()
  {
    //save the custom parameters to FS
    if (shouldSaveConfig)
    {
      Serial.println("saving config");
      StaticJsonDocument<JSON_OBJECT_SIZE(30)> json;
      json["mqtt_server"] = mqtt_server;
      json["hostname"] = host;
      json["mqtt_port"] = mqtt_port;
      json["mqtt_user"] = mqtt_user;
      json["mqtt_password"] = mqtt_password;
      json["mqtt_prefix"] = vars.mqttTopicPrefix.value;

      File configFile = SPIFFS.open("/config.json", "w");
      if (!configFile)
      {
        Serial.println("failed to open config file for writing");
      }
      serializeJson(json, Serial);
      serializeJson(json, configFile);
      configFile.close();
      shouldSaveConfig = false;
      //end save
    }
  }

  String static handleOutJson()
  {
    String payload;
    StaticJsonDocument<JSON_OBJECT_SIZE(33)> json;
    json["mode"] = vars.mode.value;
    json["heater_temp_set"] = vars.heat_temp_set.value;
    json["control_set"] = String(vars.control_set.value,1);
    json["heater_enable"] = vars.enableCentralHeating.value;
    json["dhw_temp_set"] = vars.dhw_temp_set.value;
    json["heater_temp"] = vars.heat_temp.value;
    json["dhw_temp"] = vars.dhw_temp.value;
    json["house_temp_comp"] = vars.house_temp_compsenation.value;
    json["house_temp"] = vars.house_temp.value;
    json["recirculation"] = vars.post_recirculation.value;
    json["outside_temp_comp"] = vars.enableOutsideTemperatureCompensation.value;
    json["outside_temp"] = vars.outside_temp.value;
    json["fault"] = vars.isFault.value;
    json["fault_code"] = vars.fault_code.value;
    json["flame"] = vars.isFlameOn.value;
    json["heating"] = vars.isHeatingEnabled.value;
    json["dhw"] = vars.isDHWenabled.value;
    json["cooling"] = vars.isCoolingEnabled.value;
    json["diagnostic"] = vars.isDiagnostic.value;
    json["service_req"] = vars.service_required.value;
    json["blor_enabled"] = vars.lockout_reset.value;
    json["low_water_pressure"] = vars.low_water_pressure.value;
    json["gas_fault"] = vars.gas_fault.value;
    json["air_fault"] = vars.air_fault.value;
    json["water_overtemp"] = vars.water_overtemp.value;
    json["dhw_max_limit"] = vars.DHWsetpUpp.value;
    json["dhw_min_limit"] = vars.DHWsetpLow.value;
    json["heat_max_limit"] = vars.MaxCHsetpUpp.value;
    json["heat_min_limit"] = vars.MaxCHsetpLow.value;
    json["curve_ratio"] = vars.iv_k.value;
    json["monitor"] = vars.monitor_only.value;
    json["poll_interval"] = vars.MQTT_polling_interval.value;
    serializeJson(json, payload);
    return payload;
  }

  void static handleUpdateToMQTT(bool now)
  {
    mqtt_new_ts = millis();
    if ((mqtt_new_ts - mqtt_ts > vars.MQTT_polling_interval.value) || now)
    {
      // publisish all MQTT stuff
      mqtt_ts = mqtt_new_ts;
      String payload = handleOutJson();

      int msg_size = payload.length();
      DEBUG.println((vars.mqttTopicPrefix.value + "/status").c_str());
      DEBUG.println(payload);
      DEBUG.println(String("Payload length = ") + msg_size);
      bool result = client.beginPublish((vars.mqttTopicPrefix.value + "/state").c_str(), msg_size, false);
      int bytes_left = msg_size;
      int offset = 0;
      byte *str = (byte *)payload.c_str();
      while (bytes_left > 0 || result != false)
      {
        int remaining = bytes_left > 64 ? 64 : bytes_left;
        result = client.write(str + offset, remaining);
        bytes_left -= remaining;
        offset += remaining;
      }
      if (result)
        result = client.endPublish();
      client.publish((vars.mqttTopicPrefix.value + "/status").c_str(), vars.online.value ? "online" : "offline");
      DEBUG.println("Send message was " + result ? "successful" : "not successful");
      forceARP();
    }
  }

  static String heaterMode()
  {
    switch (vars.mode.value)
    {
    case 0:
      return "ПИД";
      break;
    case 1:
      return "Кривые";
      break;
    case 2:
      return "Кривые с учетом температуры";
      break;
    default:
      return "";
    }
    return "";
  }
  static void handleRoot()
  {
    String reply = "Режим работы регулятора = " + heaterMode();
    reply += String("\nСтатус = ") + (vars.online.value ? String("онлайн") : String("оффлайн"));
    reply += String("\nГорелка включена = ") + vars.isFlameOn.value;
    reply += String("\nТемпература котла = ") + vars.heat_temp.value;
    reply += String("\nТемпература ГВС = ") + vars.dhw_temp.value;
    reply += String("\nТемпература на улице = ") + vars.outside_temp.value;
    reply += String("\nТемпература в доме = ") + vars.house_temp.value;
    reply += String("\nУстановка котла = ") + vars.heat_temp_set.value;
    reply += String("\nУстановка ГВС = ") + vars.dhw_temp_set.value;
    reply += String("\nУстановка Котла при регуляторах = ") + vars.control_set.value;
    reply += String("\nОтопление разрешено  = ") + vars.isHeatingEnabled.value;
    reply += String("\nГВС разрешено = ") + vars.isDHWenabled.value;
    reply += String("\nОхлаждение разрешено = ") + vars.isCoolingEnabled.value;
    reply += String("\nКонтур отопления вкл  = ") + vars.enableCentralHeating.value;
    reply += String("\nКонтур ГВС вкл = ") + vars.enableHotWater.value;
    reply += String("\nКонтур Охлаждения вкл = ") + vars.enableCooling.value;
    reply += String("\nКомпенсация внешней температуры  = ") + (vars.enableOutsideTemperatureCompensation.value ? String("да") : String("нет"));
    reply += String("\nКомпенсация комнатной температуры  = ") + (vars.house_temp_compsenation.value ? String("да") : String("нет"));
    reply += String("\nСервис требуется  = ") + (vars.service_required.value ? String("да") : String("нет"));
    reply += String("\n ----------- Конфигурация ---------------");
    reply += String("\nГраницы установок контура отопления = от ") + vars.MaxCHsetpLow.value + " до " + vars.MaxCHsetpUpp.value;
    reply += String("\nГраницы установок контура ГВС = от ") + vars.DHWsetpLow.value + " до " + vars.DHWsetpUpp.value;
    reply += String("\nГВС встроен  = ") + vars.dhw_present.value;
    reply += String("\nТип управления = ") + (vars.control_type.value ? String("On/Off") : String("Модуляция"));
    reply += String("\nГВС  = ") + (vars.dhw_tank_present.value ? String("бак") : String("проточная"));
    reply += String("\nНаклон эквитермической кривой  = ") + vars.iv_k.value;
    reply += String("\n ----------- Статусы ---------------");
    reply += String("\nОшибка котла  = ") + vars.isFault.value;
    reply += String("\nКод ошибки  = E") + vars.fault_code.value;
    reply += String("\nСервис требуется  = ") + (vars.service_required.value ? String("да") : String("нет"));
    reply += String("\nУдаленный сброс разрешен  = ") + (vars.lockout_reset.value ? String("да") : String("нет"));
    reply += String("\nОшибка низкого давления воды  = ") + (vars.low_water_pressure.value ? String("да") : String("нет"));
    reply += String("\nОшибка по газу/огню  = ") + (vars.gas_fault.value ? String("да") : String("нет"));
    reply += String("\nОшибка по тяге воздуха  = ") + (vars.air_fault.value ? String("да") : String("нет"));
    reply += String("\nПерегрев теплоносителя = ") + (vars.water_overtemp.value ? String("да") : String("нет"));

    httpServer.sendHeader("Content-Type", "text/plain; charset=utf-8");
    httpServer.send(200, "text/plain", reply);
  }

  static void handleGetStatus()
  {
    httpServer.sendHeader("Content-Type", "application/json; charset=utf-8");
    httpServer.send(200, "text/plain", handleOutJson());
  }

  static void handlePostCommand()
  {
    handleUpdateToMQTT(handleIncomingJson(String(httpServer.arg("plain"))));
    httpServer.send(204, "text/plain", "");
  }

  void setup()
  {

    MDNS.begin(host);

    httpUpdater.setup(&httpServer);
    httpServer.on("/", handleRoot);
    httpServer.on("/state", handleGetStatus);
    httpServer.on("/command", HTTP_POST, handlePostCommand);
    httpServer.begin();

    MDNS.addService("http", "tcp", 80);
    Serial.printf("HTTPUpdateServer ready! Open http://%s.local/update in your browser\n", host);
    client.setServer(mqtt_server, String(mqtt_port).toInt()); // подключаемся к MQTT
    client.setCallback(callback);
  }

  void loop()
  {
    // MQTT Loop
    if (!client.connected())
    {
      reconnect();
    } else {
      handleUpdateToMQTT(false);
      client.loop();
    }
    checkAndSaveConfig();
    httpServer.handleClient();
    MDNS.update();
  }

} MainTask;