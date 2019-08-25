// OpenTherm handling task class

class OTHandleTask: public Task {

private:
  unsigned long new_ts = 0;
  unsigned long ts = 0;
  unsigned long send_ts = 0;
  unsigned long send_newts = 0;
  unsigned long loop_counter = 0;
  float 
       pv_last = 0,                              // предыдущая температура
       ierr = 0,                                    // интегральная погрешность
       dt = 0;                                      // время между измерениями




public:
void static ICACHE_RAM_ATTR handleInterrupt() {
  ot.handleInterrupt();
}


void static responseCallback(unsigned long result,OpenThermResponseStatus status)
{
  DEBUG.println("Status of response " + String(status));
  DEBUG.println("Result of response " + String(result));
  response = result;
}
  protected:

//===============================================================
//              Вычисляем коэффициенты ПИД регулятора
//===============================================================
float pid(float sp, float pv, float pv_last, float& ierr, float dt) {
  float Kc = 10.0; // K / %Heater
  float tauI = 50.0; // sec
  float tauD = 1.0;  // sec
  // ПИД коэффициенты
  float KP = Kc;
  float KI = Kc / tauI;
  float KD = Kc * tauD;
  // верхняя и нижняя границы уровня нагрева
  float ophi = 100;
  float oplo = 0;
  // вычислить ошибку
  float error = sp - pv;
  // calculate the integral error
  ierr = ierr + KI * error * dt;
  // вычислить производную измерения
  float dpv = (pv - pv_last) / dt;
  // рассчитать выход ПИД регулятора
  float P = KP * error;                      // пропорциональная составляющая
  float I = ierr;                                  // интегральная составляющая
  float D = -KD * dpv;                      // дифференциальная составляющая
  float op = P + I + D;
  // защита от сброса
  if ((op < oplo) || (op > ophi)) {
    I = I - KI * error * dt;
    // выход регулятора, он же уставка для ID-1 (температура теплоносителя контура СО котла)
    op = max(oplo, min(ophi, op));
  }
  ierr = I;
  DEBUG.println("Заданное значение температуры в помещении = " + String(sp) + " °C");
  DEBUG.println("Текущее значение температуры в помещении = " + String(pv) + " °C");
  DEBUG.println("Выхов ПИД регулятора = " + String(op));
  return op;
}


float getBoilerTemp() {
  unsigned long response = sendRequest(ot.buildGetBoilerTemperatureRequest());
  return ot.getTemperature(response);
}
float getDHWTemp() {
  unsigned long request26 = ot.buildRequest(OpenThermRequestType::READ, OpenThermMessageID::Tdhw, 0);
  unsigned long respons26 = sendRequest(request26);
  uint16_t dataValue26 = respons26 & 0xFFFF;
  float result26 = dataValue26 / 256;
  return result26;
}
float getOutsideTemp() {
  unsigned long request27 = ot.buildRequest(OpenThermRequestType::READ, OpenThermMessageID::Toutside, 0);
  unsigned long respons27 = sendRequest(request27);
  uint16_t dataValue27 = respons27 & 0xFFFF;
  if (dataValue27 > 32768) {
    //negative
    float result27 = -(65536 - dataValue27) / 256;
    return result27;
  } else {
    //positive
    float result27 = dataValue27 / 256;
    return result27;
  }
}
float setDHWTemp(float val) {
  unsigned int  val_in_hex = (val * 256 * 16 / 16);
  unsigned long request56 = ot.buildRequest(OpenThermRequestType::WRITE, OpenThermMessageID::TdhwSet, val_in_hex);
  unsigned long respons56 = sendRequest(request56);
  uint16_t dataValue56 = respons56 & 0xFFFF;
  float result56 = dataValue56 / 256;
  return result56;
}
unsigned int getFaultCode() {
  unsigned long request5 = ot.buildRequest(OpenThermRequestType::READ, OpenThermMessageID::ASFflags, 0);
  unsigned long respons5 = sendRequest(request5);
  uint8_t dataValue5 = respons5 & 0xFF;
  unsigned result5 = dataValue5;
  return result5;
}

unsigned int resetBoiler()
{
  unsigned long request = ot.buildRequest(OpenThermRequestType::WRITE, OpenThermMessageID::Command, 1);
  unsigned long response = sendRequest(request);
  uint8_t dataValue = response & 0xFF;
  unsigned result = dataValue;
  return result; 
}

unsigned long sendRequest(unsigned long request) {
    send_newts= millis();
    if(send_newts - send_ts < 100) {
      // Преждем чем слать что то - надо подождать 100ms согласно специфиикации протокола ОТ
      delay(100 - send_newts - send_ts);
    }
    ot.sendRequestAync(request);
      while (!ot.isReady()) {
      ot.process();
      yield(); // This is local Task yield() call which allow us to switch to another task in scheduler
    }
    send_ts = millis();
    return response;  // Response is global variable
}

// Main blocks here
  void setup() {
    // Setting up
    ot.begin(handleInterrupt,responseCallback);
  }


  // Main OpenTherm loop
  void loop()
  {
      new_ts = millis();
  if (new_ts - ts > 1000) {
    if(vars.heater_mode.isChanged()) {
      switch(vars.heater_mode.value) {
        case 0: // All off
          vars.enableCentralHeating.value = false;
          vars.enableHotWater.value = false;
          break;
        case 1 : // Summer mode
          vars.enableCentralHeating.value = false;
          vars.enableHotWater.value = true; 
          break;
        case 2:
          vars.enableCentralHeating.value = true;
          vars.enableHotWater.value = true; 
          break;
        default: // No changes
          break;                    
      }
    }
    unsigned long statusRequest = ot.buildSetBoilerStatusRequest(vars.enableCentralHeating.value, vars.enableHotWater.value, vars.enableCooling.value, vars.enableOutsideTemperatureCompensation.value, vars.enableCentralHeating2.value);
    unsigned long statusResponse = sendRequest(statusRequest);
    vars.isHeatingEnabled.value = ot.isCentralHeatingActive(statusResponse);
    vars.isDHWenabled.value = ot.isHotWaterActive(statusResponse);
    vars.isCoolingEnabled.value = ot.isCoolingActive(statusResponse);
    vars.isFlameOn = ot.isFlameOn(statusResponse);
    vars.isFault = ot.isFault(statusResponse);
    vars.isDiagnostic = ot.isDiagnostic(statusResponse);


    dt = (new_ts - ts) / 1000;
    ts = new_ts;
    OpenThermResponseStatus responseStatus = ot.getLastResponseStatus();

        if (responseStatus == OpenThermResponseStatus::SUCCESS) {
          //=======================================================================================
          // Эта группа элементов данных определяет информацию о конфигурации как на ведомых, так 
          // и на главных сторонах. Каждый из них имеет группу флагов конфигурации (8 бит) 
          // и код MemberID (1 байт). Перед передачей информации об управлении и состоянии
          // рекомендуется обмен сообщениями о допустимой конфигурации ведомого устройства 
          // чтения и основной конфигурации записи. Нулевой код MemberID означает клиентское
          // неспецифическое устройство. Номер/тип версии продукта следует использовать в сочетании
          // с "кодом идентификатора участника", который идентифицирует производителя устройства.
          //=======================================================================================
          unsigned long request3 = ot.buildRequest(OpenThermRequestType::READ, OpenThermMessageID::SConfigSMemberIDcode, 0xFFFF);
          unsigned long response3 = sendRequest(request3);
          uint8_t SlaveMemberIDcode =  response3 >> 0 & 0xFF;
          uint8_t flags = (response3 & 0xFFFF) >> 8 & 0xFF;
          vars.dhw_present.value = flags & 0x01;  
          vars.control_type.value = flags & 0x02;  
          vars.cooling_present.value = flags & 0x04;
          vars.dhw_tank_present.value = flags & 0x08; 
          vars.pump_control_present.value = flags & 0x10; 
          vars.ch2_present.value = flags & 0x20; 

          unsigned long request2 = ot.buildRequest(OpenThermRequestType::WRITE, OpenThermMessageID::MConfigMMemberIDcode, SlaveMemberIDcode);
          unsigned long respons2 = sendRequest(request2);
          
          unsigned long request127 = ot.buildRequest(OpenThermRequestType::READ, OpenThermMessageID::SlaveVersion, 0);
          unsigned long respons127 = sendRequest(request127);
          uint16_t dataValue127_type = respons127 & 0xFFFF;
          uint8_t dataValue127_num = respons127 & 0xFF;
          unsigned result127_type = dataValue127_type / 256;
          unsigned result127_num = dataValue127_num;

          unsigned long request126 = ot.buildRequest(OpenThermRequestType::WRITE, OpenThermMessageID::MasterVersion, 0x013F);
          unsigned long respons126 = sendRequest(request126);
          uint16_t dataValue126_type = respons126 & 0xFFFF;
          uint8_t dataValue126_num = respons126 & 0xFF;
          unsigned result126_type = dataValue126_type / 256;
          unsigned result126_num = dataValue126_num;

          DEBUG.println("Тип и версия термостата: тип " + String(result126_type) + ", версия " + String(result126_num));
          DEBUG.println("Тип и версия котла: тип " + String(result127_type) + ", версия " + String(result127_num));
        }

        if (responseStatus == OpenThermResponseStatus::SUCCESS) {
          if(vars.heater_mode.value == 2) {
           if(vars.house_temp_compsenation.value)
           {
            float op = pid(vars.heat_temp_set.value, vars.house_temp.value, pv_last, ierr, dt);
            pv_last =  vars.house_temp.value;
            unsigned long  setTempRequest = ot.buildSetBoilerTemperatureRequest(op);
            sendRequest(setTempRequest); // Записываем заданную температуру СО, вычисляемую ПИД регулятором (переменная op)
           } else if(vars.heat_temp_set.isChanged()) 
            {
                  unsigned long  setTempRequest = ot.buildSetBoilerTemperatureRequest(vars.heat_temp_set.value);
                  sendRequest(setTempRequest); 
            }
          }

       }

          if (responseStatus == OpenThermResponseStatus::SUCCESS) {
            if(vars.heater_mode.value == 2 || vars.heater_mode.value == 1) 
            {
            if(vars.dhw_temp_set.isChanged()) 
              setDHWTemp(vars.dhw_temp_set.value);        // Записываем заданную температуру ГВС
            }
          }



          if (responseStatus == OpenThermResponseStatus::SUCCESS) {
            vars.outside_temp.setValue(getOutsideTemp());
          }


          if (responseStatus == OpenThermResponseStatus::SUCCESS) {
            vars.heat_temp.setValue( getBoilerTemp());
          }

          if (responseStatus == OpenThermResponseStatus::SUCCESS) {
            vars.dhw_temp.setValue(getDHWTemp());
          }

          
          if (responseStatus == OpenThermResponseStatus::SUCCESS && vars.isFault.value) {
            vars.dhw_temp.setValue(getFaultCode());
          }

          if (responseStatus == OpenThermResponseStatus::SUCCESS) {
            if(vars.BLOR.isChanged())
              resetBoiler();
          }

    
    }
  }

} OtHandler;
