
#include <boost/bind.hpp>

#include "agoconnection-qpid.h"
#include "agojson-qpid.h"
#include "agolog.h"

agocontrol::AgoQPIDImpl::AgoQPIDImpl(const std::string& uri, const std::string& user, const std::string& password) {
    qpid::types::Variant::Map connectionOptions;
    connectionOptions["username"] = user;
    connectionOptions["password"] = password;
    connectionOptions["reconnect"] = "true";

    connection = qpid::messaging::Connection(uri, connectionOptions);
}

agocontrol::AgoQPIDImpl::~AgoQPIDImpl() {
    if(receiver.isValid()) {
        AGO_DEBUG() << "Closing notification receiver";
        receiver.close();
    }

    if(session.isValid() && connection.isValid()) {
        AGO_DEBUG() << "Closing pending broker connection";
        // Not yet connected, break out of connection attempt
        // TODO: This does not actually abort on old qpid
        connection.close();
    }

    try {
        if(connection.isOpen()) {
            AGO_DEBUG() << "Closing broker connection";
            connection.close();
        }
    } catch(const std::exception& error) {
        AGO_ERROR() << "Failed to close broker connection: " << error.what();
    }
}

bool agocontrol::AgoQPIDImpl::start() {
    try {
        AGO_DEBUG() << "Opening QPid broker connection";
        connection.open();
        session = connection.createSession();
        sender = session.createSender("agocontrol; {create: always, node: {type: topic}}");
    } catch(const std::exception& error) {
        AGO_FATAL() << "Failed to connect to broker: " << error.what();
        connection.close();
        return false;
    }

    try {
        receiver = session.createReceiver("agocontrol; {create: always, node: {type: topic}}");
    } catch(const std::exception& error) {
        AGO_FATAL() << "Failed to create broker receiver: " << error.what();
        return false;
    }

    return true;
}

bool agocontrol::AgoQPIDImpl::sendMessage(const Json::Value& message)
{
    qpid::messaging::Message qpmessage;
    std::string subject;
    qpid::types::Variant::Map msgMap = jsonToVariantMap(message);
    if(message.isMember("subject")) {
        subject = message["subject"].asString();
        msgMap.erase("subject");
    }

    try {
        qpid::messaging::encode(msgMap, qpmessage);
        if (!subject.empty())
            qpmessage.setSubject(subject);

        AGO_TRACE() << "Sending message [src=" << qpmessage.getReplyTo() <<
                    ", sub="<< qpmessage.getSubject()<<"]: " << msgMap;
        sender.send(qpmessage);
    } catch(const std::exception& error) {
        AGO_ERROR() << "Exception in sendMessage: " << error.what();
        return false;
    }


    return true;
}

agocontrol::AgoResponse agocontrol::AgoQPIDImpl::sendRequest(const Json::Value& message, std::chrono::milliseconds timeout)
{
    AgoResponse r;
    qpid::messaging::Message qpmessage;
    qpid::messaging::Receiver responseReceiver;
    qpid::messaging::Session recvsession = connection.createSession();

    qpid::types::Variant::Map msgMap = jsonToVariantMap(message);
    std::string subject;
    if(message.isMember("subject")) {
        subject = message["subject"].asString();
        msgMap.erase("subject");
    }
    try {
        encode(msgMap, qpmessage);
        if(!subject.empty())
            qpmessage.setSubject(subject);

        qpid::messaging::Address responseQueue("#response-queue; {create:always, delete:always}");
        responseReceiver = recvsession.createReceiver(responseQueue);
        qpmessage.setReplyTo(responseQueue);

        AGO_TRACE() << "Sending request [sub=" << subject << ", replyTo=" << responseQueue <<"]" << msgMap;
        sender.send(qpmessage);

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

agocontrol::AgoConnectionMessage agocontrol::AgoQPIDImpl::fetchMessage(std::chrono::milliseconds timeout)
{
    AgoConnectionMessage ret;
    try {
        qpid::types::Variant::Map contentMap;
        qpid::messaging::Message message = receiver.fetch(qpid::messaging::Duration(timeout.count()));
        session.acknowledge();

        // workaround for bug qpid-3445
        if (message.getContent().size() < 4) {
            throw qpid::messaging::EncodingException("message too small");
        }

        qpid::messaging::decode(message, contentMap);
        variantMapToJson(contentMap, ret.msg);

        AGO_TRACE() << "Incoming message [src=" << message.getReplyTo() <<
                    ", sub="<< message.getSubject()<<"]: " << ret.msg;

        const qpid::messaging::Address replyaddress = message.getReplyTo();

        if(!message.getSubject().empty())
            ret.msg["subject"] = message.getSubject();

        ret.replyFuction = boost::bind(&AgoQPIDImpl::sendReply, this, _1, replyaddress);

    } catch(const qpid::messaging::NoMessageAvailable& error) {

    } catch(const std::exception& error) {
        // TODO: XXX: Pass sshutdownsignaled somehow...
        // if(shutdownSignaled)
        //    break;

        AGO_ERROR() << "Exception in message loop: " << error.what();

        if (session.hasError()) {
            AGO_ERROR() << "Session has error, recreating";
            session.close();
            session = connection.createSession();
            receiver = session.createReceiver("agocontrol; {create: always, node: {type: topic}}");
            sender = session.createSender("agocontrol; {create: always, node: {type: topic}}");
        }

        usleep(50);
    }

    return ret;
}

void agocontrol::AgoQPIDImpl::sendReply(const Json::Value& content, const qpid::messaging::Address replyAddress)
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
