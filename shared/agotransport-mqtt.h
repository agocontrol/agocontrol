#ifndef AGOCONNECTION_MQTT_H
#define AGOCONNECTION_MQTT_H

#include "agotransport.h"

namespace agocontrol {
namespace transport {

class MqttImpl;

class AgoMqttTransport : public AgoTransport {
public:
    AgoMqttTransport(const std::string &id, const std::string &broker, const std::string &user, const std::string &password);
    ~AgoMqttTransport();

    bool start();
    void shutdown();

    bool sendMessage(Json::Value &message);
    agocontrol::AgoResponse sendRequest(Json::Value &message, std::chrono::milliseconds timeout);
    AgoTransportMessage fetchMessage(std::chrono::milliseconds timeout);

private:
    std::unique_ptr<MqttImpl> impl;
};

};
};

/**
 * Definition matching the "create_instance" in agotransport-mqtt.cpp, for creating via dlsym.
 */
typedef agocontrol::transport::AgoTransport* agotransport_mqtt_factory(const char *id, const char *broker, const char *user, const char *password);


#endif //AGOCONNECTION_MQTT_H
