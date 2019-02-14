#include <boost/asio.hpp> 
#include <boost/foreach.hpp> 

#include "agoapp.h"
#include "yamaha_device.h"

using namespace agocontrol;

class AgoYamaha: public AgoApp {
private:
    void setupApp();
    void cleanupApp();
    Json::Value commandHandler(const Json::Value& command);

    std::list<YamahaDevice *> devices;

public:
    AGOAPP_CONSTRUCTOR(AgoYamaha);
};

Json::Value AgoYamaha::commandHandler(const Json::Value& content) {
    checkMsgParameter(content, "command", Json::stringValue);
    checkMsgParameter(content, "internalid", Json::stringValue);

    std::string command = content["command"].asString();
    std::string internalid = content["internalid"].asString();

    // Find device
    // XX Ugly
    YamahaDevice *dev = NULL;
    BOOST_FOREACH(YamahaDevice *i, devices) {
        if(i->endpoint().address().to_string() == internalid) {
            dev = i;
            break;
        }
    }
    if(!dev) {
        AGO_WARNING() << "Got command for unknown device " << internalid;
        return responseError(RESPONSE_ERR_INTERNAL, "Received command for unknown device");
    }

    if (command == "on") {
        AGO_INFO() << "Switch " << internalid << " ON";
        dev->powerOn();
    } else if (command == "off") {
        AGO_INFO() << "Switch " << internalid << " OFF";
        dev->powerOff();
    } else if (command == "mute") {
        AGO_INFO() << "Muting " << internalid;
        dev->mute();
    } else if (command == "unmute") {
        AGO_INFO() << "Unmuting " << internalid;
        dev->unmute();
    } else if (command == "mutetoggle") {
        AGO_INFO() << "Toggling mute on " << internalid;
        dev->muteToggle();
    } else if (command == "vol+") {
        AGO_INFO() << "Increasing volume on " << internalid;
        // XXX: Dangerous if we queue up alot which cannot raech!
        dev->volIncr();
    } else if (command == "vol-") {
        AGO_INFO() << "Decreasing volume on " << internalid;
        dev->volDecr();
    } else if (command == "setlevel") {
        checkMsgParameter(content, "level", Json::realValue);
        float level = content["level"].asFloat();

        if(level > -10)
            // XXX: Temp protect
            level = -10;

        AGO_INFO() << "Setting volume on " << internalid << " to " << level;
        dev->volSet(level);
    }

    return responseSuccess();
}

void AgoYamaha::setupApp() {
    // TODO: upnp auto-discovery
    // TODO: configuration file...
    // TODO: value callback in YamahaDevice
    devices.push_back(
            new YamahaDevice(ioService(), ip::address::from_string("172.28.4.130"))
        );

    agoConnection->addDevice("172.28.4.130", "avreceiver");
    addCommandHandler();
}

void AgoYamaha::cleanupApp() {
    BOOST_FOREACH(YamahaDevice *device, devices) {
        device->shutdown();
    }
}

AGOAPP_ENTRY_POINT(AgoYamaha);


