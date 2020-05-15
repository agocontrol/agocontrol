#ifndef AGOCONNECTION_QPID_H
#define AGOCONNECTION_QPID_H

#include "agotransport.h"

namespace agocontrol {
namespace transport {

class QpidImpl;

class AgoQpidTransport : public AgoTransport {
public:
    AgoQpidTransport(const std::string &uri, const std::string &user, const std::string &password);
    ~AgoQpidTransport();

    bool start();

    bool sendMessage(Json::Value &message);
    AgoResponse sendRequest(Json::Value &message, std::chrono::milliseconds timeout);
    AgoTransportMessage fetchMessage(std::chrono::milliseconds timeout);

private:
    std::unique_ptr<QpidImpl> impl;
};


};
};

/**
 * Definition matching the "create_instance" in agotransport-qpid.cpp, for creating via dlsym.
 */
typedef agocontrol::transport::AgoTransport* agotransport_qpid_factory(const char *uri, const char *user, const char *password);

#endif //AGOCONNECTION_QPID_H
