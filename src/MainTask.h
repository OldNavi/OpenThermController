#ifndef __MAINTASK___
#define ___MAINTASK___

class MainTaskClass : public Task
{

protected:
    bool static handleIncomingJson(String payload);
    void static callback(char *topic, byte *payload, unsigned int length);
    void reconnect();
    void checkAndSaveConfig();
    String static handleOutJson();
    void static handleUpdateToMQTT(bool now);
    static String heaterMode();
    static void handleRoot();
    static void handleGetStatus();
    static void handlePostCommand();
    void setup();
    void loop();
};

#endif