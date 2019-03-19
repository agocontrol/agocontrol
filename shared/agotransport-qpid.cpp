
#include <boost/bind.hpp>

#include <qpid/messaging/Connection.h>
#include <qpid/messaging/Receiver.h>
#include <qpid/messaging/Sender.h>
#include <qpid/messaging/Session.h>
#include <qpid/messaging/Message.h>
#include <qpid/messaging/Address.h>

#include "agotransport-qpid.h"
#include "agojson-qpid.h"
#include "agolog.h"

namespace agotransport = agocontrol::transport;

/**
 * Factory function for creating AgoQpidTransport via dlsym.
 */
extern "C" agocontrol::transport::AgoTransport *create_instance(const char *uri, const char *user, const char *password) {
    return new agocontrol::transport::AgoQpidTransport(uri, user, password);
}

class agotransport::QpidImpl {
public:
    void sendReply(const Json::Value &content, qpid::messaging::Address replyAddress);

    qpid::messaging::Connection connection;
    qpid::messaging::Sender sender;
    qpid::messaging::Receiver receiver;
    qpid::messaging::Session session;
};


agotransport::AgoQpidTransport::AgoQpidTransport(const std::string &uri, const std::string &user, const std::string &password) {
    impl.reset(new agotransport::QpidImpl());

    qpid::types::Variant::Map connectionOptions;
    connectionOptions["username"] = user;
    connectionOptions["password"] = password;
    connectionOptions["reconnect"] = "true";

    impl->connection = qpid::messaging::Connection(uri, connectionOptions);
}

agotransport::AgoQpidTransport::~AgoQpidTransport() {
    if(impl->receiver.isValid()) {
        AGO_DEBUG() << "Closing notification receiver";
        impl->receiver.close();
    }

    if(impl->session.isValid() && impl->connection.isValid()) {
        AGO_DEBUG() << "Closing pending broker connection";
        // Not yet connected, break out of connection attempt
        // TODO: This does not actually abort on old qpid
        impl->connection.close();
    }

    try {
        if(impl->connection.isOpen()) {
            AGO_DEBUG() << "Closing broker connection";
            impl->connection.close();
        }
    } catch(const std::exception& error) {
        AGO_ERROR() << "Failed to close broker connection: " << error.what();
    }
}

bool agotransport::AgoQpidTransport::start() {
    try {
        AGO_DEBUG() << "Opening QPid broker connection";
        impl->connection.open();
        impl->session = impl->connection.createSession();
        impl->sender = impl->session.createSender("agocontrol; {create: always, node: {type: topic}}");
    } catch(const std::exception& error) {
        AGO_FATAL() << "Failed to connect to broker: " << error.what();
        impl->connection.close();
        return false;
    }

    try {
        impl->receiver = impl->session.createReceiver("agocontrol; {create: always, node: {type: topic}}");
    } catch(const std::exception& error) {
        AGO_FATAL() << "Failed to create broker receiver: " << error.what();
        return false;
    }

    return true;
}

bool agotransport::AgoQpidTransport::sendMessage(Json::Value& message)
{
    qpid::messaging::Message qpmessage;
    std::string subject;
    qpid::types::Variant::Map msgMap = jsonToVariantMap(message["content"]);
    if(message.isMember("subject")) {
        subject = message["subject"].asString();
    }

    try {
        qpid::messaging::encode(msgMap, qpmessage);
        if (!subject.empty())
            qpmessage.setSubject(subject);

        AGO_TRACE() << "Sending message [src=" << qpmessage.getReplyTo() <<
                    ", sub="<< qpmessage.getSubject()<<"]: " << msgMap;
        impl->sender.send(qpmessage);
    } catch(const std::exception& error) {
        AGO_ERROR() << "Exception in sendMessage: " << error.what();
        return false;
    }


    return true;
}

agocontrol::AgoResponse agotransport::AgoQpidTransport::sendRequest(Json::Value& message, std::chrono::milliseconds timeout)
{
    AgoResponse r;
    qpid::messaging::Message qpmessage;
    qpid::messaging::Receiver responseReceiver;
    qpid::messaging::Session recvsession = impl->connection.createSession();

    qpid::types::Variant::Map msgMap = jsonToVariantMap(message["content"]);
    std::string subject;
    if(message.isMember("subject")) {
        subject = message["subject"].asString();
    }

    try {
        encode(msgMap, qpmessage);
        if(!subject.empty())
            qpmessage.setSubject(subject);

        qpid::messaging::Address responseQueue("#response-queue; {create:always, delete:always}");
        responseReceiver = recvsession.createReceiver(responseQueue);
        qpmessage.setReplyTo(responseQueue);

        AGO_TRACE() << "Sending request [sub=" << subject << ", replyTo=" << responseQueue <<"]" << msgMap;
        impl->sender.send(qpmessage);

        qpid::messaging::Message message = responseReceiver.fetch(qpid::messaging::Duration(timeout.count()));

        try {
            Json::Value response;
            if (message.getContentSize() > 3) {
                qpid::types::Variant::Map responseMap;
                decode(message, responseMap);
                variantMapToJson(responseMap, response);
            }else{
                Json::Value err;
                err["message"] = "invalid.response";
                response["error"] = err;
            }

            r.init(response);
            AGO_TRACE() << "Remote response received: " << r.response;
        }catch(const std::invalid_argument& ex) {
            AGO_ERROR() << "Failed to initate response, wrong response format? Error: "
                        << ex.what()
                        << ". Message: " << r.response;

            r.init(responseError(RESPONSE_ERR_INTERNAL, ex.what()));
        }
        recvsession.acknowledge();

    } catch (const qpid::messaging::NoMessageAvailable&) {
        AGO_WARNING() << "No reply for message sent to subject " << subject;

        r.init(responseError(RESPONSE_ERR_NO_REPLY, "Timeout"));
    } catch(const std::exception& ex) {
        AGO_ERROR() << "Exception in sendRequest: " << ex.what();

        r.init(responseError(RESPONSE_ERR_INTERNAL, ex.what()));
    }

    recvsession.close();
    return r;
}

agotransport::AgoTransportMessage agotransport::AgoQpidTransport::fetchMessage(std::chrono::milliseconds timeout)
{
    AgoTransportMessage ret;
    try {
        qpid::types::Variant::Map contentMap;
        qpid::messaging::Message message = impl->receiver.fetch(qpid::messaging::Duration(timeout.count()));
        impl->session.acknowledge();

        // workaround for bug qpid-3445
        if (message.getContent().size() < 4) {
            throw qpid::messaging::EncodingException("message too small");
        }

        qpid::messaging::decode(message, contentMap);
        variantMapToJson(contentMap, ret.message["content"]);

        AGO_TRACE() << "Incoming message [src=" << message.getReplyTo() <<
                    ", sub="<< message.getSubject()<<"]: " << ret.message;

        const qpid::messaging::Address replyaddress = message.getReplyTo();

        if(!message.getSubject().empty())
            ret.message["subject"] = message.getSubject();

        ret.replyFuction = boost::bind(&QpidImpl::sendReply, boost::ref(*impl), _1, replyaddress);

    } catch(const qpid::messaging::NoMessageAvailable& error) {

    } catch(const std::exception& error) {
        if(shutdownSignaled)
            return ret;

        AGO_ERROR() << "Exception in message loop: " << error.what();

        if (impl->session.hasError()) {
            AGO_ERROR() << "Session has error, recreating";
            impl->session.close();
            impl->session = impl->connection.createSession();
            impl->receiver = impl->session.createReceiver("agocontrol; {create: always, node: {type: topic}}");
            impl->sender = impl->session.createSender("agocontrol; {create: always, node: {type: topic}}");
        }

        usleep(50);
    }

    return ret;
}

void agotransport::QpidImpl::sendReply(const Json::Value& content, const qpid::messaging::Address replyAddress)
{
    qpid::messaging::Message response;
    qpid::types::Variant::Map responseMap = jsonToVariantMap(content);
    AGO_TRACE() << "[qpid] sending reply " << content;

    qpid::messaging::Session replysession = connection.createSession();
    try {
        qpid::messaging::Sender replysender = replysession.createSender(replyAddress);
        qpid::messaging::encode(responseMap, response);
        //response.setSubject(instance);
        replysender.send(response);
    } catch(const std::exception& error) {
        AGO_ERROR() << "[qpid] failed to send reply: " << error.what();;
    }
    replysession.close();
}
