#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <qpid/messaging/Connection.h>
#include <qpid/messaging/Message.h>
#include <qpid/messaging/Receiver.h>
#include <qpid/messaging/Sender.h>
#include <qpid/messaging/Session.h>
#include <qpid/messaging/Address.h>

#include "agoapp.h"
using namespace agocontrol;

class AgoDrain : public AgoApp {
private:
    // qpid session and sender/receiver
    qpid::messaging::Receiver receiver;
    qpid::messaging::Sender sender;
    qpid::messaging::Session session;
    qpid::messaging::Connection *connection;

    // Override, we do not use a AgoConnection in drain
    bool setupAgoConnection(boost::unique_lock<boost::mutex> &lock) { return true; }
    void doShutdown() ;

    int appMain();

public:
    AGOAPP_CONSTRUCTOR(AgoDrain);
};


int AgoDrain::appMain() {
    std::string broker;

    qpid::types::Variant::Map connectionOptions;
    broker = getConfigSectionOption("system", "broker", "localhost:5672");
    connectionOptions["username"] = getConfigSectionOption("system", "username", "agocontrol");
    connectionOptions["password"] = getConfigSectionOption("system", "password", "letmein");

    connectionOptions["reconnect"] = "true";

    connection = new qpid::messaging::Connection(broker, connectionOptions);

    try {
        connection->open(); 
        session = connection->createSession(); 
        receiver = session.createReceiver("agocontrol; {create: always, node: {type: topic}}"); 
        sender = session.createSender("agocontrol; {create: always, node: {type: topic}}"); 
    } catch(const std::exception& error) {
        std::cerr << error.what() << std::endl;
        connection->close();
        printf("could not startup\n");
        return 1;
    }


    while (!isExitSignaled()) {
        try{
            qpid::messaging::Message message = receiver.fetch(qpid::messaging::Duration::SECOND * 3);
            std::cout << "Message(properties=" << message.getProperties() << ", content='" ;
            if (message.getContentType() == "amqp/map") {
                qpid::types::Variant::Map map;
                qpid::messaging::decode(message, map);
                std::cout << map;
            } else {
                std::cout << message.getContent();
            }
            std::cout << "')" << std::endl;
            session.acknowledge(message);

        } catch(const qpid::messaging::NoMessageAvailable& error) {

        } catch(const std::exception& error) {
            std::cerr << error.what() << std::endl;
            usleep(50);
        }
    }

    return 0;
}

void AgoDrain::doShutdown() {
    try {
        // Trigger fetch() abort
        connection->close();
    } catch(std::exception &e) {
    }
}

AGOAPP_ENTRY_POINT(AgoDrain);

