// OpenTherm handling task class
#define TIMEOUT_TRESHOLD 10
static unsigned long timeout_count = 0;

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
  switch(status) {
  case OpenThermResponseStatus::TIMEOUT:
     timeout_count++;
     if(timeout_count > TIMEOUT_TRESHOLD) {
      vars.online.value = false;
      timeout_count = TIMEOUT_TRESHOLD;
     }
     break;
  case OpenThermResponseStatus::SUCCESS:
     timeout_count = 0;   
     vars.online.value = true;
     break;
  default:
     break;
  }     
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
  return ot.getTemperature(respons26);
}
float getOutsideTemp() {
  unsigned long request27 = ot.buildRequest(OpenThermRequestType::READ, OpenThermMessageID::Toutside, 0);
  return ot.getTemperature(sendRequest(request27));
}
float setDHWTemp(float val) {
  unsigned long request56 = ot.buildRequest(OpenThermRequestType::WRITE, OpenThermMessageID::TdhwSet, ot.temperatureToData(val));
  return ot.getTemperature(sendRequest(request56));
}

unsigned int getFaultCode() {
  unsigned long request5 = ot.buildRequest(OpenThermRequestType::READ, OpenThermMessageID::ASFflags, 0);
  unsigned long respons5 = sendRequest(request5);
  uint8_t dataValue5 = respons5 & 0xFF;
  unsigned result5 = dataValue5;
  return result5;
}


unsigned long sendRequest(unsigned long request) {
    send_newts= millis();
    if(send_newts - send_ts < 100) {
      // Преждем чем слать что то - надо подождать 100ms согласно специфиикации протокола ОТ
      delay(100 - (send_newts - send_ts));
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
    

      vars.enableCentralHeating.value = vars.heater_mode.value & 0x2;
      vars.enableHotWater.value = vars.heater_mode.value & 0x1;
    
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
           if(vars.house_temp_compsenation.value)
           {
            float op = pid(vars.heat_temp_set.value, vars.house_temp.value, pv_last, ierr, dt);
            pv_last =  vars.house_temp.value;
            unsigned long  setTempRequest = ot.buildSetBoilerTemperatureRequest(op);
            sendRequest(setTempRequest); // Записываем заданную температуру СО, вычисляемую ПИД регулятором (переменная op)
           } else 
            {
                  unsigned long  setTempRequest = ot.buildSetBoilerTemperatureRequest(vars.heat_temp_set.value);
                  sendRequest(setTempRequest); 
            }
          }

       

          if (responseStatus == OpenThermResponseStatus::SUCCESS) {
              setDHWTemp(vars.dhw_temp_set.value);        // Записываем заданную температуру ГВС

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

  // Верхняя и нижняя границы для регулировки установки TdhwSet-UB / TdhwSet-LB  (t°C)
          if (responseStatus == OpenThermResponseStatus::SUCCESS) {
            unsigned long request48 = ot.buildRequest(OpenThermRequestType::READ, OpenThermMessageID::TdhwSetUBTdhwSetLB, 0);
            unsigned long respons48 = sendRequest(request48);
            uint16_t dataValue48 = respons48 & 0xFFFF;
            vars.DHWsetpUpp.value = dataValue48 / 256;
            uint8_t dataValue48_ = respons48 & 0xFF;
            vars.DHWsetpLow.value = dataValue48_;
          }

  // Верхняя и нижняя границы для регулировки MaxTSet-UB / MaxTSet-LB (t°C)
          if (responseStatus == OpenThermResponseStatus::SUCCESS) {
            unsigned long request49 = ot.buildRequest(OpenThermRequestType::READ, OpenThermMessageID::MaxTSetUBMaxTSetLB, 0);
            unsigned long respons49 = sendRequest(request49);
            uint16_t dataValue49 = respons49 & 0xFFFF;
            vars.MaxCHsetpUpp.value = dataValue49 / 256;
            uint8_t dataValue49_ = respons49 & 0xFF;
            vars.MaxCHsetpLow.value = dataValue49_;
          }    
    }
  }

} OtHandler;
