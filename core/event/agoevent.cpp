#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <pthread.h>

#include <string>
#include <iostream>
#include <sstream>
#include <cerrno>

#include "agoapp.h"
#include "bool.h"

#ifndef EVENTMAPFILE
#define EVENTMAPFILE "maps/eventmap.json"
#endif

using namespace agocontrol;
namespace fs = ::boost::filesystem;

class AgoEvent: public AgoApp {
private:
    Json::Value eventmap;
    Json::Value commandHandler(const Json::Value& content) ;
    void eventHandler(const std::string& subject , const Json::Value& content) ;
    void setupApp();
public:
    AGOAPP_CONSTRUCTOR(AgoEvent);
};

std::string typeName(const Json::Value& v) {
    switch(v.type()) {
        case Json::nullValue:     return "null";
        case Json::intValue:      return "int";
        case Json::uintValue:     return "uint";
        case Json::realValue:     return "real";
        case Json::stringValue:   return "string";
        case Json::booleanValue:  return "bool";
        case Json::arrayValue:    return "array";
        case Json::objectValue:   return "object";
    }
    return "INVALID";
}

// example event:eb68c4a5-364c-4fb8-9b13-7ea3a784081f:{action:{command:on, uuid:25090479-566d-4cef-877a-3e1927ed4af0}, criteria:{0:{comp:eq, lval:hour, rval:7}, 1:{comp:eq, lval:minute, rval:1}}, event:event.environment.timechanged, nesting:(criteria["0"] and criteria["1"])}


void AgoEvent::eventHandler(const std::string& subject , const Json::Value& content)
{
    // ignore device announce events
    if( subject=="event.device.announce" || subject=="event.device.discover" )
    {
        return;
    }

    // iterate event map and match for event name
    for (auto it = eventmap.begin(); it!=eventmap.end(); it++)
    {
        Json::Value& event (*it);
        if(event.isNull())
        {
            AGO_ERROR() << "Eventmap entry is void";
            continue;
        }

        if (event["event"] != subject)
            continue;

        AGO_TRACE() << "Found matching event: " << event;
        // check if the event is disabled
        if(event.isMember("disabled") && event["disabled"].isConvertibleTo(Json::booleanValue) && event["disabled"].asBool())
        {
            return;
        }

        Json::Value inventory; // this will hold the inventory from the resolver if we need it during evaluation
        Json::Value criteria(Json::objectValue); // this holds the criteria evaluation results for each criteria
        std::string nesting = event["nesting"].asString();
        if (event.isMember("criteria")) for (auto crit = event["criteria"].begin(); crit!= event["criteria"].end(); crit++)
        {
            Json::Value& element (*crit);
            AGO_TRACE() << "criteria[" << crit.name() << "] - " << element;
            if(element.isNull())
            {
                AGO_ERROR() << "Criteria element is void";
                continue;
            }

            try
            {
                // AGO_TRACE() << "LVAL: " << element["lval"];
                Json::Value lvalmap;
                Json::Value lval;
                if (element.isMember("lval"))
                {
                    if (element["lval"].type() == Json::stringValue)
                    {
                        // legacy eventmap entry
                        lvalmap["type"] = "event";
                        lvalmap["parameter"] = element["lval"];
                    }
                    else
                    {
                        lvalmap = element["lval"];
                    }
                }
                // determine lval depending on type
                if (lvalmap["type"] == "variable")
                {
                    Json::Value variables;
                    std::string name = lvalmap["name"].asString();
                    if (inventory.isMember("system"))
                        inventory = agoConnection->getInventory(); // fetch inventory as it is needed for eval but has not yet been pulled
                    if (inventory.isMember("variables"))
                    {
                        variables = inventory["variables"];
                    }
                    lval = variables[name];

                }
                else if (lvalmap["type"] == "device")
                {
                    std::string uuid = lvalmap["uuid"].asString();
                    if (inventory.isMember("system"))
                        inventory = agoConnection->getInventory(); // fetch inventory as it is needed for eval but has not yet been pulled
                    Json::Value& devices(inventory["devices"]);
                    Json::Value& device(devices[uuid]);
                    if (lvalmap["parameter"] == "state")
                    {
                        lval = device["state"];
                    }
                    else
                    {
                        Json::Value values = device["values"];
                        std::string parameter = lvalmap["parameter"].asString();
                        Json::Value value = values[parameter];
                        lval = value["level"];
                    }
                }
                else //event
                {
                    lval = content[lvalmap["parameter"].asString()];
                }

                Json::Value rval = element["rval"];
                AGO_TRACE() << "lval: " << lval << " (" << typeName(lval.type()) << ")";
                AGO_TRACE() << "rval: " << rval << " (" << typeName(rval.type()) << ")";

                if (element["comp"] == "eq")
                {
                    if (lval.type() == Json::stringValue || rval.type() == Json::stringValue) // compare as string
                        criteria[crit.name()] = lval.asString() == rval.asString();
                    else
                        criteria[crit.name()] = lval == rval;
                } else if (element["comp"] == "neq")
                {
                    if (lval.type() == Json::stringValue || rval.type() == Json::stringValue) // compare as string
                        criteria[crit.name()] = lval.asString() != rval.asString();
                    else
                        criteria[crit.name()] = lval != rval;
                } else if (element["comp"] == "lt")
                {
                    criteria[crit.name()] = lval < rval;
                }
                else if (element["comp"] == "gt")
                {
                    criteria[crit.name()] = lval > rval;
                }
                else if (element["comp"] == "gte")
                {
                    criteria[crit.name()] = lval >= rval;
                }
                else if (element["comp"] == "lte")
                {
                    criteria[crit.name()] = lval <= rval;
                }
                else
                {
                    criteria[crit.name()] = false;
                }
                AGO_TRACE() << lval << " " << element["comp"] << " " << rval << " : " << criteria[crit.name()];
            }
            catch ( const std::exception& error)
            {
                std::stringstream errorstring;
                errorstring << error.what();
                AGO_ERROR() << "Exception occured: " << errorstring.str();
                criteria[crit.name()] = false;
            }

            // this is for converted legacy scenario maps
            std::stringstream token; token << "criteria[\"" << crit.name() << "\"]";
            std::stringstream boolval; boolval << criteria[crit.name()];
            replaceString(nesting, token.str(), boolval.str());

            // new javascript editor sends criteria[x] not criteria["x"]
            std::stringstream token2; token2 << "criteria[" << crit.name() << "]";
            replaceString(nesting, token2.str(), boolval.str());
        }
        replaceString(nesting, "and", "&");
        replaceString(nesting, "or", "|");
        nesting += ";";
        AGO_TRACE() << "nesting prepared: " << nesting;
        if (evaluateNesting(nesting))
        {
            AGO_DEBUG() << "sending event action as command";
            agoConnection->sendMessage(event["action"]);
        }
    }

}

Json::Value AgoEvent::commandHandler(const Json::Value& content)
{
    Json::Value responseData;
    std::string internalid = content["internalid"].asString();
    if (internalid == "eventcontroller")
    {
        if (content["command"] == "setevent")
        {
            checkMsgParameter(content, "eventmap", Json::objectValue);

            AGO_DEBUG() << "setevent request";
            Json::Value newevent = content["eventmap"];
            std::string eventuuid = content["event"].asString();
            if (eventuuid.empty())
                eventuuid = generateUuid();

            AGO_TRACE() << "event content:" << newevent;
            AGO_TRACE() << "event uuid:" << eventuuid;
            eventmap[eventuuid] = newevent;

            agoConnection->addDevice(eventuuid, "event", true);
            if (writeJsonFile(eventmap, getConfigPath(EVENTMAPFILE)))
            {
                responseData["event"] = eventuuid;
                return responseSuccess(responseData);
            }
            else
            {
                return responseFailed("Failed to write map file");
            }
        }
        else if (content["command"] == "getevent")
        {
            checkMsgParameter(content, "event", Json::stringValue);

            std::string event = content["event"].asString();

            AGO_DEBUG() << "getevent request:" << event;
            responseData["eventmap"] = eventmap[event];
            responseData["event"] = event;

            return responseSuccess(responseData);
        }
        else if (content["command"] == "delevent")
        {
            checkMsgParameter(content, "event", Json::stringValue);

            std::string event = content["event"].asString();
            AGO_DEBUG() << "delevent request:" << event;

            if (eventmap.isMember(event))
            {
                AGO_TRACE() << "removing ago device" << event;
                agoConnection->removeDevice(event);
                eventmap.removeMember(event);
                if (!writeJsonFile(eventmap, getConfigPath(EVENTMAPFILE)))
                {
                    return responseFailed("Failed to write map file");
                }
            }

            // If it was not found, it was already deleted; delete succeded
            return responseSuccess();
        }else{
            return responseUnknownCommand();
        }
    }

    // We do not support sending commands to our 'devices'
    return responseNoDeviceCommands();
}

void AgoEvent::setupApp()
{
    AGO_DEBUG() << "parsing eventmap file";
    fs::path file = getConfigPath(EVENTMAPFILE);
    readJsonFile(eventmap, file);

    addCommandHandler();
    addEventHandler();

    agoConnection->addDevice("eventcontroller", "eventcontroller");

    for (Json::Value::const_iterator it = eventmap.begin(); it!=eventmap.end(); it++)
    {
        AGO_DEBUG() << "adding event:" << it.name() << ":" << *it;
        agoConnection->addDevice(it.name(), "event", true);
    }
}

AGOAPP_ENTRY_POINT(AgoEvent);
