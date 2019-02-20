#ifndef AGOCONNECTION_MQTT_H
#define AGOCONNECTION_MQTT_H

#include "agoconnection-impl.h"

namespace agocontrol {

class MosquittoAdapter;
class AgoMQTTImpl : public AgoConnectionImpl {
public:
    AgoMQTTImpl(const std::string& id, const std::string& host, int port);
    ~AgoMQTTImpl();

    bool start();
    bool sendMessage(const std::string &topic, const Json::Value &content);

    agocontrol::AgoResponse sendRequest(const std::string &topic, const Json::Value &content, std::chrono::milliseconds timeout);
    AgoConnectionMessage fetchMessage(std::chrono::milliseconds timeout);

private:
    void sendReply(const Json::Value& content, std::string replyAddress);
    std::unique_ptr<MosquittoAdapter> adapter;
};

};
#endif //AGOCONNECTION_MQTT_H
