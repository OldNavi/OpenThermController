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
#define HEATER_TEMP_SET MODE_VAR + 4
#define BOILER_TEMP_SET HEATER_TEMP_SET + 4
#define HOUSE_TEMP_COMP BOILER_TEMP_SET + 4
#define OTC_COMP HOUSE_TEMP_COMP + 1
#define HEATER_ENABLE OTC_COMP + 1
#define CURVE_K HEATER_ENABLE + 4
#define MONITOR CURVE_K + 1


// EEPROM_Rotate EEPROMr;

template <typename V>
struct VARIABLE
{
  V value;
  size_t size() { return sizeof(V); }
  byte *addr() { return (byte *)&this->value; }
  VARIABLE()
  {
  }
  VARIABLE(V val)
  {
    value = val;
  }
  void setValue(V val)
  {
    value = val;
  }
};

static struct MAIN_VARIABLES
{
  VARIABLE<bool> online = VARIABLE<bool>(false);
  VARIABLE<int> mode = VARIABLE<int>(0); // Режим регулятора 0-ПИД, 1-Эквитермические кривая ,2-уквитермическая кривая с учетом темрератур
  VARIABLE<float> heat_temp_set;
  VARIABLE<float> house_temp;
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

private:
  EEPROM_Rotate eprom;

  void EEPROM_read(int offset, byte *ptr, size_t size)
  {
    for (size_t i = 0; i < size; i++)
      ptr[i] = eprom.read(offset + i);
  }

  void EEPROM_write(int offset, byte *ptr, size_t size)
  {
    for (size_t i = 0; i < size; i++)
      eprom.write(offset + i, ptr[i]);
  }

public:
  MAIN_VARIABLES()
  {
    eprom.size(4);
    eprom.begin(32);
  }

  void write()
  {

    // Еще не разу не писали значения по умолчанию
    eprom.write(INIT_VAR, 'O');
    eprom.write(INIT_VAR + 1, 'T');
    EEPROM_write(MODE_VAR, mode.addr(), mode.size());
    EEPROM_write(HEATER_TEMP_SET, heat_temp_set.addr(), heat_temp_set.size());
    EEPROM_write(BOILER_TEMP_SET, dhw_temp_set.addr(), dhw_temp_set.size());
    EEPROM_write(HOUSE_TEMP_COMP, house_temp_compsenation.addr(), house_temp_compsenation.size());
    EEPROM_write(OTC_COMP, enableOutsideTemperatureCompensation.addr(), enableOutsideTemperatureCompensation.size());
    EEPROM_write(HEATER_ENABLE, enableCentralHeating.addr(), enableCentralHeating.size());
    EEPROM_write(CURVE_K, iv_k.addr(), iv_k.size());
    EEPROM_write(MONITOR, monitor_only.addr(), monitor_only.size());
    commit();
  }

  void read()
  {
    if (eprom.read(INIT_VAR) != 'O' && eprom.read(INIT_VAR + 1) != 'T')
    {
      Serial.println("Инициализируем данные по умолчанию...");
      write();
    }
    EEPROM_read(MODE_VAR, mode.addr(), mode.size());
    EEPROM_read(HEATER_TEMP_SET, heat_temp_set.addr(), heat_temp_set.size());
    EEPROM_read(BOILER_TEMP_SET, dhw_temp_set.addr(), dhw_temp_set.size());
    EEPROM_read(HOUSE_TEMP_COMP, house_temp_compsenation.addr(), house_temp_compsenation.size());
    EEPROM_read(OTC_COMP, enableOutsideTemperatureCompensation.addr(), enableOutsideTemperatureCompensation.size());
    EEPROM_read(HEATER_ENABLE, enableCentralHeating.addr(), enableCentralHeating.size());
    EEPROM_read(CURVE_K, iv_k.addr(), iv_k.size());
    EEPROM_read(MONITOR, monitor_only.addr(), monitor_only.size());
  }

  void commit()
  {
    eprom.commit();
  }

} vars; 
