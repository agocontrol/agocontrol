/*
   Copyright (C) 2012 Harald Klein <hari@vt100.at>

   This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

   See the GNU General Public License for more details.

*/

#include <iostream>
#include <stdlib.h>

#include <unistd.h>
#include <stdio.h>

#include <tinyxml2.h>

#include <ola/DmxBuffer.h>
#include <ola/Logging.h>
#include <ola/StreamingClient.h>

#include "agoapp.h"

using namespace tinyxml2;
using namespace agocontrol;
namespace fs = ::boost::filesystem;

class AgoLogDestination: public ola::LogDestination {
    public:
        void Write(ola::log_level level, const std::string &log_line);
};

class AgoDmx: public AgoApp {
private:
    Json::Value channelMap;
    ola::DmxBuffer buffer;
    ola::StreamingClient ola_client;
    AgoLogDestination *agoLogDestination;
    int universe;

    void setupApp();
    void cleanupApp();
    Json::Value commandHandler(const Json::Value& content);

    bool setDevice_strobe(const Json::Value& device, int strobe);
    void ola_setChannel(int channel, int value);
    bool setDevice_color(const Json::Value& device, int red, int green, int blue);
    bool setDevice_level(const Json::Value& device, int level);
    bool ola_send();
    void reportDevices();
    bool loadChannels(std::string filename, Json::Value& _channelMap);

public:
    AGOAPP_CONSTRUCTOR(AgoDmx);
};

void AgoLogDestination::Write(ola::log_level level, const std::string &log_line) {
    std::string line = log_line;
    replaceString(line, "\n"," ");

    if (level == OLA_FATAL) AGO_FATAL() << "[OLA] " << line;
    else if (level == OLA_WARN) AGO_WARNING() <<  "[OLA] " << line;
    else if (level == OLA_DEBUG) AGO_DEBUG() <<  "[OLA] " << line;
    else if (level == OLA_INFO) AGO_INFO() <<  "[OLA] " << line;
    else AGO_DEBUG() << "[OLA] " << line;
}

/**
 * parses the device XML file and creates a qpid::types::Variant::Map with the data
 */
bool AgoDmx::loadChannels(std::string filename, Json::Value& _channelMap) {
    XMLDocument channelsFile;
    int returncode;

    AGO_INFO() << "trying to open channel file: " << filename;
    returncode = channelsFile.LoadFile(filename.c_str());
    if (returncode != XML_NO_ERROR) {
        AGO_ERROR() << "error loading XML file, code: " << returncode;
        return false;
    }

    AGO_TRACE() << "parsing file";
    XMLHandle docHandle(&channelsFile);
    XMLElement* device = docHandle.FirstChildElement( "devices" ).FirstChild().ToElement();
    if (device) {
        XMLElement *nextdevice = device;
        while (nextdevice != NULL) {
            Json::Value content;

            AGO_TRACE() << "node: " << nextdevice->Attribute("internalid") << " type: " << nextdevice->Attribute("type");

            content["internalid"] = nextdevice->Attribute("internalid");
            content["devicetype"] = nextdevice->Attribute("type");
            content["onlevel"] = 100;
            XMLElement *channel = nextdevice->FirstChildElement( "channel" );
            if (channel) {
                XMLElement *nextchannel = channel;
                while (nextchannel != NULL) {
                    AGO_DEBUG() << "channel: " << nextchannel->GetText() << " type: " << nextchannel->Attribute("type");

                    content[nextchannel->Attribute("type")] = parseToJson(nextchannel->GetText());

                    nextchannel = nextchannel->NextSiblingElement();
                }
            }
            _channelMap[nextdevice->Attribute("internalid")] = content;
            nextdevice = nextdevice->NextSiblingElement();
        }
    }
    return true;
}

/**
 * set a channel to off
 */
void AgoDmx::ola_setChannel(int channel, int value) {
    AGO_DEBUG() << "Setting channel " << channel << " to value " << value;
    buffer.SetChannel(channel, value);
}

/**
 * send buffer to ola
 */
bool AgoDmx::ola_send() {
    if (!ola_client.SendDmx(universe, buffer)) {
        AGO_ERROR() << "Send to dmx failed for universe " << universe;
        return false;
    }
    return true;
}

/**
 * set a device to a color
 */
bool AgoDmx::setDevice_color(const Json::Value& device, int red=0, int green=0, int blue=0) {
    int channel_red = device["red"].asInt();
    int channel_green = device["green"].asInt();
    int channel_blue = device["blue"].asInt();
    ola_setChannel(channel_red, red);
    ola_setChannel(channel_green, green);
    ola_setChannel(channel_blue, blue);
    return ola_send();
}

/**
 * set device level 
 */
bool AgoDmx::setDevice_level(const Json::Value& device, int level=0) {
    if (device.isMember("level")) {
        int channel = device["level"].asInt();
        ola_setChannel(channel, (int) ( 255.0 * level / 100 ));
        return ola_send();
    } else {
        int channel_red = device["red"].asInt();
        int channel_green = device["green"].asInt();
        int channel_blue = device["blue"].asInt();

        int red = (int) ( buffer.Get(channel_red) * level / 100);
        int green = (int) ( buffer.Get(channel_green) * level / 100);
        int blue = (int) ( buffer.Get(channel_blue) * level / 100);

        return setDevice_color(device, red, green, blue);
    }
}

/**
 * set device strobe
 */
bool AgoDmx::setDevice_strobe(const Json::Value& device, int strobe=0) {
    if (device.isMember("strobe")) {
        int channel = device["strobe"].asInt();
        ola_setChannel(channel, (int) ( 255.0 * strobe / 100 ));
        return ola_send();
    } else {
        AGO_ERROR() << "Strobe command not supported on device";
        return false;
    }
}

/**
 * announces our devices in the channelmap to the resolver
 */
void AgoDmx::reportDevices() {
    for (auto it = channelMap.begin(); it != channelMap.end(); ++it) {
        const Json::Value& device = *it;
        agoConnection->addDevice(device["internalid"].asString(), device["devicetype"].asString());
    }
}

Json::Value AgoDmx::commandHandler(const Json::Value& content) {
    checkMsgParameter(content, "command", Json::stringValue);
    checkMsgParameter(content, "internalid", Json::stringValue);

    std::string command = content["command"].asString();
    std::string internalid = content["internalid"].asString();

    Json::Value& device = channelMap[internalid];

    if (command == "on") {
        if (setDevice_level(device, device["onlevel"].asInt())) {
            agoConnection->emitEvent(internalid, "event.device.statechanged", device["onlevel"].asInt(), "");
            return responseSuccess();
        } else return responseFailed("cannot set device level via OLA");
    } else if (command == "off") {
        if (setDevice_level(device, 0)) {
            agoConnection->emitEvent(internalid, "event.device.statechanged", 0, "");
            return responseSuccess();
        } else return responseFailed("cannot set device level via OLA");
    } else if (command == "setlevel") {
        checkMsgParameter(content, "level", Json::intValue);
        if (setDevice_level(device, content["level"].asInt())) {
            device["onlevel"] = content["level"];
            agoConnection->emitEvent(internalid, "event.device.statechanged", content["level"].asInt(), "");
            return responseSuccess();
        } else return responseFailed("cannot set device level via OLA");
    } else if (command == "setcolor") {
        checkMsgParameter(content, "red", Json::intValue);
        checkMsgParameter(content, "green", Json::intValue);
        checkMsgParameter(content, "blue", Json::intValue);
        if (setDevice_color(device, content["red"].asInt(), content["green"].asInt(), content["blue"].asInt())) {
            return responseSuccess();
        } else return responseFailed("cannot set device color via OLA");
    } else if (command == "setstrobe") {
        if (setDevice_strobe(device, content["strobe"].asInt())) {
            return responseSuccess();
        } else return responseFailed("cannot set device strobe via OLA");
    }
    return responseUnknownCommand();
}

void AgoDmx::setupApp() {
    fs::path channelsFile;
    std::string ola_server;

    channelsFile=getConfigOption("channelsfile", getConfigPath("/dmx/channels.xml"));
    ola_server=getConfigOption("url", "ip:127.0.0.1");
    universe =  atoi(getConfigOption("universe","0").c_str());
    // load xml file into map
    if (!loadChannels(channelsFile.string(), channelMap)) {
        AGO_FATAL() << "Can't load channel xml";
        throw StartupError();
    }

    // connect to OLA
    // turn on OLA logging
    //ola::InitLogging(ola::OLA_LOG_WARN, ola::OLA_LOG_STDERR);
    AGO_DEBUG() << "Setting up OLA log destination";
    agoLogDestination = new AgoLogDestination();
    ola::InitLogging(ola::OLA_LOG_WARN, agoLogDestination);

    // set all channels to 0
    AGO_TRACE() << "Blacking out DMX buffer";
    buffer.Blackout();

    // Setup the client, this connects to the server
    AGO_DEBUG() << "Setting up OLA client";
    if (!ola_client.Setup()) {
        AGO_FATAL() << "OLA client setup failed";
        throw StartupError();
    }

    AGO_TRACE() << "reporting devices";
    reportDevices();

    addCommandHandler();

}

void AgoDmx::cleanupApp() {
    ola_client.Stop();
}

AGOAPP_ENTRY_POINT(AgoDmx);

