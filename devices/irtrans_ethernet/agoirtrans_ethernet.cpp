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
#include <sys/socket.h>
#include <netdb.h>

#include <fcntl.h>
#include <string.h>

#include <termios.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

#include "agoapp.h"

using namespace agocontrol;

class AgoIrtrans_Ethernet: public AgoApp {
private:
    int irtrans_socket;
    struct sockaddr_in server_addr;
    struct hostent *host;

    void setupApp();
    Json::Value commandHandler(const Json::Value& content);
public:
    AGOAPP_CONSTRUCTOR(AgoIrtrans_Ethernet);
};

Json::Value AgoIrtrans_Ethernet::commandHandler(const Json::Value& content) {
    checkMsgParameter(content, "command", Json::stringValue);
    std::string command = content["command"].asString();

    checkMsgParameter(content, "internalid");
    int internalid;
    if(!stringToInt(content["internalid"], internalid))
        return responseError(RESPONSE_ERR_PARAMETER_INVALID, "internalid must be integer");

    AGO_TRACE() << "Command: " << content["command"] << " internal id: " << internalid;
    if (content["command"] == "sendir" ) {
        checkMsgParameter(content, "ircode", Json::stringValue);
        AGO_DEBUG() << "sending IR code";
        std::string udpcommand;
        udpcommand.assign("sndccf ");
        udpcommand.append(content["ircode"].asString());
        sendto(irtrans_socket, udpcommand.c_str(), udpcommand.length(), 0, (struct sockaddr *)&server_addr, sizeof(struct sockaddr));
        // TODO: Determine sane result code
        return responseSuccess();
    }

    return responseUnknownCommand();
}

void AgoIrtrans_Ethernet::setupApp() {
    std::string hostname;
    std::string port;

    hostname=getConfigOption("host", "192.168.80.12");
    port=getConfigOption("port", "21000");

    host= (struct hostent *) gethostbyname((char *)hostname.c_str());

    if ((irtrans_socket = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        AGO_FATAL() << "Cannot open UDP socket";
        throw StartupError();
    }

    server_addr.sin_family = AF_INET;

    // read the port from device data, TCP is a bit misleading, we do UDP
    server_addr.sin_port = htons(atoi(port.c_str()));

    server_addr.sin_addr = *((struct in_addr *)host->h_addr);
    bzero(&(server_addr.sin_zero),8);

    agoConnection->addDevice("1", "infraredblaster");
    addCommandHandler();
}

AGOAPP_ENTRY_POINT(AgoIrtrans_Ethernet);
