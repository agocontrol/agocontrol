#ifndef AGOCLIENT_H
#define AGOCLIENT_H

#include <string>
#include <sstream>
#include <fstream>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <boost/filesystem.hpp>
#include <boost/function.hpp>
#include <json/json.h>

#include "agolog.h"
#include "agoconfig.h"
#include "agoproto.h"

#include <chrono>
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
    class AgoConnectionImpl;

    class AgoConnection {
    protected:
        std::unique_ptr<AgoConnectionImpl> impl;

        Json::Value deviceMap; // this holds the internal device list
        Json::Value uuidMap; // this holds the permanent uuid to internal id mapping
        bool shutdownSignaled;
        bool storeUuidMap(); // stores the map on disk
        bool loadUuidMap(); // loads it
        boost::filesystem::path uuidMapFile;
        std::string instance;
        void reportDevices();
        bool filterCommands;
        boost::function< Json::Value (const Json::Value&) > commandHandler;
        boost::function< void (const std::string&, const Json::Value&) > eventHandler;
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
        bool addHandler(Json::Value (*handler)(const Json::Value&));
        bool addEventHandler(void (*eventHandler)(const std::string&, const Json::Value&));

        // C++-style function pointers, permits class references
        bool addHandler(boost::function<Json::Value (const Json::Value&)> handler);
        bool addEventHandler(boost::function<void (const std::string&, const Json::Value&)> eventHandler);

        bool setFilter(bool filter);
        bool sendMessage(const std::string& subject, const Json::Value& content);
        bool sendMessage(const Json::Value& content);

        AgoResponse sendRequest(const Json::Value& content);
        AgoResponse sendRequest(const std::string& subject, const Json::Value& content);
        AgoResponse sendRequest(const std::string& subject, const Json::Value& content, std::chrono::milliseconds timeout);

        bool emitEvent(const std::string& internalId, const std::string& eventType, const std::string& level, const std::string& units);
        bool emitEvent(const std::string& internalId, const std::string& eventType, double level, const std::string& units);
        bool emitEvent(const std::string& internalId, const std::string& eventType, int level, const std::string& units);
        bool emitEvent(const std::string& internalId, const std::string& eventType, const Json::Value& content);
        Json::Value getInventory();
        std::string getAgocontroller();
        bool setGlobalVariable(const std::string& variable, const Json::Value& value);
        std::string uuidToInternalId(const std::string& uuid); // lookup in map
        std::string internalIdToUuid(const std::string& internalId); // lookup in map
    };

}/* namespace agocontrol */

#endif
