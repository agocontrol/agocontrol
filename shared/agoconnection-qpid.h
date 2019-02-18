#ifndef AGOCONNECTION_QPID_H
#define AGOCONNECTION_QPID_H


#include <qpid/messaging/Connection.h>
#include <qpid/messaging/Receiver.h>
#include <qpid/messaging/Sender.h>
#include <qpid/messaging/Session.h>
#include <qpid/messaging/Message.h>
#include <qpid/messaging/Address.h>

#include "agoconnection-impl.h"

namespace agocontrol {

class AgoQPIDImpl : public AgoConnectionImpl {
public:
    AgoQPIDImpl(const std::string& uri, const std::string& user, const std::string& password);
    ~AgoQPIDImpl();

    bool start();

    bool sendMessage(const std::string& topic, const Json::Value& content);
    AgoResponse sendRequest(const std::string& topic, const Json::Value& content, std::chrono::milliseconds timeout);
    AgoConnectionMessage fetchMessage(std::chrono::milliseconds timeout);

private:
    void sendReply(const Json::Value& content, qpid::messaging::Address replyAddress);

    qpid::messaging::Connection connection;
    qpid::messaging::Sender sender;
    qpid::messaging::Receiver receiver;
    qpid::messaging::Session session;
};

};
#endif //AGOCONNECTION_QPID_H
