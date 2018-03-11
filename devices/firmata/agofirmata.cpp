#include <iostream>
#include <stdlib.h>
#include <sstream>

#include "agoapp.h"
#include "firmata.h"


using namespace agocontrol;

class AgoFirmata: public AgoApp {
private:
    std::unique_ptr<Firmata> f;

    Json::Value commandHandler(const Json::Value& content);
    void setupApp();
    void cleanupApp();
public:
    AGOAPP_CONSTRUCTOR(AgoFirmata);
};

Json::Value AgoFirmata::commandHandler(const Json::Value& content) {
    checkMsgParameter(content, "command", Json::stringValue);
    checkMsgParameter(content, "internalid");

    std::string command = content["command"].asString();
    std::string internalid = content["internalid"].asString();

    Json::Value returnval;
    int pin;
    if(!stringToInt(content["internalid"], pin))
        return responseError(RESPONSE_ERR_PARAMETER_INVALID, "internalid must be integer");

    int result;
    if (content["command"] == "on" ) {
        result = f->writeDigitalPin(pin, ARDUINO_HIGH);
        // TODO: send proper status events
    } else if (content["command"] == "off") {
        result = f->writeDigitalPin(pin, ARDUINO_LOW);
    }
    else
        return responseUnknownCommand();

    if (result >= 0)
    {
        return responseSuccess();
    }

    return responseError(RESPONSE_ERR_INTERNAL, "Cannot set firmata digital pin");
}


void AgoFirmata::setupApp() {
    std::string devicefile=getConfigOption("device", "/dev/ttyUSB2");
    std::stringstream outputs(getConfigOption("outputs", "2")); // read digital out pins from config, default to pin 2 only

    f.reset(new Firmata());
    if (f->openPort(devicefile.c_str()) != 0) {
        AGO_FATAL() << "cannot open device: " << devicefile;
        f->destroy();
        throw StartupError();
    }

    AGO_INFO() << "Firmata version: " <<  f->getFirmwareVersion();

    std::string output;
    while (getline(outputs, output, ',')) {
        f->setPinMode(atoi(output.c_str()), FIRMATA_OUTPUT);
        agoConnection->addDevice(output, "switch");
        AGO_INFO() << "adding DIGITAL out pin as switch: " << output;
    } 
    addCommandHandler();
}

void AgoFirmata::cleanupApp() {
    f->destroy();
}

AGOAPP_ENTRY_POINT(AgoFirmata);
