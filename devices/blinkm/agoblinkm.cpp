#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <stdlib.h>
#include <sstream>
#include <fcntl.h>

#include <sys/ioctl.h>

#include "../i2c_include.h"

#include "agoapp.h"

using namespace agocontrol;

class AgoBlinkm: public AgoApp {
private:
    std::string devicefile;

    bool i2ccommand(const char *device, int i2caddr, int command, size_t size, __u8  *buf);
    Json::Value  commandHandler(const Json::Value& content);
    void setupApp();
public:
    AGOAPP_CONSTRUCTOR(AgoBlinkm);
};

bool AgoBlinkm::i2ccommand(const char *device, int i2caddr, int command, size_t size, __u8  *buf) {
    int file = open(device, O_RDWR);
    if (file < 0) {
        AGO_ERROR() << "cannot open " << file << " - error: " << file;
        return false;
    }
    else
        AGO_DEBUG() << "device open succeeded"  << device;

    if (ioctl(file, I2C_SLAVE, i2caddr) < 0) {
        AGO_ERROR() << "cannot open i2c slave: 0x" << std::hex << i2caddr;
        return false;
    }
    else
        AGO_DEBUG() << "open i2c slave succeeded: 0x" << std::hex << i2caddr;
    int result = i2c_smbus_write_i2c_block_data(file, command, size,buf);
    AGO_DEBUG() << "result: " << result;

    return true;
}

Json::Value  AgoBlinkm::commandHandler(const Json::Value& content) {
    Json::Value returnval;
    checkMsgParameter(content, "internalid", Json::stringValue);
    checkMsgParameter(content, "command", Json::stringValue);
    
    std::string internalid = content["internalid"].asString();
    std::string command = content["command"].asString();
    
    int i2caddr = atoi(internalid.c_str());
    __u8 buf[10];
    if (command == "on" ) {
        buf[0]=0xff;
        buf[1]=0xff;
        buf[2]=0xff;
        if (i2ccommand(devicefile.c_str(),i2caddr,0x63,3,buf))
        {
            agoConnection->emitEvent(internalid, "event.device.statechanged", "255", "");
            return responseSuccess();
        } else return responseFailed("Cannot write i2c command");
    } else if (command == "off") {
        buf[0]=0x0;
        buf[1]=0x0;
        buf[2]=0x0;
        if (i2ccommand(devicefile.c_str(),i2caddr,0x63,3,buf))
        {
            agoConnection->emitEvent(internalid, "event.device.statechanged", "0", "");
            return responseSuccess();
        } else return responseFailed("Cannot write i2c command");
    } else if (command == "setlevel") {
        checkMsgParameter(content, "level", Json::intValue);
        int level = content["level"].asInt();
        buf[0] = level * 255 / 100;
        buf[1] = level * 255 / 100;
        buf[2] = level * 255 / 100;
        if (i2ccommand(devicefile.c_str(),i2caddr,0x63,3,buf))
        {
            agoConnection->emitEvent(internalid, "event.device.statechanged", level, "");
            return responseSuccess();
        } else return responseFailed("Cannot write i2c command");
    } else if (command == "setcolor") {
        checkMsgParameter(content, "red", Json::uintValue);
        checkMsgParameter(content, "green", Json::uintValue);
        checkMsgParameter(content, "blue", Json::uintValue);

        int red = content["red"].asUInt();
        int green = content["green"].asUInt();
        int blue = content["blue"].asUInt();

        buf[0] = red * 255 / 100;
        buf[1] = green * 255 / 100;
        buf[2] = blue * 255 / 100;
        if (i2ccommand(devicefile.c_str(),i2caddr,0x63,3,buf))
        {
            return responseSuccess();
        } else return responseFailed("Cannot write i2c command");
    }
    return responseUnknownCommand();
}

void AgoBlinkm::setupApp() {
    devicefile=getConfigOption("bus", "/dev/i2c-0");
    std::stringstream devices(getConfigOption("devices", "9")); // read blinkm addr from config, default to addr 9

    std::string device;
    while (getline(devices, device, ',')) {
        agoConnection->addDevice(device, "dimmerrgb");
        i2ccommand(devicefile.c_str(),atoi(device.c_str()),0x6f,0,NULL); // stop script on blinkm
    } 
    addCommandHandler();
}

AGOAPP_ENTRY_POINT(AgoBlinkm);
