#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <string>
#include <iostream>
#include <sstream>
#include <cerrno>

#include "agoapp.h"
#include "agoutils.h"

#ifndef SCENARIOMAPFILE
#define SCENARIOMAPFILE "maps/scenariomap.json"
#endif

using namespace agocontrol;

class AgoScenario: public AgoApp
{
private:
    Json::Value scenariomap;
    Json::Value commandHandler(const Json::Value& content) ;
    void setupApp();
    void runscenario(Json::Value scenario) ;

public:
    AGOAPP_CONSTRUCTOR(AgoScenario);
};

void AgoScenario::runscenario(Json::Value scenario)
{
    AGO_DEBUG() << "Executing scenario";

    // build sorted list of scenario elements
    std::list<int> elements;
    for (auto it = scenario.begin(); it!= scenario.end(); it++)
    {
        // AGO_TRACE() << it.name();
        // AGO_TRACE() << *it;
        elements.push_back(atoi(it.name().c_str()));
    }
    // AGO_TRACE() << "elements: " << elements;
    elements.sort();
    for (auto it = elements.begin(); it != elements.end(); it++)
    {
        // AGO_TRACE() << *it;
        int seq = *it;
        std::stringstream sseq;
        sseq << seq;
        const Json::Value& element(scenario[sseq.str()]);
        AGO_TRACE() << sseq.str() << ": " << element;
        if (element["command"] == "scenariosleep")
        {
            uint delay;
            if(stringToUInt(element["delay"], delay))
            {
                AGO_DEBUG() << "scenariosleep special command detected. Delay: " << delay;
                sleep(delay);
            }
            else
            {
                AGO_ERROR() << "Invalid 'delay' argument value in scenario " << element;
            }
        }
        else
        {
            AGO_DEBUG() << "sending scenario command: " << element;
            agoConnection->sendMessage(element);
        }
    }
}

Json::Value AgoScenario::commandHandler(const Json::Value& content)
{
    Json::Value returnData;
    std::string internalid = content["internalid"].asString();

    if (internalid == "scenariocontroller")
    {
        if (content["command"] == "setscenario")
        {
            checkMsgParameter(content, "scenariomap", Json::objectValue);
            AGO_TRACE() << "setscenario request";
            const Json::Value& newscenario(content["scenariomap"]);

            std::string scenariouuid;
            if(content.isMember("scenario"))
                scenariouuid = content["scenario"].asString();
            else
                scenariouuid = agocontrol::utils::generateUuid();

            AGO_TRACE() << "Scenario content:" << newscenario;
            AGO_TRACE() << "scenario uuid:" << scenariouuid;
            scenariomap[scenariouuid] = newscenario;

            agoConnection->addDevice(scenariouuid, "scenario", true);
            if (writeJsonFile(scenariomap, getConfigPath(SCENARIOMAPFILE)))
            {
                returnData["scenario"] = scenariouuid;
                return responseSuccess(returnData);
            }

            return responseFailed("Failed to write map file");
        }
        else if (content["command"] == "getscenario")
        {
            checkMsgParameter(content, "scenario", Json::stringValue);
            std::string scenario = content["scenario"].asString();

            AGO_TRACE() << "getscenario request:" << scenario;
            if(!scenariomap.isMember(scenario))
                return responseError(RESPONSE_ERR_NOT_FOUND, "Scenario not found");

            returnData["scenariomap"] = scenariomap[scenario];
            returnData["scenario"] = scenario;

            return responseSuccess(returnData);
        }
        else if (content["command"] == "delscenario")
        {
            checkMsgParameter(content, "scenario", Json::stringValue);

            std::string scenario = content["scenario"].asString();
            AGO_TRACE() << "delscenario request:" << scenario;
            if(scenariomap.isMember(scenario)) {
                AGO_DEBUG() << "removing ago device" << scenario;
                agoConnection->removeDevice(scenario);
                scenariomap.removeMember(scenario);
                if (!writeJsonFile(scenariomap, getConfigPath(SCENARIOMAPFILE)))
                {
                    return responseFailed("Failed to write file");
                }
            }
            return responseSuccess();
        }
        
        return responseUnknownCommand();
    }
    else
    {
        checkMsgParameter(content, "command", Json::stringValue);

        if ((content["command"] == "on") || (content["command"] == "run"))
        {
            AGO_DEBUG() << "spawning thread for scenario: " << internalid;
            boost::thread t(boost::bind(&AgoScenario::runscenario, this, scenariomap[internalid]));
            t.detach();
            return responseSuccess();
        }

        return responseError(RESPONSE_ERR_PARAMETER_INVALID, "Only commands 'on' and 'run' are supported");
    }
}

void AgoScenario::setupApp()
{
    addCommandHandler();
    agoConnection->addDevice("scenariocontroller", "scenariocontroller");

    readJsonFile(scenariomap, getConfigPath(SCENARIOMAPFILE));
    for (Json::Value::const_iterator it = scenariomap.begin(); it!=scenariomap.end(); it++)
    {
        AGO_DEBUG() << "Loading scenario: " << it.name() << ":" << *it;
        agoConnection->addDevice(it.name(), "scenario", true);
    }
}

AGOAPP_ENTRY_POINT(AgoScenario);
