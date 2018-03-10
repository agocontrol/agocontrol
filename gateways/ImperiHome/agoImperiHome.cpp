/*
   Copyright (C) 2012 Harald Klein <hari@vt100.at>

   This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

   See the GNU General Public License for more details.

   */

#include <json/json.h>

#include <boost/foreach.hpp>
#include <boost/tokenizer.hpp>

#include "agoapp.h"
#include "agohttp/agohttp.h"

//default auth file
#define HTPASSWD ".htpasswd"

namespace fs = ::boost::filesystem;
using namespace agocontrol;
using namespace agocontrol::agohttp;

class AgoImperiHome : public AgoApp {
private:
    AgoHttp agoHttp;

    boost::shared_ptr<HttpReqRep>
    handleReqHome(struct mg_connection *conn, struct http_message *hm, const std::string &path);

    boost::shared_ptr<HttpReqRep>
    handleReqDevices(struct mg_connection *conn, struct http_message *hm, const std::string &path);

    boost::shared_ptr<HttpReqRep>
    handleReqSystem(struct mg_connection *conn, struct http_message *hm, const std::string &path);

    boost::shared_ptr<HttpReqRep>
    handleReqRooms(struct mg_connection *conn, struct http_message *hm, const std::string &path);

    void eventHandler(const std::string &subject, const Json::Value &content);

    void setupApp();

    void doShutdown();

    void cleanupApp();

    std::string convertDeviceType(std::string devicetype);

public:
    AGOAPP_CONSTRUCTOR(AgoImperiHome)
};

boost::shared_ptr<HttpReqRep>
AgoImperiHome::handleReqHome(struct mg_connection *conn, struct http_message *hm, const std::string &path) {
    boost::shared_ptr<HttpReqJsonRep> reqRep(new HttpReqJsonRep());
    reqRep->jsonResponse = Json::Value("ImperiHome Gateway");
    reqRep->responseReady = true;
    return reqRep;
}

boost::shared_ptr<HttpReqRep>
AgoImperiHome::handleReqDevices(struct mg_connection *conn, struct http_message *hm, const std::string &path) {
    boost::shared_ptr<HttpReqJsonRep> reqRep(new HttpReqJsonRep());

    if (path == "/devices") {
        // build device list as specified in http://www.imperihome.com/apidoc/systemapi/#!/devices/listDevices_get_0
        Json::Value devicelist(Json::arrayValue);
        const Json::Value inventory = agoConnection->getInventory();
        if (inventory.isMember("devices")) {
            Json::Value devices = inventory["devices"];
            for (auto it = devices.begin(); it != devices.end(); it++) {
                if (it->isNull()) {
                    AGO_ERROR() << "No device map for " << it.name();
                    continue;
                }
                const Json::Value& device = *it;
                if (!device.isMember("name") || device["name"].asString() == "") continue; // skip unnamed devices
                if (!device.isMember("room") || device["room"].asString() == "")
                    continue; // skip devices without room assignment

                Json::Value deviceinfo;
                deviceinfo["name"] = device["name"];
                deviceinfo["room"] = device["room"];

                Json::Value values;
                if (device.isMember("values"))
                    values = device["values"];

                if (device["devicetype"] == "switch") {
                    deviceinfo["type"] = "DevSwitch";
                    if (values.isMember("state")) {
                        Json::Value param;
                        Json::Value paramList;
                        param["key"] = "Status";
                        param["value"] = values["state"].asInt64() == 0 ? "0" : "1";
                        paramList.append(param);
                        deviceinfo["params"] = paramList;
                    }
                } else if (device["devicetype"] == "dimmer") {
                    AGO_DEBUG() << "Values for dimmer device: " << values;
                    deviceinfo["type"] = "DevDimmer";
                    if (values.isMember("state")) {
                        Json::Value param;
                        Json::Value paramList;
                        param["key"] = "Status";
                        param["value"] = values["state"].asInt64() == 0 ? "0" : "1";
                        paramList.append(param);

                        param["key"] = "Level";
                        param["value"] = values["state"].asInt64() == 255 ? 100 : values["state"].asInt64();
                        paramList.append(param);
                        deviceinfo["params"] = paramList;
                    }
                } else if (device["devicetype"] == "dimmerrgb" || device["devicetype"] == "dimmerrgbw" ||
                           device["devicetype"] == "smartdimmer") {
                    AGO_DEBUG() << "Values for dimmerrgb device: " << values;
                    deviceinfo["type"] = "DevRGBLight";
                    Json::Value paramList;
                    Json::Value param;
                    if (values.isMember("state")) {
                        param["key"] = "Status";
                        param["value"] = values["state"].asInt64() == 0 ? "0" : "1";
                        paramList.append(param);

                        param["key"] = "Level";
                        param["value"] = values["state"].asInt64() == 255 ? 100 : values["state"].asInt64();
                        paramList.append(param);
                    }

                    param["key"] = "dimmable";
                    param["value"] = "1";
                    paramList.append(param);

                    param["key"] = "whitechannel";
                    if (device["devicetype"] == "dimmerrgb" ||
                        device["devicetype"] == "smartdimmer") { /* TODO: Check if this is right for smartdimmer */
                        param["value"] = "0";
                    } else {
                        param["value"] = "1";
                    }
                    paramList.append(param);

                    deviceinfo["params"] = paramList;
                } else if (device["devicetype"] == "drapes") {
                    AGO_DEBUG() << "Values for drapes/shutter device: " << values;
                    deviceinfo["type"] = "DevShutter";
                    if (values.isMember("state")) {
                        Json::Value param;
                        Json::Value paramList;
                        param["key"] = "Level";
                        param["value"] =
                                values["state"].asInt64() == 0 ? "100" : "0"; // TODO: should reflect real level
                        paramList.append(param);

                        param["key"] = "stopable";
                        param["value"] = 1; // TODO: add new device type to agocontrol so that we can distinguish
                        paramList.append(param);

                        param["key"] = "pulseable";
                        param["value"] = 1; // TODO: add new device type to agocontrol so that we can distinguish
                        paramList.append(param);

                        deviceinfo["params"] = paramList;
                    }

                } else if (device["devicetype"] == "scenario") {
                    deviceinfo["type"] = "DevScene";
                } else if (device["devicetype"] == "camera") {
                    deviceinfo["type"] = "DevCamera";
                    Json::Value param;
                    param["key"] = "localjpegurl";
                    param["value"] = device["internalid"];

                    Json::Value paramList;
                    paramList.append(param);
                    deviceinfo["params"] = paramList;
                } else if (device["devicetype"] == "co2sensor") {
                    deviceinfo["type"] = "DevCO2";
                    Json::Value paramList;
                    if (values.isMember("co2")) {
                        const Json::Value& agoValue(values["co2"]);
                        Json::Value param;
                        param["key"] = "Value";
                        param["value"] = agoValue["level"].asString();
                        param["unit"] = agoValue["unit"];
                        param["graphable"] = "false";
                        paramList.append(param);
                        deviceinfo["params"] = paramList;
                    }
                } else if (device["devicetype"] == "multilevelsensor") {
                    deviceinfo["type"] = "DevGenericSensor";
                    Json::Value paramList;
                    for (auto paramIt = values.begin(); paramIt != values.end(); paramIt++) {
                        Json::Value agoValue(*paramIt);
                        Json::Value param;

                        //param["key"]=it.name();
                        param["key"] = "Value";
                        param["value"] = agoValue["level"].asString();
                        param["unit"] = agoValue["unit"];
                        param["graphable"] = "false";
                        paramList.append(param);
                    }
                    deviceinfo["params"] = paramList;
                } else if (device["devicetype"] == "brightnesssensor") {
                    deviceinfo["type"] = "DevLuminosity";
                    if (values.isMember("brightness")) {
                        Json::Value& agoValue(values["brightness"]);
                        Json::Value paramList;
                        Json::Value param;
                        param["key"] = "Value";
                        param["value"] = agoValue["level"].asString();
                        param["unit"] = agoValue["unit"];
                        param["graphable"] = "false";
                        paramList.append(param);
                        deviceinfo["params"] = paramList;
                    }
                } else if (device["devicetype"] == "smokedetector") {
                    deviceinfo["type"] = "DevSmoke";
                } else if (device["devicetype"] == "temperaturesensor") {
                    deviceinfo["type"] = "DevTemperature";
                    if (values.isMember("temperature")) {
                        Json::Value paramList;
                        Json::Value& agoValue(values["temperature"]);
                        Json::Value param;
                        param["key"] = "Value";
                        param["value"] = agoValue["level"].asString();
                        if (agoValue["unit"] == "degC") {
                            param["unit"] = "˚C";
                        } else {
                            param["unit"] = "˚F";
                        }
                        param["graphable"] = "false";
                        paramList.append(param);
                        deviceinfo["params"] = paramList;
                    }
                } else if (device["devicetype"] == "humiditysensor") {
                    deviceinfo["type"] = "DevHygrometry";
                    if (values.isMember("humidity")) {
                        Json::Value agoValue(values["humidity"]);
                        Json::Value paramList;
                        Json::Value param;
                        param["key"] = "Value";
                        param["value"] = agoValue["level"].asString();
                        param["unit"] = agoValue["unit"];
                        param["graphable"] = "false";
                        paramList.append(param);
                        deviceinfo["params"] = paramList;
                    }
                } else if (device["devicetype"] == "thermostat") {
                    deviceinfo["type"] = "DevThermostat";
                } else
                    continue;
                deviceinfo["id"] = it.name();
                devicelist.append(deviceinfo);
            }
        }

        reqRep->jsonResponse["devices"] = devicelist;
        reqRep->responseReady = true;
        return reqRep;
    }

    // Else, assume /devices/xxx/action/xxx
    // parse the action URI as defined by http://www.imperihome.com/apidoc/systemapi/#!/devices/deviceAction_get_1
    std::vector<std::string> items = split(path, '/');
    for (unsigned int i = 0; i < items.size(); i++) {
        AGO_TRACE() << "Item " << i << ": " << items[i];
    }
    if (items.size() >= 5 && items.size() <= 6) {
        if (items[1] == "devices" && items[3] == "action") {
            Json::Value command;
            command["uuid"] = items[2];
            if (items.size() == 6) { // we got an action with paramter
                if (items[4] == "setStatus") {
                    command["command"] = items[5] == "1" ? "on" : "off";
                } else if (items[4] == "setLevel") {
                    command["command"] = "setlevel";
                    command["level"] = atoi(items[5].c_str());
                } else if (items[4] == "setArmed") {
                    // TODO
                } else if (items[4] == "setChoice") {
                    // TODO
                } else if (items[4] == "setMode") {
                    command["command"] = "setthermostatmode";
                    command["mode"] = items[5];
                } else if (items[4] == "setSetPoint") {
                    command["command"] = "settemperature";
                    command["temperature"] = atof(items[5].c_str());
                } else if (items[4] == "pulseShutter") {
                    command["command"] = items[5] == "up" ? "off" : "on";
                } else if (items[4] == "setColor") {
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
                    command["command"] = "stop";
                } else if (items[4] == "launchScene") {
                    command["command"] = "run";
                } else if (items[4] == "setAck") {
                    // TODO
                }
            }

            reqRep->jsonResponse = Json::Value(Json::objectValue);
            if (!command.isMember("command")) {
                reqRep->jsonResponse["success"] = false;
                reqRep->jsonResponse["errormsg"] = "Unsupported command in agoImperiHome";
            } else {
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

boost::shared_ptr<HttpReqRep>
AgoImperiHome::handleReqSystem(struct mg_connection *conn, struct http_message *hm, const std::string &path) {
    boost::shared_ptr<HttpReqJsonRep> reqRep(new HttpReqJsonRep());

    Json::Value& rep (reqRep->jsonResponse);

    rep["apiversion"] = 1;
    rep["id"] = getConfigSectionOption("system", "uuid", "00000000-0000-0000-000000000000");

    reqRep->responseReady = true;
    return reqRep;
}

boost::shared_ptr<HttpReqRep>
AgoImperiHome::handleReqRooms(struct mg_connection *conn, struct http_message *hm, const std::string &path) {
    boost::shared_ptr<HttpReqJsonRep> reqRep(new HttpReqJsonRep());

    Json::Value roomlist(Json::arrayValue);

    // XXX: bad, do not block and wait in this thread!
    const Json::Value inventory = agoConnection->getInventory();
    if (inventory.isMember("rooms")) {
        const Json::Value& rooms = inventory["rooms"];
        for (auto it = rooms.begin(); it != rooms.end(); it++) {
            const Json::Value& room = *it;
            AGO_TRACE() << room;
            Json::Value roominfo;
            roominfo["name"] = room["name"];
            roominfo["id"] = it.name();
            roomlist.append(roominfo);
        }
    }

    reqRep->jsonResponse["rooms"] = roomlist;
    reqRep->responseReady = true;

    return reqRep;
}

/**
 * Agoclient event handler
 */
void AgoImperiHome::eventHandler(const std::string &subject, const Json::Value &content) {
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
    for (tokenizer::iterator gen = tok.begin(); gen != tok.end(); ++gen) {
        std::string addr(*gen);
        if (addr[addr.length() - 1] == 's') {
            addr.assign(addr, 0, addr.length() - 1);
            agoHttp.addBinding(addr, certificate);
        } else
            agoHttp.addBinding(addr);
    }

    fs::path authPath = htdocs / HTPASSWD;
    if (fs::exists(authPath))
        agoHttp.setAuthFile(authPath);
    else
        AGO_INFO() << "Disabling authentication: file does not exist";

    agoHttp.addHandler("/", boost::bind(&AgoImperiHome::handleReqHome, this, _1, _2, _3));
    agoHttp.addPrefixHandler("/devices", boost::bind(&AgoImperiHome::handleReqDevices, this, _1, _2, _3));
    agoHttp.addHandler("/system", boost::bind(&AgoImperiHome::handleReqSystem, this, _1, _2, _3));
    agoHttp.addHandler("/rooms", boost::bind(&AgoImperiHome::handleReqRooms, this, _1, _2, _3));

    try {
        agoHttp.start();
    } catch (const std::runtime_error &err) {
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

