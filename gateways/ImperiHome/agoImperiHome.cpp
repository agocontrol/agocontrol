/*
   Copyright (C) 2012 Harald Klein <hari@vt100.at>

   This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

   See the GNU General Public License for more details.

   */

#include <qpid/messaging/Message.h>

#include <json/reader.h>
#include <json/writer.h>

#include <boost/foreach.hpp>
#include <boost/tokenizer.hpp>

#include "agoapp.h"
#include "agoapp.h"
#include "agohttp/agohttp.h"

//default auth file
#define HTPASSWD ".htpasswd"

namespace fs = ::boost::filesystem;
using namespace agocontrol;
using namespace agocontrol::agohttp;

class AgoImperiHome: public AgoApp {
private:
    AgoHttp agoHttp;

    boost::shared_ptr<HttpReqRep> handleReqHome(struct mg_connection *conn, struct http_message *hm, const std::string &path);
    boost::shared_ptr<HttpReqRep> handleReqDevices(struct mg_connection *conn, struct http_message *hm, const std::string &path);
    boost::shared_ptr<HttpReqRep> handleReqSystem(struct mg_connection *conn, struct http_message *hm, const std::string &path);
    boost::shared_ptr<HttpReqRep> handleReqRooms(struct mg_connection *conn, struct http_message *hm, const std::string &path);

    void eventHandler(std::string subject, qpid::types::Variant::Map content) ;

    void setupApp();

    void doShutdown();
    void cleanupApp();
    std::string convertDeviceType(std::string devicetype);

public:
    AGOAPP_CONSTRUCTOR(AgoImperiHome)
};

boost::shared_ptr<HttpReqRep> AgoImperiHome::handleReqHome(struct mg_connection *conn, struct http_message *hm, const std::string &path) {
    boost::shared_ptr<HttpReqJsonRep> reqRep(new HttpReqJsonRep());
    reqRep->jsonResponse = Json::Value("ImperiHome Gateway");
    reqRep->responseReady = true;
    return reqRep;
}

boost::shared_ptr<HttpReqRep> AgoImperiHome::handleReqDevices(struct mg_connection *conn, struct http_message *hm, const std::string &path) {
    boost::shared_ptr<HttpReqJsonRep> reqRep(new HttpReqJsonRep());

    if(path == "/devices") {
        // build device list as specified in http://www.imperihome.com/apidoc/systemapi/#!/devices/listDevices_get_0
        qpid::types::Variant::List devicelist;
        qpid::types::Variant::Map inventory = agoConnection->getInventory();
        if( inventory.size()>0 && !inventory["devices"].isVoid() ) {
            qpid::types::Variant::Map devices = inventory["devices"].asMap();
            for (qpid::types::Variant::Map::iterator it = devices.begin(); it != devices.end(); it++) {
                if (it->second.isVoid()) {
                    AGO_ERROR() << "No device map for " << it->first;
                    continue;
                }
                qpid::types::Variant::Map device = it->second.asMap();
                if (device["name"].asString() == "") continue; // skip unnamed devices
                if (device["room"].asString() == "") continue; // skip devices without room assignment
                qpid::types::Variant::Map deviceinfo;
                deviceinfo["name"]=device["name"];
                deviceinfo["room"]=device["room"];
                qpid::types::Variant::Map values;
                if (!device["values"].isVoid()) values = device["values"].asMap();

                if (device["devicetype"] == "switch") {
                    deviceinfo["type"]="DevSwitch";
                    if (!values["state"].isVoid()) {
                        qpid::types::Variant::Map param;
                        qpid::types::Variant::List paramList;
                        param["key"]="Status";
                        param["value"]=values["state"].asInt64() == 0 ? "0" : "1";
                        paramList.push_back(param);
                        deviceinfo["params"]=paramList;
                    }
                } else if (device["devicetype"] == "dimmer") {
                    AGO_DEBUG() << "Values for dimmer device: " << values;
                    deviceinfo["type"]="DevDimmer";
                    if (!values["state"].isVoid()) {
                        qpid::types::Variant::Map param, param2;
                        qpid::types::Variant::List paramList;
                        param["key"]="Status";
                        param["value"]=values["state"].asInt64() == 0 ? "0" : "1";
                        paramList.push_back(param);
                        param2["key"]="Level";
                        param2["value"]=values["state"].asInt64() == 255 ? 100 : values["state"].asInt64();
                        paramList.push_back(param2);
                        deviceinfo["params"]=paramList;
                    }
                } else if (device["devicetype"] == "dimmerrgb" || device["devicetype"] == "dimmerrgbw" || device["devicetype"] == "smartdimmer") {
                    AGO_DEBUG() << "Values for dimmerrgb device: " << values;
                    deviceinfo["type"]="DevRGBLight";
                    qpid::types::Variant::List paramList;
                    if (!values["state"].isVoid()) {
                        qpid::types::Variant::Map param, param2;
                        param["key"]="Status";
                        param["value"]=values["state"].asInt64() == 0 ? "0" : "1";
                        paramList.push_back(param);
                        param2["key"]="Level";
                        param2["value"]=values["state"].asInt64() == 255 ? 100 : values["state"].asInt64();
                        paramList.push_back(param2);

                    }
                    qpid::types::Variant::Map param3, param4;
                    param3["key"]="dimmable";
                    param3["value"]="1";
                    paramList.push_back(param3);
                    param4["key"]="whitechannel";
                    if (device["devicetype"] == "dimmerrgb" || device["devicetype"] == "smartdimmer") { /* TODO: Check if this is right for smartdimmer */
                        param4["value"]="0";
                    } else {
                        param4["value"]="1";
                    }
                    paramList.push_back(param4);
                    deviceinfo["params"]=paramList;
                } else if (device["devicetype"] == "drapes") {
                    AGO_DEBUG() << "Values for drapes/shutter device: " << values;
                    deviceinfo["type"]="DevShutter";
                    if (!values["state"].isVoid()) {
                        qpid::types::Variant::Map param, param2, param3;
                        qpid::types::Variant::List paramList;
                        param["key"]="Level";
                        param["value"]=values["state"].asInt64() == 0 ? "100" : "0"; // TODO: should reflect real level
                        paramList.push_back(param);
                        param2["key"]="stopable";
                        param2["value"]=1; // TODO: add new device type to agocontrol so that we can distinguish
                        paramList.push_back(param2);
                        param2["key"]="pulseable";
                        param2["value"]=1; // TODO: add new device type to agocontrol so that we can distinguish
                        paramList.push_back(param3);
                        deviceinfo["params"]=paramList;
                    }

                } else if (device["devicetype"] == "scenario") {
                    deviceinfo["type"]="DevScene";
                } else if (device["devicetype"] == "camera") {
                    deviceinfo["type"]="DevCamera";
                    qpid::types::Variant::Map param;
                    param["key"]="localjpegurl";
                    param["value"]=device["internalid"];
                    qpid::types::Variant::List paramList;
                    paramList.push_back(param);
                    deviceinfo["params"]=paramList;
                } else if (device["devicetype"] == "co2sensor") {
                    deviceinfo["type"]="DevCO2";
                    qpid::types::Variant::List paramList;
                    if (!(values["co2"]).isVoid()) {
                        qpid::types::Variant::Map agoValue;
                        qpid::types::Variant::Map param;
                        agoValue = (values["co2"]).asMap();
                        param["key"]="Value";
                        param["value"]=agoValue["level"].asString();
                        param["unit"]=agoValue["unit"];
                        param["graphable"]="false";
                        paramList.push_back(param);
                        deviceinfo["params"]=paramList;
                    }
                } else if (device["devicetype"] == "multilevelsensor") {
                    deviceinfo["type"]="DevGenericSensor";
                    qpid::types::Variant::List paramList;
                    for (qpid::types::Variant::Map::iterator paramIt= values.begin(); paramIt != values.end(); paramIt++) {
                        qpid::types::Variant::Map agoValue;
                        qpid::types::Variant::Map param;
                        if (!(paramIt->second).isVoid()) agoValue = (paramIt->second).asMap();
                        //param["key"]=paramIt->first;
                        param["key"]="Value";
                        param["value"]=agoValue["level"].asString();
                        param["unit"]=agoValue["unit"];
                        param["graphable"]="false";
                        paramList.push_back(param);
                        deviceinfo["params"]=paramList;
                    }
                    deviceinfo["params"]=paramList;
                } else if (device["devicetype"] == "brightnesssensor") {
                    deviceinfo["type"]="DevLuminosity";
                    qpid::types::Variant::List paramList;
                    if (!(values["brightness"]).isVoid()) {
                        qpid::types::Variant::Map agoValue;
                        qpid::types::Variant::Map param;
                        agoValue = (values["brightness"]).asMap();
                        param["key"]="Value";
                        param["value"]=agoValue["level"].asString();
                        param["unit"]=agoValue["unit"];
                        param["graphable"]="false";
                        paramList.push_back(param);
                        deviceinfo["params"]=paramList;
                    }
                } else if (device["devicetype"] == "smokedetector") {
                    deviceinfo["type"]="DevSmoke";
                } else if (device["devicetype"] == "temperaturesensor") {
                    deviceinfo["type"]="DevTemperature";
                    qpid::types::Variant::List paramList;
                    if (!(values["temperature"]).isVoid()) {
                        qpid::types::Variant::Map agoValue;
                        qpid::types::Variant::Map param;
                        agoValue = (values["temperature"]).asMap();
                        param["key"]="Value";
                        param["value"]=agoValue["level"].asString();
                        if (agoValue["unit"] == "degC") {
                            param["unit"]="˚C";
                        } else {
                            param["unit"]="˚F";
                        }
                        param["graphable"]="false";
                        paramList.push_back(param);
                        deviceinfo["params"]=paramList;
                    }
                } else if (device["devicetype"] == "humiditysensor") {
                    deviceinfo["type"]="DevHygrometry";
                    qpid::types::Variant::List paramList;
                    if (!(values["humidity"]).isVoid()) {
                        qpid::types::Variant::Map agoValue;
                        qpid::types::Variant::Map param;
                        agoValue = (values["humidity"]).asMap();
                        param["key"]="Value";
                        param["value"]=agoValue["level"].asString();
                        param["unit"]=agoValue["unit"];
                        param["graphable"]="false";
                        paramList.push_back(param);
                        deviceinfo["params"]=paramList;
                    }
                } else if (device["devicetype"] == "thermostat") {
                    deviceinfo["type"]="DevThermostat";
                } else continue;
                deviceinfo["id"]=it->first;
                devicelist.push_back(deviceinfo);
            }
        }

        qpid::types::Variant::Map finaldevices;
        finaldevices["devices"] = devicelist;

        reqRep->jsonResponse = Json::Value(Json::objectValue);
        variantMapToJson(finaldevices, reqRep->jsonResponse);
        reqRep->responseReady = true;
        return reqRep;
    }

    // Else, assume /devices/xxx/action/xxx
    // parse the action URI as defined by http://www.imperihome.com/apidoc/systemapi/#!/devices/deviceAction_get_1
    std::vector<std::string> items = split(path, '/');
    for (unsigned int i=0;i<items.size();i++) {
        AGO_TRACE() << "Item " << i << ": " << items[i];
    }
    if ( items.size()>=5 && items.size()<=6 ) {
        if (items[1] == "devices" && items[3]=="action") {
            qpid::types::Variant::Map command;
            command["uuid"]=items[2];
            if (items.size()==6) { // we got an action with paramter
                if (items[4] == "setStatus") {
                    command["command"]= items[5]=="1" ? "on" : "off";
                } else  if (items[4] == "setLevel") {
                    command["command"]="setlevel";
                    command["level"]= atoi(items[5].c_str());
                } else  if (items[4] == "setArmed") {
                    // TODO
                } else  if (items[4] == "setChoice") {
                    // TODO
                } else  if (items[4] == "setMode") {
                    command["command"]="setthermostatmode";
                    command["mode"]=items[5];
                } else  if (items[4] == "setSetPoint") {
                    command["command"]="settemperature";
                    command["temperature"]=atof(items[5].c_str());
                } else  if (items[4] == "pulseShutter") {
                    command["command"]= items[5]=="up" ? "off" : "on";
                } else  if (items[4] == "setColor") {
                    std::stringstream colorstring(items[5]);
                    unsigned int num = 0;
                    colorstring >> std::hex >> num;
                    command["command"] = "setcolor";
                    command["white"] = (num / 0x1000000) % 0x100;
                    command["red"] = (num / 0x10000) % 0x100;
                    command["green"] = (num / 0x100) % 0x100;
                    command["blue"] = num % 0x100;
                }
            } else { // we got action without parameter
                if (items[4] == "stopShutter") {
                    command["command"]="stop";
                } else if (items[4] == "launchScene") {
                    command["command"]="run";
                } else  if (items[4] == "setAck") {
                    // TODO
                }
            }

            reqRep->jsonResponse = Json::Value(Json::objectValue);
            if(!command.count("command")) {
                reqRep->jsonResponse["success"] = false;
                reqRep->jsonResponse["errormsg"] = "Unsupported command in agoImperiHome";
            }else{
                // XXX: Do not interact with remote in this thread!
                AgoResponse rep = agoConnection->sendRequest(command);

                reqRep->jsonResponse["success"] = !rep.isError();
                reqRep->jsonResponse["errormsg"] = rep.getMessage();
            }

            reqRep->responseReady = true;
            return reqRep;
        }
    }

    // TODO: 404
    reqRep->setResponseCode(404);
    reqRep->responseReady = true;

    return reqRep;
}

boost::shared_ptr<HttpReqRep> AgoImperiHome::handleReqSystem(struct mg_connection *conn, struct http_message *hm, const std::string &path) {
    boost::shared_ptr<HttpReqJsonRep> reqRep(new HttpReqJsonRep());

    qpid::types::Variant::Map systeminfo;
    systeminfo["apiversion"] = 1;
    systeminfo["id"] = getConfigSectionOption("system", "uuid", "00000000-0000-0000-000000000000");

    reqRep->jsonResponse = Json::Value(Json::objectValue);
    variantMapToJson(systeminfo, reqRep->jsonResponse);
    reqRep->responseReady = true;

    return reqRep;
}

boost::shared_ptr<HttpReqRep> AgoImperiHome::handleReqRooms(struct mg_connection *conn, struct http_message *hm, const std::string &path) {
    boost::shared_ptr<HttpReqJsonRep> reqRep(new HttpReqJsonRep());

    qpid::types::Variant::List roomlist;

    // XXX: bad, do not block and wait in this thread!
    qpid::types::Variant::Map inventory = agoConnection->getInventory();
    if( inventory.size()>0 && !inventory["rooms"].isVoid() ) {
        qpid::types::Variant::Map rooms = inventory["rooms"].asMap();
        for (qpid::types::Variant::Map::iterator it = rooms.begin(); it != rooms.end(); it++) {
            qpid::types::Variant::Map room = it->second.asMap();
            AGO_TRACE() << room;
            qpid::types::Variant::Map roominfo;
            roominfo["name"]=room["name"];
            roominfo["id"]=it->first;
            roomlist.push_back(roominfo);
        }
    }

    qpid::types::Variant::Map finalrooms;
    finalrooms["rooms"] = roomlist;

    reqRep->jsonResponse = Json::Value(Json::objectValue);
    variantMapToJson(finalrooms, reqRep->jsonResponse);
    reqRep->responseReady = true;

    return reqRep;
}

/**
 * Agoclient event handler
 */
void AgoImperiHome::eventHandler(std::string subject, qpid::types::Variant::Map content)
{
}

void AgoImperiHome::setupApp() {
    std::string ports_cfg;
    fs::path htdocs;
    fs::path certificate;
    std::string domainname;

    //get parameters
    ports_cfg = getConfigOption("ports", "8088");
    htdocs = getConfigOption("htdocs", fs::path(BOOST_PP_STRINGIZE(DEFAULT_HTMLDIR)));
    certificate = getConfigOption("certificate", getConfigPath("/rpc/rpc_cert.pem"));
    domainname = getConfigOption("domainname", "agocontrol");

    //agoHttp.setDocumentRoot(htdocs.string());
    agoHttp.setAuthDomain(domainname);

    // Parse bindings/ports
    typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
    boost::char_separator<char> sep(", ");
    tokenizer tok(ports_cfg, sep);
    for(tokenizer::iterator gen=tok.begin(); gen != tok.end(); ++gen) {
        std::string addr(*gen);
        if(addr[addr.length() -1] == 's') {
            addr.assign(addr, 0, addr.length()-1);
            agoHttp.addBinding(addr, certificate);
        }else
            agoHttp.addBinding(addr);
    }

    fs::path authPath = htdocs / HTPASSWD;
    if( fs::exists(authPath) )
        agoHttp.setAuthFile(authPath);
    else
        AGO_INFO() << "Disabling authentication: file does not exist";

    agoHttp.addHandler("/", boost::bind(&AgoImperiHome::handleReqHome, this, _1, _2, _3));
    agoHttp.addPrefixHandler("/devices", boost::bind(&AgoImperiHome::handleReqDevices, this, _1, _2, _3));
    agoHttp.addHandler("/system", boost::bind(&AgoImperiHome::handleReqSystem, this, _1, _2, _3));
    agoHttp.addHandler("/rooms", boost::bind(&AgoImperiHome::handleReqRooms, this, _1, _2, _3));

    try {
        agoHttp.start();
    }catch(const std::runtime_error &err) {
        throw ConfigurationError(err.what());
    }

    addEventHandler();
}

void AgoImperiHome::doShutdown() {
    agoHttp.shutdown();
    AgoApp::doShutdown();
}

void AgoImperiHome::cleanupApp() {
    // Wait for Http to close and cleanup
    agoHttp.close();
}

AGOAPP_ENTRY_POINT(AgoImperiHome);

