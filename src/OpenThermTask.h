#ifndef __OPENTHERMTASK__
#define __OPENTHERMTASK__

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
    void static ICACHE_RAM_ATTR handleInterrupt();
    void static HandleReply(unsigned long response);
    void static responseCallback(unsigned long result, OpenThermResponseStatus status);

protected:
  //===============================================================
  //              Вычисляем коэффициенты ПИД регулятора
  //===============================================================
    float pid(float sp, float pv, float pv_last, float &ierr, float dt);
    float curve(float sp, float pv);
    float curve2(float sp, float pv);
    bool getBoilerTemp();
    bool getDHWTemp();
    bool getOutsideTemp();
    bool setDHWTemp(float val);
    bool getFaultCode();
    bool sendRequest(unsigned long request, unsigned long& response);
    void setup();
    void printRequestDetail(OpenThermMessageID id, unsigned long request, unsigned long response);
    void testSupportedIDs();
    void loop();
};

#endif