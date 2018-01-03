#ifndef AGOCLIENT_H
#define AGOCLIENT_H

#include <string>
#include <sstream>
#include <fstream>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <qpid/messaging/Connection.h>
#include <qpid/messaging/Message.h>
#include <qpid/messaging/Receiver.h>
#include <qpid/messaging/Sender.h>
#include <qpid/messaging/Session.h>
#include <qpid/messaging/Address.h>

#include <boost/filesystem.hpp>
#include <boost/function.hpp>

#include "agolog.h"
#include "agoconfig.h"
#include "agojson.h"
#include "agoproto.h"

#include <uuid/uuid.h>
namespace agocontrol {

    bool nameval(const std::string& in, std::string& name, std::string& value);

    /// string replace helper.
    void replaceString(std::string& subject, const std::string& search, const std::string& replace);

    // string split helper
    std::vector<std::string> split(const std::string &s, char delimiter);
    std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems);

    /// helper to generate a string containing a uuid.
    std::string generateUuid();

    /// helper for conversions
    std::string uint64ToString(uint64_t i);
    unsigned int stringToUint(const std::string& v);

    /// convert int to std::string.
    std::string int2str(int i);

    /// convert float to std::string.
    std::string float2str(float f);
    std::string double2str(double f);

    /// ago control client connection class.
    class AgoConnection {
    protected:
        qpid::messaging::Connection connection;
        qpid::messaging::Sender sender;
        qpid::messaging::Receiver receiver;
        qpid::messaging::Session session;
        qpid::types::Variant::Map deviceMap; // this holds the internal device list
        qpid::types::Variant::Map uuidMap; // this holds the permanent uuid to internal id mapping
        bool shutdownSignaled;
        bool storeUuidMap(); // stores the map on disk
        bool loadUuidMap(); // loads it
        boost::filesystem::path uuidMapFile;
        std::string instance;
        void reportDevices();
        bool filterCommands;
        boost::function< qpid::types::Variant::Map (qpid::types::Variant::Map) > commandHandler;
        boost::function< void (const std::string&, qpid::types::Variant::Map) > eventHandler;
        bool emitDeviceAnnounce(const std::string& internalId, const std::string& deviceType, const std::string& initialName);
        bool emitDeviceDiscover(const std::string& internalId, const std::string& deviceType);
        bool emitDeviceRemove(const std::string& internalId);
    public:
        AgoConnection(const std::string& interfacename);
        ~AgoConnection();
        void start();
        void run();
        void shutdown();
        bool addDevice(const std::string& internalId, const std::string& deviceType, const std::string& initialName = {});
        bool addDevice(const std::string& internalId, const std::string& deviceType, bool passuuid);
        bool removeDevice(const std::string& internalId);
        bool suspendDevice(const std::string& internalId);
        bool resumeDevice(const std::string& internalId);
        std::string getDeviceType(const std::string& internalId);
        int isDeviceStale(const std::string& internalId);
        bool emitDeviceStale(const std::string& uuid, const int stale);

        // C-style function pointers
        bool addHandler(qpid::types::Variant::Map (*handler)(qpid::types::Variant::Map));
        bool addEventHandler(void (*eventHandler)(const std::string&, qpid::types::Variant::Map));

        // C++-style function pointers, permits class references
        bool addHandler(boost::function<qpid::types::Variant::Map (qpid::types::Variant::Map)> handler);
        bool addEventHandler(boost::function<void (const std::string&, qpid::types::Variant::Map)> eventHandler);

        bool setFilter(bool filter);
        bool sendMessage(const std::string& subject, qpid::types::Variant::Map content);
        bool sendMessage(qpid::types::Variant::Map content);

        AgoResponse sendRequest(const qpid::types::Variant::Map& content);
        AgoResponse sendRequest(const std::string& subject, const qpid::types::Variant::Map& content);
        AgoResponse sendRequest(const std::string& subject, const qpid::types::Variant::Map& content, qpid::messaging::Duration timeout);

        bool emitEvent(const std::string& internalId, const std::string& eventType, const std::string& level, const std::string& units);
        bool emitEvent(const std::string& internalId, const std::string& eventType, double level, const std::string& units);
        bool emitEvent(const std::string& internalId, const std::string& eventType, int level, const std::string& units);
        bool emitEvent(const std::string& internalId, const std::string& eventType, qpid::types::Variant::Map content);
        qpid::types::Variant::Map getInventory();
        std::string getAgocontroller();
        bool setGlobalVariable(const std::string& variable, const qpid::types::Variant& value);
        std::string uuidToInternalId(const std::string& uuid); // lookup in map
        std::string internalIdToUuid(const std::string& internalId); // lookup in map
    };

}/* namespace agocontrol */

#endif
