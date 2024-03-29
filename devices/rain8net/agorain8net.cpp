/*
   Copyright (C) 2012 Harald Klein <hari@vt100.at>

   This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

   See the GNU General Public License for more details.

*/

#include <iostream>
#include <sstream>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include <termios.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

#include "agoapp.h"

#include "rain8.h"

using namespace agocontrol;

class AgoRain8net: public AgoApp {
private:
    rain8net rain8;
    int rc;

    void setupApp();
    Json::Value commandHandler(const Json::Value& content);
public:
    AGOAPP_CONSTRUCTOR(AgoRain8net);
};

Json::Value AgoRain8net::commandHandler(const Json::Value& content) {
    checkMsgParameter(content, "command", Json::stringValue);
    checkMsgParameter(content, "internalid");

    std::string command = content["command"].asString();
    std::string internalid = content["internalid"].asString();

    Json::Value returnval;
    int valve;
    if(!stringToInt(content["internalid"], valve))
        return responseError(RESPONSE_ERR_PARAMETER_INVALID, "internalid must be integer");

    if (command == "on" ) {
        if (rain8.zoneOn(1, valve) != 0) {
            AGO_ERROR() << "can't switch on valve " << valve;
            return responseError(RESPONSE_ERR_INTERNAL, "Cannot switch on rain8net output");
        } else {
            return responseSuccess();
        }
    } else if (command == "off") {
        if (rain8.zoneOff(1, valve) != 0) {
            AGO_ERROR() << "can't switch off valve " << valve;
            return responseError(RESPONSE_ERR_INTERNAL, "Cannot switch on rain8net output");
        } else {
            return responseSuccess();
        }
    }
    return responseUnknownCommand();
}

void AgoRain8net::setupApp() {
    std::string devicefile;

    devicefile=getConfigOption("device", "/dev/ttyS_01");

    if (rain8.init(devicefile.c_str()) != 0) {
        AGO_FATAL() << "can't open rainnet device " << devicefile;
        throw StartupError();
    }
    rain8.setTimeout(10000);
    if ((rc = rain8.comCheck()) != 0) {
        AGO_FATAL() << "can't talk to rainnet device " << devicefile << " - comcheck failed: " << rc;
        throw StartupError();
    }
    AGO_INFO() << "connection to rain8net established";

    for (int i=1; i<9; i++) {
        agoConnection->addDevice(std::to_string(i), "switch");
    }
    addCommandHandler();

    AGO_DEBUG() << "fetching zone status";
    unsigned char status;
    if (rain8.getStatus(1,status) == 0) {
        AGO_FATAL() << "can't get zone status, aborting";
        throw StartupError();
    } else {
        AGO_INFO() << "Zone status: 8:" << ((status & 128)?1:0) << " 7:" << ((status & 64)?1:0) << " 6:" << ((status &32)?1:0) << " 5:" 
            << ((status&16)?1:0) << " 4:" << ((status&8)?1:0) << " 3:" << ((status&4)?1:0) << " 2:" << ((status&2)?1:0) << " 1:" << ((status&1)?1:0);
    }

}

AGOAPP_ENTRY_POINT(AgoRain8net);
