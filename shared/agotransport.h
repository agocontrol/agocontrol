#ifndef AGOCONNECTION_IMPL_H
#define AGOCONNECTION_IMPL_H

#include <string>
#include <chrono>
#include <json/json.h>
#include <boost/function.hpp>
#include "agoproto.h"

namespace agocontrol {
namespace transport {

class AgoTransportMessage {
public:
    /* Base message, shall hold at least a "content". */
    Json::Value message;

    /* If set, the message was identified as being replyable */
    boost::function<void(const Json::Value&)> replyFuction;
};

class AgoTransport {
public:
    virtual ~AgoTransport() {};

    virtual bool start() = 0;
    virtual void shutdown() { shutdownSignaled = true; }

    /**
     * Send a one-of message
     *
     * message shall hold "content" and optional "subject" fields.
     *
     * The passed message may be mutated, depending on implementation.
     */ 
    virtual bool sendMessage(Json::Value &message) = 0;

    /**
     * Send a message and wait for a reply.
     *
     * message shall hold "content" and optional "subject" fields.
     *
     * The passed message may be mutated, depending on implementation.
     */ 
    virtual AgoResponse sendRequest(Json::Value &message, std::chrono::milliseconds timeout) = 0;

    virtual AgoTransportMessage fetchMessage(std::chrono::milliseconds timeout) = 0;
protected:
    bool shutdownSignaled = false;
};

};

};

#endif //AGOCONNECTION_IMPL_H
