// OpenTherm handling task class
#define TIMEOUT_TRESHOLD 10
static unsigned long timeout_count = 0;
// OT Response
static  unsigned long ot_response = 0;

class OTHandleTask : public Task {

private:
  unsigned long new_ts = 0;
  unsigned long ts = 0;
  unsigned long send_ts = 0;
  unsigned long send_newts = 0;
  long loop_counter = 0;
  bool  recirculation = true;
  float
      pv_last = 0, // предыдущая температура
      ierr = 0,    // интегральная погрешность
      dt = 0;      // время между измерениями

public:
  void static ICACHE_RAM_ATTR handleInterrupt()
  {
    ot.handleInterrupt();
  }

  void static HandleReply(unsigned long response) {
    OpenThermMessageID id = ot.getDataID(response);
    uint8_t flags;
    switch (id)
    {
    case OpenThermMessageID::SConfigSMemberIDcode:
        vars.SlaveMemberIDcode.value = response >> 0 & 0xFF; 
        flags = (response & 0xFFFF) >> 8 & 0xFF;
        vars.dhw_present.value = flags & 0x01;
        vars.control_type.value = flags & 0x02;
        vars.cooling_present.value = flags & 0x04;
        vars.dhw_tank_present.value = flags & 0x08;
        vars.pump_control_present.value = flags & 0x10;
        vars.ch2_present.value = flags & 0x20;
        break;
    case OpenThermMessageID::Status:
        // Статус котла получен
        vars.isHeatingEnabled.value = ot.isCentralHeatingActive(response);
        vars.isDHWenabled.value = ot.isHotWaterActive(response);
        vars.isCoolingEnabled.value = ot.isCoolingActive(response);
        vars.isFlameOn.value = ot.isFlameOn(response);
        vars.isFault.value = ot.isFault(response);
        vars.isDiagnostic.value = ot.isDiagnostic(response);
        break;
    case OpenThermMessageID::Tboiler:
        // Получили температуру котла
        vars.heat_temp.value = ot.getTemperature(response);
        break;
    case OpenThermMessageID::Tdhw:
        // Получили температуру ГВС
        vars.dhw_temp.value = ot.getTemperature(response);
        break;
    case OpenThermMessageID::Toutside:
        // Получили внешнюю температуру
        vars.outside_temp.value = ot.getTemperature(response);
        break;
    case OpenThermMessageID::ASFflags:
        flags = (response & 0xFFFF) >> 8;
        vars.service_required.value = flags & 0x01;
        vars.lockout_reset.value = flags & 0x02;
        vars.low_water_pressure.value = flags & 0x04;
        vars.gas_fault.value = flags & 0x08;
        vars.air_fault.value = flags & 0x10;
        vars.water_overtemp.value = flags & 0x20;
        vars.fault_code.value = response & 0xFF;
        break;
    case OpenThermMessageID::SlaveVersion:
        vars.SlaveType.value = (response & 0xFFFF) >> 8;
        vars.SlaveVersion.value = response & 0xFF;
        break;
    case OpenThermMessageID::MasterVersion:
        vars.MasterType.value = (response & 0xFFFF) >> 8;
        vars.MasterVersion.value = response & 0xFF;
        break;
    case OpenThermMessageID::TdhwSetUBTdhwSetLB:
        vars.DHWsetpUpp.value = (response & 0xFFFF) >> 8;
        vars.DHWsetpLow.value = response & 0xFF;
        break;
    case OpenThermMessageID::MaxTSetUBMaxTSetLB:
        vars.MaxCHsetpUpp.value = (response & 0xFFFF) >> 8;
        vars.MaxCHsetpLow.value = response & 0xFF;
    default:
      break;
    }
  }

  void static responseCallback(unsigned long result, OpenThermResponseStatus status)
  {
    DEBUG.println("Status of response " + String(status));
    DEBUG.println("Result of response " + String(result));
    ot_response = result;
    switch (status)
    {
    case OpenThermResponseStatus::INVALID:
      WARN.println("Ошибочный ответ от котла");
      if(debug)
        client.publish((vars.mqttTopicPrefix.value + "/message").c_str(), "Ошибочный ответ от котла");
      break;
    case OpenThermResponseStatus::TIMEOUT:
      WARN.println("Таймаут ответа от котла");
      if(debug)
        client.publish((vars.mqttTopicPrefix.value + "/message").c_str(), "Таймаут ответа от котла");
      timeout_count++;
      if (timeout_count > TIMEOUT_TRESHOLD)
      {
        vars.online.value = false;
        timeout_count = TIMEOUT_TRESHOLD;
      }
      break;
    case OpenThermResponseStatus::SUCCESS:
      timeout_count = 0;
      vars.online.value = true;
      HandleReply(result);
      break;
    default:
      break;
    }
  }

protected:
  //===============================================================
  //              Вычисляем коэффициенты ПИД регулятора
  //===============================================================
  float pid(float sp, float pv, float pv_last, float &ierr, float dt)
  {
    float Kc = 5.0;   // K / %Heater
    float tauI = 50.0; // sec
    float tauD = 1.0;  // sec
    // ПИД коэффициенты
    float KP = Kc;
    float KI = Kc / tauI;
    float KD = Kc * tauD;
    // верхняя и нижняя границы уровня нагрева
    float ophi =  100;
    float oplo = 0;
    // вычислить ошибку
    float error = sp - pv;
    // calculate the integral error
    ierr = ierr + KI * error * dt;
    // вычислить производную измерения
    float dpv = (pv - pv_last) / dt;
    // рассчитать выход ПИД регулятора
    float P = KP * error; // пропорциональная составляющая
    float I = ierr;       // интегральная составляющая
    float D = -KD * dpv;  // дифференциальная составляющая
    float op = P + I + D;
    // защита от сброса
    if((op < oplo) || (op > ophi))
    {
      I = I - KI * error * dt;
      // выход регулятора, он же уставка для ID-1 (температура теплоносителя контура СО котла)
      op =  constrain(op, oplo, ophi);
    }
    ierr = I;
    DEBUG.println("Заданное значение температуры в помещении = " + String(sp) + " °C");
    DEBUG.println("Текущее значение температуры в помещении = " + String(pv) + " °C");
    DEBUG.println("Выхов ПИД регулятора = " + String(op));
    return op;
  }
  //===================================================================================================================
  //       Вычисляем температуру контура отпления, эквитермические кривые
  //===================================================================================================================
  float curve(float sp, float pv)
  {
    float a = (-0.21 * vars.iv_k.value) - 0.06;     // a = -0,21k — 0,06
    float b = (6.04 * vars.iv_k.value) + 1.98;      // b = 6,04k + 1,98
    float c = (-5.06 * vars.iv_k.value) + 18.06;    // с = -5,06k + 18,06
    float x = (-0.2 * vars.outside_temp.value) + 5; // x = -0.2*t1 + 5
    float temp_n = (a * x * x) + (b * x) + c;       // Tn = ax2 + bx + c
    // Расчетная температура конура отопления
    float op = temp_n; // T = Tn
    // Ограничиваем температуру для ID-1
    op =  constrain(op, 0, 100);
    return op;
  }
  //===================================================================================================================
  //       Вычисляем температуру контура отпления, эквитермические кривые с учётом влияния температуры в помещении
  //===================================================================================================================
  float curve2(float sp, float pv)
  {
    // Расчет поправки (ошибки) термостата
    float error = sp - pv; // Tt = (Tu — T2) × 5
    float temp_t = error * 3.0;
    // Поправка на желаемую комнатную температуру
    // Температура контура отопления в зависимости от наружной температуры
    float a = (-0.21 * vars.iv_k.value) - 0.06;     // a = -0,21k — 0,06
    float b = (6.04 * vars.iv_k.value) + 1.98;      // b = 6,04k + 1,98
    float c = (-5.06 * vars.iv_k.value) + 18.06;    // с = -5,06k + 18,06
    float x = (-0.2 * vars.outside_temp.value) + 5; // x = -0.2*t1 + 5
    float temp_n = (a * x * x) + (b * x) + c;       // Tn = ax2 + bx + c
    // Расчетная температура конура отопления
    float op = temp_n + temp_t; // T = Tn + Tk + Tt
    // Ограничиваем температуру для ID-1
    op =  constrain(op, 0, 100);
    return op;
  }

  bool getBoilerTemp()
  {
    unsigned long response;
    return sendRequest(ot.buildGetBoilerTemperatureRequest(),response);
  }

  bool getDHWTemp()
  {
    unsigned long response;
    unsigned long request = ot.buildRequest(OpenThermRequestType::READ, OpenThermMessageID::Tdhw, 0);
    return sendRequest(request,response);
  }

  bool getOutsideTemp()
  {
    unsigned long response;
    unsigned long request = ot.buildRequest(OpenThermRequestType::READ, OpenThermMessageID::Toutside, 0);
    return sendRequest(request,response);
  }

  bool setDHWTemp(float val)
  {
    unsigned long request = ot.buildRequest(OpenThermRequestType::WRITE, OpenThermMessageID::TdhwSet, ot.temperatureToData(val));
    unsigned long response;
    return sendRequest(request,response);
  }

  bool getFaultCode()
  {
    unsigned long response;
    unsigned long request = ot.buildRequest(OpenThermRequestType::READ, OpenThermMessageID::ASFflags, 0);
    return sendRequest(request,response);
  }

  bool sendRequest(unsigned long request, unsigned long& response)
  {
    send_newts = millis();
    if (send_newts - send_ts < 200)
    {
      // Преждем чем слать что то - надо подождать 100ms согласно специфиикации протокола ОТ
      delay(200 - (send_newts - send_ts));
    }
    bool result = ot.sendRequestAync(request);
    if(!result) {
      DEBUG.println("Не могу отправить запрос");
      DEBUG.println("Шина "+ ot.isReady() ? "готова" : "неготова");
      return false; 
      }
    while (!ot.isReady())
    {
      ot.process();
      yield(); // This is local Task yield() call which allow us to switch to another task in scheduler
    }
    send_ts = millis();
    response = ot_response;
    return true; // Response is global variable
  }

  // Main blocks here
  void setup()
  {
    // Setting up
    ot.begin(handleInterrupt, responseCallback);
  }

  void printRequestDetail(OpenThermMessageID id, unsigned long request, unsigned long response)
  {
    Serial.print("ID ");
    Serial.print(id);
    Serial.print(" Request - ");
    Serial.print(request, HEX);
    Serial.print(" Response - ");
    Serial.print(response, HEX);
    Serial.print(" Status: ");
    Serial.println(ot.statusToString(ot.getLastResponseStatus()));
  }

  void testSupportedIDs()
  {
    // Basic
    unsigned long request;
    unsigned long response;
    OpenThermMessageID id;
    //Command
    id = OpenThermMessageID::Command;
    request = ot.buildRequest(OpenThermRequestType::READ, id, 0);
    if(sendRequest(request,response))
      printRequestDetail(id, request, response);
    //ASFlags
    id = OpenThermMessageID::ASFflags;
    request = ot.buildRequest(OpenThermRequestType::READ, id, 0);
    if(sendRequest(request,response))
      printRequestDetail(id, request, response);

    //TrOverride
    id = OpenThermMessageID::TrOverride;
    request = ot.buildRequest(OpenThermRequestType::READ, id, 0);
    if(sendRequest(request,response))
      printRequestDetail(id, request, response);

    //TSP
    id = OpenThermMessageID::TSP;
    request = ot.buildRequest(OpenThermRequestType::READ, id, 0);
    if(sendRequest(request,response))
      printRequestDetail(id, request, response);

    //TSPindexTSPvalue
    id = OpenThermMessageID::TSPindexTSPvalue;
    request = ot.buildRequest(OpenThermRequestType::READ, id, 0);
    if(sendRequest(request,response))
      printRequestDetail(id, request, response);

    //FHBsize
    id = OpenThermMessageID::FHBsize;
    request = ot.buildRequest(OpenThermRequestType::READ, id, 0);
    if(sendRequest(request,response))
      printRequestDetail(id, request, response);

    //FHBindexFHBvalue
    id = OpenThermMessageID::FHBindexFHBvalue;
    request = ot.buildRequest(OpenThermRequestType::READ, id, 0);
    if(sendRequest(request,response))
      printRequestDetail(id, request, response);

    //MaxCapacityMinModLevel
    id = OpenThermMessageID::MaxCapacityMinModLevel;
    request = ot.buildRequest(OpenThermRequestType::READ, id, 0);
    if(sendRequest(request,response))
      printRequestDetail(id, request, response);

    //TrSet
    id = OpenThermMessageID::TrSet;
    request = ot.buildRequest(OpenThermRequestType::WRITE, id, ot.temperatureToData(21));
    if(sendRequest(request,response))
      printRequestDetail(id, request, response);

    //RelModLevel
    id = OpenThermMessageID::RelModLevel;
    request = ot.buildRequest(OpenThermRequestType::READ, id, 0);
    if(sendRequest(request,response))
      printRequestDetail(id, request, response);

    //CHPressure
    id = OpenThermMessageID::CHPressure;
    request = ot.buildRequest(OpenThermRequestType::READ, id, 0);
    if(sendRequest(request,response))
      printRequestDetail(id, request, response);

    //DHWFlowRate
    id = OpenThermMessageID::DHWFlowRate;
    request = ot.buildRequest(OpenThermRequestType::READ, id, 0);
    if(sendRequest(request,response))
      printRequestDetail(id, request, response);

    //DayTime
    id = OpenThermMessageID::DayTime;
    request = ot.buildRequest(OpenThermRequestType::READ, id, 0);
    if(sendRequest(request,response))
      printRequestDetail(id, request, response);

    //Date
    id = OpenThermMessageID::Date;
    request = ot.buildRequest(OpenThermRequestType::READ, id, 0);
    if(sendRequest(request,response))
      printRequestDetail(id, request, response);

    //Year
    id = OpenThermMessageID::Year;
    request = ot.buildRequest(OpenThermRequestType::READ, id, 0);
    if(sendRequest(request,response))
      printRequestDetail(id, request, response);

    //TrSetCH2
    id = OpenThermMessageID::TrSetCH2;
    request = ot.buildRequest(OpenThermRequestType::WRITE, id, ot.temperatureToData(21));
    if(sendRequest(request,response))
      printRequestDetail(id, request, response);

    //Tr
    id = OpenThermMessageID::Tr;
    request = ot.buildRequest(OpenThermRequestType::WRITE, id, ot.temperatureToData(21));
    if(sendRequest(request,response))
      printRequestDetail(id, request, response);

    //Tret
    id = OpenThermMessageID::Tret;
    request = ot.buildRequest(OpenThermRequestType::READ, id, 0);
    if(sendRequest(request,response))
      printRequestDetail(id, request, response);

    //Texhaust
    id = OpenThermMessageID::Texhaust;
    request = ot.buildRequest(OpenThermRequestType::READ, id, 0);
    if(sendRequest(request,response))
      printRequestDetail(id, request, response);

    //Hcratio
    id = OpenThermMessageID::Hcratio;
    request = ot.buildRequest(OpenThermRequestType::READ, id, 0);
    if(sendRequest(request,response))
      printRequestDetail(id, request, response);

    //RemoteOverrideFunction
    id = OpenThermMessageID::RemoteOverrideFunction;
    request = ot.buildRequest(OpenThermRequestType::READ, id, 0);
    if(sendRequest(request,response))
      printRequestDetail(id, request, response);

    //OEMDiagnosticCode
    id = OpenThermMessageID::OEMDiagnosticCode;
    request = ot.buildRequest(OpenThermRequestType::READ, id, 0);
    if(sendRequest(request,response))
      printRequestDetail(id, request, response);

    //BurnerStarts
    id = OpenThermMessageID::BurnerStarts;
    request = ot.buildRequest(OpenThermRequestType::READ, id, 0);
    if(sendRequest(request,response))
      printRequestDetail(id, request, response);

    //CHPumpStarts
    id = OpenThermMessageID::CHPumpStarts;
    request = ot.buildRequest(OpenThermRequestType::READ, id, 0);
    if(sendRequest(request,response))
      printRequestDetail(id, request, response);

    //DHWPumpValveStarts
    id = OpenThermMessageID::DHWPumpValveStarts;
    request = ot.buildRequest(OpenThermRequestType::READ, id, 0);
    if(sendRequest(request,response))
      printRequestDetail(id, request, response);

    //DHWBurnerStarts
    id = OpenThermMessageID::DHWBurnerStarts;
    request = ot.buildRequest(OpenThermRequestType::READ, id, 0);
    if(sendRequest(request,response))
      printRequestDetail(id, request, response);

    //BurnerOperationHours
    id = OpenThermMessageID::BurnerOperationHours;
    request = ot.buildRequest(OpenThermRequestType::READ, id, 0);
    if(sendRequest(request,response))
      printRequestDetail(id, request, response);

    //CHPumpOperationHours
    id = OpenThermMessageID::CHPumpOperationHours;
    request = ot.buildRequest(OpenThermRequestType::READ, id, 0);
    if(sendRequest(request,response))
      printRequestDetail(id, request, response);

    //DHWPumpValveOperationHours
    id = OpenThermMessageID::DHWPumpValveOperationHours;
    request = ot.buildRequest(OpenThermRequestType::READ, id, 0);
    if(sendRequest(request,response))
      printRequestDetail(id, request, response);

    //DHWBurnerOperationHours
    id = OpenThermMessageID::DHWBurnerOperationHours;
    request = ot.buildRequest(OpenThermRequestType::READ, id, 0);
    if(sendRequest(request,response))
      printRequestDetail(id, request, response);
  }

  // Main OpenTherm loop
  void loop()
  {
    new_ts = millis();
    if (new_ts - ts > 1000)
    {
      unsigned long localResponse;
      unsigned long localRequest = ot.buildSetBoilerStatusRequest(vars.enableCentralHeating.value & recirculation, vars.enableHotWater.value, vars.enableCooling.value, vars.enableOutsideTemperatureCompensation.value, vars.enableCentralHeating2.value);
      sendRequest(localRequest,localResponse);

      dt = (new_ts - ts) / 1000;
      ts = new_ts;

      if (vars.dump_request.value)
      {
        testSupportedIDs();
        vars.dump_request.value = false;
      }

      //=======================================================================================
      // Эта группа элементов данных определяет информацию о конфигурации как на ведомых, так
      // и на главных сторонах. Каждый из них имеет группу флагов конфигурации (8 бит)
      // и код MemberID (1 байт). Перед передачей информации об управлении и состоянии
      // рекомендуется обмен сообщениями о допустимой конфигурации ведомого устройства
      // чтения и основной конфигурации записи. Нулевой код MemberID означает клиентское
      // неспецифическое устройство. Номер/тип версии продукта следует использовать в сочетании
      // с "кодом идентификатора участника", который идентифицирует производителя устройства.
      //=======================================================================================
      localRequest = ot.buildRequest(OpenThermRequestType::READ, OpenThermMessageID::SConfigSMemberIDcode, 0xFFFF);
      sendRequest(localRequest, localResponse);

      if(!vars.monitor_only.value && ot.getLastResponseStatus() == OpenThermResponseStatus::SUCCESS && ot.getDataID(localResponse) == OpenThermMessageID::SConfigSMemberIDcode) 
      {
      localRequest = ot.buildRequest(OpenThermRequestType::WRITE, OpenThermMessageID::MConfigMMemberIDcode, vars.SlaveMemberIDcode.value);
      sendRequest(localRequest, localResponse);
      }
      
      localRequest = ot.buildRequest(OpenThermRequestType::READ, OpenThermMessageID::SlaveVersion, 0);
      sendRequest(localRequest, localResponse);

      localRequest = ot.buildRequest(OpenThermRequestType::WRITE, OpenThermMessageID::MasterVersion, 0x013F);
      sendRequest(localRequest, localResponse);

      DEBUG.println("Тип и версия термостата: тип " + String(vars.MasterType.value) + ", версия " + String(vars.MasterVersion.value));
      DEBUG.println("Тип и версия котла: тип " + String(vars.SlaveType.value) + ", версия " + String(vars.SlaveVersion.value));
      
      // Команды чтения данных котла
      getOutsideTemp();
      getBoilerTemp();
      getDHWTemp();
      getFaultCode();

      // Команды уставки
      setDHWTemp(vars.dhw_temp_set.value); // Записываем заданную температуру ГВС

      if(vars.monitor_only.value == false) {
        if (vars.house_temp_compsenation.value)
        {
          float op;
          switch (vars.mode.value)
          {
          case 0:
            // pid
            op = pid(vars.heat_temp_set.value, vars.house_temp.value, pv_last, ierr, dt);
            pv_last = vars.house_temp.value;
            vars.control_set.value = op;
            if(vars.post_recirculation.value) 
              recirculation = true;
            else 
              recirculation = (op >= vars.heat_temp_set.value);
            localRequest = ot.buildSetBoilerTemperatureRequest(op);
            sendRequest(localRequest, localResponse); // Записываем заданную температуру СО, вычисляемую ПИД регулятором (переменная op)
            break;
          case 1:
            // эквитермические кривые
            op = curve(vars.heat_temp_set.value, vars.house_temp.value);
            vars.control_set.value = op;
            if(vars.post_recirculation.value) 
              recirculation = true;
            else 
              recirculation = (op >= vars.heat_temp_set.value);
            localRequest = ot.buildSetBoilerTemperatureRequest(op);
            sendRequest(localRequest, localResponse); // Записываем заданную температуру СО, вычисляемую ПИД регулятором (переменная op)          
            break;
          case 2:
            // эквитермические кривые с учетом температуры
            op = curve2(vars.heat_temp_set.value, vars.house_temp.value);
            vars.control_set.value = op;
            if(vars.post_recirculation.value) 
              recirculation = true;
            else 
              recirculation = (op >= vars.heat_temp_set.value);
            localRequest = ot.buildSetBoilerTemperatureRequest(op);
            sendRequest(localRequest, localResponse); // Записываем заданную температуру СО, вычисляемую ПИД регулятором (переменная op)
            break;
          default:
            break;
          }
          // if(vars.control_set.value < vars.heat_temp_set.value)
        }
        else
        {
          localRequest = ot.buildSetBoilerTemperatureRequest(vars.heat_temp_set.value);
          sendRequest(localRequest, localResponse);
          recirculation = true; // No control over recirculation
        }
      }
      // Верхняя и нижняя границы для регулировки установки TdhwSet-UB / TdhwSet-LB  (t°C)
      if(loop_counter <= 0) {
      localRequest = ot.buildRequest(OpenThermRequestType::READ, OpenThermMessageID::TdhwSetUBTdhwSetLB, 0);
      sendRequest(localRequest, localResponse);
      

      // Верхняя и нижняя границы для регулировки MaxTSet-UB / MaxTSet-LB (t°C)
      localRequest = ot.buildRequest(OpenThermRequestType::READ, OpenThermMessageID::MaxTSetUBMaxTSetLB, 0);
      sendRequest(localRequest, localResponse);

      loop_counter = 20;
      }
      loop_counter--;
    }
  }

} OtHandler;