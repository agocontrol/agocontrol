/*
   Copyright (C) 2009 Harald Klein <hari@vt100.at>

   This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

   See the GNU General Public License for more details.

*/

#include <iostream>
#include <sstream>
#include <uuid/uuid.h>
#include <stdlib.h>

#include <unistd.h>
#include <pthread.h>
#include <stdio.h>

#include "agoapp.h"
#include "esp3.h"

#ifndef DEVICEMAPFILE
#define DEVICEMAPFILE "/maps/enocean3.json"
#endif

using namespace agocontrol;

class AgoEnocean3: public AgoApp {
private:
    esp3::ESP3 *myESP3;
    bool learnmode;
    Json::Value devicemap;
    void setupApp();
    Json::Value commandHandler(const Json::Value& content);
public:
    AGOAPP_CONSTRUCTOR_HEAD(AgoEnocean3)
        , learnmode(false)
        {}

    void _handler(esp3::Notification const* message);
};

void handler(esp3::Notification const* _message, void *context) {
    AgoEnocean3 *inst = static_cast<AgoEnocean3*>(context);
    inst->_handler(_message);
}

void AgoEnocean3::_handler(esp3::Notification const* message) {
    AGO_DEBUG() << "Received Enocean Notification type: " << message->getType() << " ID: " << std::hex << message->getId();
    std::stringstream id;
    id << std::hex << message->getId();
    if (message->getType() == "Switch") {
        const esp3::SwitchNotification *switchNotif = (const esp3::SwitchNotification *)message;
        if (switchNotif->getIsPressed()) AGO_DEBUG() << "isPressed() == true";
        AGO_DEBUG() << "Rocker id: " << switchNotif->getRockerId();
        if (learnmode) {
            learnmode=false;
            int max;
            if (switchNotif->getRockerId() > 2) { // this is a two paddle switch - beware, user has to press the second paddle in learn mode for this detection to work
                max = 4;
            } else max = 2;
                
            for (int i=1; i<= max; i++) {
                std::stringstream internalid;
                internalid << id.str() << "-" << i;
                devicemap[internalid.str()]="remoteswitch";
                agoConnection->addDevice(internalid.str(), "remoteswitch");
            }
            writeJsonFile(devicemap, getConfigPath(DEVICEMAPFILE));
        } else {
            if (switchNotif->getIsPressed()) {
                std::stringstream internalid;
                internalid << id.str() << "-" << switchNotif->getRockerId();
                agoConnection->emitEvent(internalid.str(), "event.device.statechanged", 255, "");
            } else {
                for (auto it = devicemap.begin(); it!=devicemap.end(); it++) {
                    AGO_DEBUG() << "matching id: " << it.name();
                    if (it.name().find(id.str()) != std::string::npos) {
                        agoConnection->emitEvent(it.name(), "event.device.statechanged", 0, "");
                    }
                }
            }
        }
    }
}

Json::Value AgoEnocean3::commandHandler(const Json::Value& content) {
    std::string internalid = content["internalid"].asString();
    if (internalid == "enoceancontroller") {
        if (content["command"] == "teachframe") {
            checkMsgParameter(content, "channel", Json::intValue);
            // this is optional - checkMsgParameter(content, "profile", Json::stringValue);
            int channel = content["channel"].asInt();
            std::string profile = content["profile"].asString();
            if (profile == "central command dimming") {
                myESP3->fourbsCentralCommandDimTeachin(channel);
            } else {
                myESP3->fourbsCentralCommandSwitchTeachin(channel);
            }
            return responseSuccess();
        } else if (content["command"] == "setlearnmode") {
            checkMsgParameter(content, "mode");
            if (content["mode"].asString() == "start") {
                learnmode = true;
            } else {
                learnmode = false;
            }
            return responseSuccess();
        } else if (content["command"] == "setidbase") {
            return responseError(RESPONSE_ERR_INTERNAL, "set id base not yet implemented");
        }
    } else {
        int rid = 0; rid = atol(internalid.c_str());
        if (content["command"] == "on") {
            if (agoConnection->getDeviceType(internalid)=="dimmer") {
                myESP3->fourbsCentralCommandDimLevel(rid,0x64,1);
            } else {
                myESP3->fourbsCentralCommandSwitchOn(rid);
            }
            return responseSuccess();
        } else if (content["command"] == "off") {
            if (agoConnection->getDeviceType(internalid)=="dimmer") {
                myESP3->fourbsCentralCommandDimOff(rid);
            } else {
                myESP3->fourbsCentralCommandSwitchOff(rid);
            }
            return responseSuccess();
        } else if (content["command"] == "setlevel") {
            checkMsgParameter(content, "level", Json::intValue);
            uint8_t level = 0;
            level = content["level"].asInt();
            myESP3->fourbsCentralCommandDimLevel(rid,level,1);
            return responseSuccess();
        }
    }
    return responseUnknownCommand();
}

void AgoEnocean3::setupApp() {
    std::string devicefile;
    devicefile=getConfigOption("device", "/dev/ttyAMA0");
    myESP3 = new esp3::ESP3(devicefile);
    if (!myESP3->init()) {
        AGO_FATAL() << "cannot initalize enocean ESP3 protocol on device " << devicefile;
        throw StartupError();
    }
    myESP3->setHandler(handler, this);

    addCommandHandler();
    agoConnection->addDevice("enoceancontroller", "enoceancontroller");

    std::stringstream dimmers(getConfigOption("dimmers", "1"));
    std::string dimmer;
    while (getline(dimmers, dimmer, ',')) {
        agoConnection->addDevice(dimmer, "dimmer");
        AGO_DEBUG() << "adding rid " << dimmer << " as dimmer";
    }

    std::stringstream switches(getConfigOption("switches", "20"));
    std::string switchdevice;
    while (getline(switches, switchdevice, ',')) {
        agoConnection->addDevice(switchdevice, "switch");
        AGO_DEBUG() << "adding rid " << switchdevice << " as switch";
    } 

    readJsonFile(devicemap, getConfigPath(DEVICEMAPFILE));
    for (auto it = devicemap.begin(); it!=devicemap.end(); it++) {
        AGO_DEBUG() << "Found id in devicemap file: " << it.name();
        agoConnection->addDevice(it.name(), it->asString());
    }
}

AGOAPP_ENTRY_POINT(AgoEnocean3);
