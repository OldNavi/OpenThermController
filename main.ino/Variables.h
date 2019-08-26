#ifndef MQTT_MAX_PACKET_SIZE
#define MQTT_MAX_PACKET_SIZE   1000           // Bytes
#endif
#ifndef MQTT_KEEPALIVE
#define MQTT_KEEPALIVE         30             // Seconds
#endif
#ifndef MQTT_TIMEOUT
#define MQTT_TIMEOUT           10000          // milli seconds
#endif
#ifndef MQTT_CLEAN_SESSION
#define MQTT_CLEAN_SESSION     1              // 0 = No clean session, 1 = Clean session (default)
#endif


template<typename V> struct VARIABLE {
  V value; // Содержит текущкк используемое значение
  V prev_value; // Предыдущее значение - если отличается от текущего - будем вызывать действие
  bool isChanged() {
    bool result = value != prev_value; 
    if(result) prev_value = value;
    return result;
  }
  bool isChangedNoClear() {
    return value != prev_value; 
  }
  VARIABLE() {
    }
  VARIABLE(V val) {
    value = val;
    prev_value = val;
  }
  VARIABLE(V val,V prev) {
    value = val;
    prev_value = prev;
  }
  void setValue(V val) {
    value = val;
  }
};

static struct MAIN_VARIABLES {
  VARIABLE<bool>  online = VARIABLE<bool>(false);
  VARIABLE<int>   heater_mode;
  VARIABLE<float> heat_temp_set;
  VARIABLE<float> house_temp;
  VARIABLE<bool> house_temp_compsenation = VARIABLE<bool>(true);
  VARIABLE<float> heat_temp;
  VARIABLE<float> dhw_temp;
  VARIABLE<unsigned long> fault_code;
  VARIABLE<float> outside_temp;
  VARIABLE<float> dhw_temp_set = VARIABLE<float>(50.0); // Температура воды по умолчанию
  VARIABLE<bool> isHeatingEnabled;
  VARIABLE<bool> isDHWenabled;
  VARIABLE<bool> isCoolingEnabled;
  VARIABLE<bool> isFault;
  VARIABLE<bool> isFlameOn;
  VARIABLE<bool> isDiagnostic;
  VARIABLE<bool> dhw_present;   // false - not present, true - present
  VARIABLE<bool> control_type;  // false - modulation, true - on/off control
  VARIABLE<bool> cooling_present; // false - no, true - yes
  VARIABLE<bool> dhw_tank_present; // false  - no, true - yes
  VARIABLE<bool> pump_control_present; // false  - no, true - yes
  VARIABLE<bool> ch2_present; // false - not present, true - present
  VARIABLE<bool> enableCentralHeating = VARIABLE<bool>(true) ; 
  VARIABLE<bool> enableHotWater = VARIABLE<bool>(true) ;
  VARIABLE<bool> enableCooling = VARIABLE<bool>(false) ; 
  VARIABLE<bool> enableOutsideTemperatureCompensation = VARIABLE<bool>(true) ; 
  VARIABLE<bool> enableCentralHeating2 = VARIABLE<bool>(false) ; 
  VARIABLE<int>  DHWsetpUpp;
  VARIABLE<int>  DHWsetpLow;
  VARIABLE<int>  MaxCHsetpUpp;
  VARIABLE<int>  MaxCHsetpLow;
  VARIABLE<bool> dump_request = VARIABLE<bool>(false); 
  VARIABLE<String> mqttTopicPrefix = VARIABLE<String>(String("opentherm"));
} vars;  // Static declartion of variables we need

#define INIT_VAR 4
#define MODE_VAR 6
#define HEATER_TEMP_SET MODE_VAR+4
#define BOILER_TEMP_SET HEATER_TEMP_SET+4
#define HOUSE_TEMP_COMP BOILER_TEMP_SET+4
#define OTC_COMP    HOUSE_TEMP_COMP+1
