#ifndef AGOSYSTEM_H
#define AGOSYSTEM_H
#include "agoapp.h"

using namespace agocontrol;

class AgoSystem: public AgoApp {
private:
    Json::Value commandHandler(const Json::Value& content) ;

    Json::Value getAgoProcessList();
    Json::Value getAgoProcessListDebug(std::string procName);

    // Implemented in processinfo_<platform>.cpp
    void getProcessInfo();

    void fillProcessesStats(Json::Value& processes);
    void checkProcessesStates(Json::Value& processes);
    void printProcess(const Json::Value& process);
    void monitorProcesses();
    Json::Value getProcessStructure();
    bool isProcessBlackListed(const std::string processName);
    void refreshAgoProcessList(Json::Value& processes);

    void setupApp();

    boost::mutex processesMutex;
    Json::Value processes;
    Json::Value config;
public:
    AGOAPP_CONSTRUCTOR(AgoSystem);
};

#endif
