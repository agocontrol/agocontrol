/*
   Copyright (C) 2012 Harald Klein <hari@vt100.at>

   This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

   See the GNU General Public License for more details.

*/

#include <iostream>
#include <string.h>

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

#include "agoapp.h"

#include "kwikwai.h"

using namespace agocontrol;

class AgoKwikwai: public AgoApp {
private:
    kwikwai::Kwikwai *myKwikwai;

    void setupApp();
    Json::Value commandHandler(const Json::Value& content);
public:
    AGOAPP_CONSTRUCTOR(AgoKwikwai);
};

Json::Value AgoKwikwai::commandHandler(const Json::Value& content) {
    checkMsgParameter(content, "command", Json::stringValue);
    checkMsgParameter(content, "internalid", Json::stringValue);

    std::string command = content["command"].asString();
    std::string internalid = content["internalid"].asString();

    Json::Value returnval;
    if (internalid == "hdmicec") {
        if (command == "alloff" ) {
            myKwikwai->cecSend("FF:36");
            return responseSuccess();
        }
    } else if (internalid == "tv") {
        if (command == "on" ) {
            myKwikwai->cecSend("F0:04");
            return responseSuccess();
        } else if (command == "off" ) {
            myKwikwai->cecSend("F0:36");
            return responseSuccess();
        }
    }
    return responseUnknownCommand();
}

void AgoKwikwai::setupApp() {
    std::string hostname;
    std::string port;

    hostname=getConfigOption("host", "kwikwai.local");
    port=getConfigOption("port", "9090");

    kwikwai::Kwikwai _myKwikwai(hostname.c_str(), port.c_str());
    myKwikwai = &_myKwikwai;
    AGO_INFO() << "Version: " << myKwikwai->getVersion();

    agoConnection->addDevice("hdmicec", "hdmicec");
    agoConnection->addDevice("tv", "tv");
    addCommandHandler();
}

AGOAPP_ENTRY_POINT(AgoKwikwai);

