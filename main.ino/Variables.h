#include <EEPROM_Rotate.h>

#ifndef MQTT_MAX_PACKET_SIZE
#define MQTT_MAX_PACKET_SIZE 1000 // Bytes
#endif
#ifndef MQTT_KEEPALIVE
#define MQTT_KEEPALIVE 30 // Seconds
#endif
#ifndef MQTT_TIMEOUT
#define MQTT_TIMEOUT 10000 // milli seconds
#endif
#ifndef MQTT_CLEAN_SESSION
#define MQTT_CLEAN_SESSION 1 // 0 = No clean session, 1 = Clean session (default)
#endif

#define INIT_VAR 4
#define MODE_VAR 6
#define HEATER_TEMP_SET MODE_VAR + sizeof(int)
#define BOILER_TEMP_SET HEATER_TEMP_SET + sizeof(float)
#define HOUSE_TEMP_COMP BOILER_TEMP_SET + sizeof(float)
#define OTC_COMP HOUSE_TEMP_COMP + sizeof(bool)
#define HEATER_ENABLE OTC_COMP + sizeof(bool)
#define CURVE_K HEATER_ENABLE + sizeof(bool)
#define MONITOR CURVE_K + sizeof(float)
#define POLL_INTERVAL MONITOR + sizeof(bool)
#define POST_REC POLL_INTERVAL  + sizeof(long)


// EEPROM_Rotate EEPROMr;

template <typename V>
struct VARIABLE
{
  V value;

  VARIABLE()
  {
  }
  VARIABLE(V val)
  {
    value = val;
  }
};

static struct MAIN_VARIABLES
{
  VARIABLE<uint8_t> SlaveMemberIDcode;
  VARIABLE<uint8_t> MasterType;
  VARIABLE<uint8_t> MasterVersion;
  VARIABLE<uint8_t> SlaveType;
  VARIABLE<uint8_t> SlaveVersion;
  VARIABLE<bool> online = VARIABLE<bool>(false);
  VARIABLE<int> mode = VARIABLE<int>(0); // Режим регулятора 0-ПИД, 1-Эквитермические кривая ,2-уквитермическая кривая с учетом темрератур
  VARIABLE<float> heat_temp_set;
  VARIABLE<float> house_temp = VARIABLE<float>(24.0);
  VARIABLE<bool> house_temp_compsenation = VARIABLE<bool>(true);
  VARIABLE<float> heat_temp;
  VARIABLE<float> dhw_temp;
  VARIABLE<unsigned int> fault_code;
  VARIABLE<float> outside_temp;
  VARIABLE<float> dhw_temp_set = VARIABLE<float>(50.0); // Температура воды по умолчанию
  VARIABLE<float> control_set = VARIABLE<float>(0.0);
  VARIABLE<bool> isHeatingEnabled;
  VARIABLE<bool> isDHWenabled;
  VARIABLE<bool> isCoolingEnabled;
  VARIABLE<bool> isFault;
  VARIABLE<bool> isFlameOn;
  VARIABLE<bool> isDiagnostic;
  VARIABLE<bool> service_required = VARIABLE<bool>(false);
  VARIABLE<bool> lockout_reset = VARIABLE<bool>(false);
  VARIABLE<bool> low_water_pressure = VARIABLE<bool>(false);
  VARIABLE<bool> gas_fault = VARIABLE<bool>(false);
  VARIABLE<bool> air_fault = VARIABLE<bool>(false);
  VARIABLE<bool> water_overtemp = VARIABLE<bool>(false);
  VARIABLE<float> iv_k = VARIABLE<float>(1.5f);
  VARIABLE<bool> monitor_only = VARIABLE<bool>(false);
  VARIABLE<bool> post_recirculation  = VARIABLE<bool>(true);
  VARIABLE<bool> dhw_present;          // false - not present, true - present
  VARIABLE<bool> control_type;         // false - modulation, true - on/off control
  VARIABLE<bool> cooling_present;      // false - no, true - yes
  VARIABLE<bool> dhw_tank_present;     // false  - no, true - yes
  VARIABLE<bool> pump_control_present; // false  - no, true - yes
  VARIABLE<bool> ch2_present;          // false - not present, true - present
  VARIABLE<bool> enableCentralHeating = VARIABLE<bool>(false);
  VARIABLE<bool> enableHotWater = VARIABLE<bool>(true);
  VARIABLE<bool> enableCooling = VARIABLE<bool>(false);
  VARIABLE<bool> enableOutsideTemperatureCompensation = VARIABLE<bool>(true);
  VARIABLE<bool> enableCentralHeating2 = VARIABLE<bool>(false);
  VARIABLE<int> DHWsetpUpp;
  VARIABLE<int> DHWsetpLow;
  VARIABLE<int> MaxCHsetpUpp;
  VARIABLE<int> MaxCHsetpLow;
  VARIABLE<bool> dump_request = VARIABLE<bool>(false);
  VARIABLE<String> mqttTopicPrefix = VARIABLE<String>(String("opentherm"));
  VARIABLE<long> MQTT_polling_interval = VARIABLE<long>(30000);

private:
  EEPROM_Rotate eprom;

public:
  MAIN_VARIABLES()
  {
    eprom.size(4);
    eprom.begin(512);
  }

  void write()
  {
    // Еще не разу не писали значения по умолчанию
    eprom.write(INIT_VAR, 'O');
    eprom.write(INIT_VAR + 1, 'T');
    eprom.put(MODE_VAR, mode.value);
    eprom.put(HEATER_TEMP_SET, heat_temp_set.value);
    eprom.put(BOILER_TEMP_SET, dhw_temp_set.value);
    eprom.put(HOUSE_TEMP_COMP, house_temp_compsenation.value);
    eprom.put(OTC_COMP, enableOutsideTemperatureCompensation.value);
    eprom.put(HEATER_ENABLE, enableCentralHeating.value);
    eprom.put(CURVE_K, iv_k.value);
    eprom.put(MONITOR, monitor_only.value);
    eprom.put(POLL_INTERVAL, MQTT_polling_interval.value);
    eprom.put(POST_REC, post_recirculation.value);
    Serial.println("Пишем в EEPROM");
    Serial.println("Режим = " + String(mode.value));
    Serial.println("Уставка отопления = " + String(heat_temp_set.value));
    Serial.println("Уставка ГВС = " + String(dhw_temp_set.value));
    Serial.println("Компенсация внешней Температуры = " + String(enableOutsideTemperatureCompensation.value));
    Serial.println("Отопление = " + String(enableCentralHeating.value));
    Serial.println("Наклон кривой = " + String(iv_k.value));
    Serial.println("Мониторинг = " + String(monitor_only.value));
    Serial.println("Цикл обновления MQTT = " + String(MQTT_polling_interval.value));
    Serial.println("Рециркуляция = " + String(post_recirculation.value));
    commit();
  }

  void read()
  {
    if (eprom.read(INIT_VAR) != 'O' && eprom.read(INIT_VAR + 1) != 'T')
    {
      Serial.println("Инициализируем данные по умолчанию...");
      write();
    }
    mode.value = eprom.get(MODE_VAR,mode.value);
    heat_temp_set.value = eprom.get(HEATER_TEMP_SET,heat_temp_set.value);
    dhw_temp_set.value = eprom.get(BOILER_TEMP_SET,dhw_temp_set.value);
    house_temp_compsenation.value = eprom.get(HOUSE_TEMP_COMP,house_temp_compsenation.value);
    enableOutsideTemperatureCompensation.value = eprom.get(OTC_COMP,enableOutsideTemperatureCompensation.value);
    enableCentralHeating.value = eprom.get(HEATER_ENABLE,enableCentralHeating.value);
    iv_k.value = eprom.get(CURVE_K,iv_k.value);
    monitor_only.value = eprom.get(MONITOR,monitor_only.value);
    MQTT_polling_interval.value = eprom.get(POLL_INTERVAL,MQTT_polling_interval.value);
    post_recirculation.value = eprom.get(POST_REC,post_recirculation.value);
    Serial.println("Загружено из EEPROM");
    Serial.println("Режим = " + String(mode.value));
    Serial.println("Уставка отопления = " + String(heat_temp_set.value));
    Serial.println("Уставка ГВС = " + String(dhw_temp_set.value));
    Serial.println("Компенсация внешней Температуры = " + String(enableOutsideTemperatureCompensation.value));
    Serial.println("Отопление = " + String(enableCentralHeating.value));
    Serial.println("Наклон кривой = " + String(iv_k.value));
    Serial.println("Мониторинг = " + String(monitor_only.value));
    Serial.println("Цикл обновления MQTT = " + String(MQTT_polling_interval.value));
    Serial.println("Рециркуляция = " + String(post_recirculation.value));
  }

  void commit()
  {
    eprom.commit();
  }

} vars; 
