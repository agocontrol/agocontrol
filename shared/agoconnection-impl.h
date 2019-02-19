#ifndef AGOCONNECTION_IMPL_H
#define AGOCONNECTION_IMPL_H

#include <string>
#include <chrono>
#include <json/json.h>
#include <boost/function.hpp>
#include "agoproto.h"

namespace agocontrol {
class AgoConnectionMessage {
public:
    Json::Value msg;
    boost::function<void(const Json::Value&)> replyFuction;
};

class AgoConnectionImpl {
public:
    virtual ~AgoConnectionImpl() {};

    virtual bool start() = 0;

    virtual bool sendMessage(const Json::Value &message) = 0;

    virtual AgoResponse sendRequest(const Json::Value &message, std::chrono::milliseconds timeout) = 0;

    virtual AgoConnectionMessage fetchMessage(std::chrono::milliseconds timeout) = 0;

};

};

#endif //AGOCONNECTION_IMPL_H
