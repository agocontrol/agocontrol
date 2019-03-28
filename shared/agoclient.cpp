#include <string>

#include <boost/thread/mutex.hpp>
#include <boost/thread/locks.hpp>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sstream>
#include <assert.h>
#include <dlfcn.h>

#include <boost/bind.hpp>

#include "build_config.h"

#include "agoclient.h"
#include "agoutils.h"
#include "agojson.h"

#include "agotransport-qpid.h"
#include "agotransport-mqtt.h"

namespace fs = ::boost::filesystem;

bool agocontrol::nameval(const std::string& in, std::string& name, std::string& value) {
    std::string::size_type i = in.find("=");
    if (i == std::string::npos) {
        name = in;
        return false;
    } else {
        name = in.substr(0, i);
        if (i+1 < in.size()) {
            value = in.substr(i+1);
            return true;
        } else {
            return false;
        }
    }
}

void agocontrol::replaceString(std::string& subject, const std::string& search, const std::string& replace) {
    size_t pos = 0;
    while ((pos = subject.find(search, pos)) != std::string::npos) {
        subject.replace(pos, search.length(), replace);
        pos += replace.length();
    }
}

/**
 * Split specified string
 */
std::vector<std::string> &agocontrol::split(const std::string &s, char delim, std::vector<std::string> &elems) {
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}
std::vector<std::string> agocontrol::split(const std::string &s, char delimiter) {
    std::vector<std::string> elements;
    split(s, delimiter, elements);
    return elements;
}

std::string agocontrol::uint64ToString(uint64_t i) {
    std::stringstream tmp;
    tmp << i;
    return tmp.str();
}

unsigned int agocontrol::stringToUint(const std::string& v)
{
    unsigned int r;
    std::istringstream (v) >> r;
    return r;
}

template <typename T> static T* loadTransportLibrary(const std::string &libName) {
#ifdef __APPLE__
    std::string library_filename("lib" + libName + ".dylib");
#else
    std::string library_filename("lib" + libName + ".so");
#endif

    T* f = NULL;
    void *libref = dlopen(library_filename.c_str(), RTLD_NOW);
    if(!libref) {
        AGO_FATAL() << "Could not load " << library_filename << ": " << dlerror();
        return NULL;
    }

    //dlerror(); // reset
    f = (T*) dlsym(libref, "create_instance");
    if (!f) {
        AGO_FATAL() << "Failed to load factory function from " << library_filename << ": " << dlerror();
        return NULL;
    }

    // XXX: leaking libref, will never be able to free it.. but will need to be loaded until
    // we shutdown anyway.
    return f;
}

agocontrol::AgoConnection::AgoConnection(const std::string& interfacename)
    : shutdownSignaled(false)
    , instance(interfacename)
{
    // TODO: Move to AgoApp
    ::agocontrol::log::log_container::initDefault();

    ConfigNameList cfgfiles = ConfigNameList(interfacename)
        .add("system");

    initTransport(cfgfiles);

    filterCommands = true; // only pass commands for child devices to handler by default
    uuidMap = Json::Value(Json::ValueType::objectValue);

    uuidMapFile = getConfigPath("uuidmap");
    uuidMapFile /= (interfacename + ".json");
    AGO_DEBUG() << "Using uuidMapFile " << uuidMapFile;
    try {
        ensureParentDirExists(uuidMapFile);
    } catch(const std::exception& error) {
        // exception msg has already been logged
        AGO_FATAL()
            << "Couldn't use configuration dir path: "
            << error.what();
        _exit(1);
    }

    loadUuidMap();

    // Must call start() to actually connect
}

bool agocontrol::AgoConnection::start() {
    return transport->start();
}

agocontrol::AgoConnection::~AgoConnection() {
}

void agocontrol::AgoConnection::initTransport(ConfigNameList &cfgfiles) {
    if (getConfigSectionOption("system", "messaging", "", cfgfiles) == "mqtt") {
        auto *f = loadTransportLibrary<agotransport_mqtt_factory>("agotransport-mqtt");
        if(!f) {
            _exit(1);
        }

        std::string broker = getConfigSectionOption("system", "broker", "localhost:1883", cfgfiles);
        agocontrol::transport::AgoTransport* trp = f(instance.c_str(),
                                                     broker.c_str(),
                                                     getConfigSectionOption("system", "username", "agocontrol", cfgfiles).c_str(),
                                                     getConfigSectionOption("system", "password", "letmein", cfgfiles).c_str());

        transport.reset(trp);
    } else {
        auto *f = loadTransportLibrary<agotransport_qpid_factory>("agotransport-qpid");
        if(!f) {
            _exit(1);
        }

        std::string broker = getConfigSectionOption("system", "broker", "localhost:5672", cfgfiles);
        agocontrol::transport::AgoTransport* trp = f(broker.c_str(),
                                                     getConfigSectionOption("system", "username", "agocontrol", cfgfiles).c_str(),
                                                     getConfigSectionOption("system", "password", "letmein", cfgfiles).c_str());

        transport.reset(trp);
    }
}

void agocontrol::AgoConnection::run() {

    while( !shutdownSignaled ) {
        agocontrol::transport::AgoTransportMessage m = transport->fetchMessage(std::chrono::seconds(3));
        if(shutdownSignaled)
            break;

        if(m.message.isNull()) {
            // Assume fetchMessage slept a while
            continue;
        }

        if (!m.message.isObject() || !m.message.isMember("content")) {
            AGO_ERROR() << "Invalid message: " << m.message;
            continue;
        }

        Json::Value& content(m.message["content"]);

        if (content.isMember("command") && content["command"] == "discover")
        {
            reportDevices(); // make resolver happy and announce devices on discover request
        }
        else
        {
            if (!m.message.isMember("subject")) {
                // no subject, this is a command
                std::string internalid = uuidToInternalId(content["uuid"].asString());
                // lets see if this is for one of our devices
                bool isOurDevice = (internalid.size() > 0) && (deviceMap.isMember(internalIdToUuid(internalid)));
                //  only handle if a command handler is set. In addition it needs to be one of our device when the filter is enabled
                if ( ( isOurDevice || (!(filterCommands))) && !commandHandler.empty()) {

                    // printf("command for id %s found, calling handler\n", internalid.c_str());
                    if (!internalid.empty())
                        content["internalid"] = internalid;

                    // found a match, reply to sender and pass the command to the assigned handler method
                    Json::Value commandResponse;
                    try {
                        commandResponse = commandHandler(content);

                        // Catch any non-updated applications HARD.
                        if(!commandResponse.empty() && !commandResponse.isMember("result") && !commandResponse.isMember("error")) {
                            AGO_ERROR() << "Application " << instance << " has not been updated properly and command handler returns non-valid responses.";
                            AGO_ERROR() << "Input: " << content;
                            AGO_ERROR() << "Output: " << content;
                            commandResponse = responseError(RESPONSE_ERR_INTERNAL,
                                    "Component "+instance+" has not been updated properly, please contact developers with logs");
                        }
                    }catch(const AgoCommandException& ex) {
                        commandResponse = ex.toResponse();
                    }catch(const std::exception &ex) {
                        AGO_ERROR() << "Unhandled exception in command handler:" << ex.what();
                        commandResponse = responseError(RESPONSE_ERR_INTERNAL, "Unhandled exception in command handler");
                    }


                    // only send a reply if this was for one of our childs
                    // or if it was the special command inventory when the filterCommands was false, that's used by the resolver
                    // to reply to "anonymous" requests not destined to any specific uuid
                    if (isOurDevice || (content["command"]=="inventory" && filterCommands==false)) {
                        if(m.replyFuction.empty())
                            AGO_WARNING() << "Attempted to send a reply to a incoming message which did not expect a reply: " << content;
                        else
                            m.replyFuction(commandResponse);
                    }
                }
            } else if (!eventHandler.empty()) {
                std::string subject = m.message["subject"].asString();

                eventHandler(subject, content);
            }
        }
    }
    AGO_TRACE() << "Leaving run() message loop";
}

void agocontrol::AgoConnection::shutdown() {
    if(shutdownSignaled) return;
    shutdownSignaled = true;
    transport->shutdown();

}

/**
 * Report device has been discovered
 */
bool agocontrol::AgoConnection::emitDeviceDiscover(const std::string& internalId, const std::string& deviceType)
{
    Json::Value content;
    content["devicetype"] = deviceType;
    content["internalid"] = internalId;
    content["handled-by"] = instance;
    content["uuid"] = internalIdToUuid(internalId);
    return sendMessage("event.device.discover", content);
}

bool agocontrol::AgoConnection::emitDeviceAnnounce(const std::string& internalId, const std::string& deviceType, const std::string& initialName) {
    Json::Value content;

    content["devicetype"] = deviceType;
    content["internalid"] = internalId;
    content["handled-by"] = instance;
    content["uuid"] = internalIdToUuid(internalId);

    if(!initialName.empty())
        content["initial_name"] = initialName;

    return sendMessage("event.device.announce", content);
}

/**
 * Emit stale state
 */
bool agocontrol::AgoConnection::emitDeviceStale(const std::string& uuid, const int stale)
{
    Json::Value content;

    //content["internalid"] = internalId;
    content["stale"] = stale;
    content["uuid"] = uuid;

    return sendMessage("event.device.stale", content);
}

bool agocontrol::AgoConnection::emitDeviceRemove(const std::string& internalId) {
    Json::Value content;
    content["uuid"] = internalIdToUuid(internalId);
    return sendMessage("event.device.remove", content);
}

bool agocontrol::AgoConnection::addDevice(const std::string& internalId, const std::string& deviceType, bool passuuid) {
    if (!passuuid) return addDevice(internalId, deviceType);
    uuidMap[internalId] = internalId;
    storeUuidMap();
    return addDevice(internalId, deviceType);

}

bool agocontrol::AgoConnection::addDevice(const std::string& internalId, const std::string& deviceType, const std::string& initialName) {
    if (internalIdToUuid(internalId).size()==0) {
        // need to generate new uuid
        uuidMap[agocontrol::utils::generateUuid()] = internalId;
        storeUuidMap();
    }
    Json::Value device(Json::ValueType::objectValue);
    device["devicetype"] = deviceType;
    device["internalid"] = internalId;
    device["stale"] = 0;

    // XXX: This is not read by agoresolver currently
    Json::Value parameters(Json::ValueType::objectValue); //specific parameters map
    device["parameters"] = parameters;

    deviceMap[internalIdToUuid(internalId)] = device;
    emitDeviceAnnounce(internalId, deviceType, initialName);
    return true;
}

bool agocontrol::AgoConnection::removeDevice(const std::string& internalId) {
    const std::string &uuid = internalIdToUuid(internalId);
    if (uuid.size() != 0) {
        emitDeviceRemove(internalId);
        deviceMap.removeMember(uuid);
        // deviceMap[internalIdToUuid(internalId)] = device;
        return true;
    } else return false;
}

/**
 * Suspend device (set stale flag)
 */
bool agocontrol::AgoConnection::suspendDevice(const std::string& internalId)
{
    const std::string &uuid = internalIdToUuid(internalId);
    if( uuid.length() > 0 && deviceMap.isMember(uuid))
    {
        deviceMap[uuid]["stale"] = 1;
        return emitDeviceStale(uuid, 1);
    }
    return false;
}

/**
 * Resume device (reset stale flag)
 */
bool agocontrol::AgoConnection::resumeDevice(const std::string& internalId)
{
    std::string uuid = internalIdToUuid(internalId);
    if(uuid.length() > 0 && deviceMap.isMember(uuid))
    {
        deviceMap[uuid]["stale"] = 0;
        return emitDeviceStale(uuid, 0);
    }
    return false;
}

std::string agocontrol::AgoConnection::uuidToInternalId(const std::string& uuid) {
    if(uuidMap.isMember(uuid))
        return uuidMap[uuid].asString();
    return std::string();
}

std::string agocontrol::AgoConnection::internalIdToUuid(const std::string& internalId) {
    for (auto it = uuidMap.begin(); it != uuidMap.end(); ++it) {
        if (it->asString() == internalId)
            return it.name();
    }
    return std::string();
}

/**
 * Report controller devices after discover request from agoresolver
 */
void agocontrol::AgoConnection::reportDevices()
{
    for (auto it = deviceMap.begin(); it != deviceMap.end(); ++it)
    {
        Json::Value& device(*it);
        // do not announce stale devices
        if( device["stale"] == 0 )
        {
            emitDeviceDiscover(device["internalid"].asString(), device["devicetype"].asString());
        }
    }
}

bool agocontrol::AgoConnection::storeUuidMap() {
    writeJsonFile(uuidMap, uuidMapFile);
    return true;
}

bool agocontrol::AgoConnection::loadUuidMap() {
    boost::system::error_code ignore;
    if(!fs::exists(uuidMapFile, ignore) || ignore)
        return false;

    if(readJsonFile(uuidMap, uuidMapFile)) {
        if (uuidMap.type() == Json::ValueType::objectValue)
            return true;

        AGO_ERROR() << "Invalid contents in " << uuidMapFile;
    }

    return false;
}

bool agocontrol::AgoConnection::addHandler(Json::Value (*handler)(const Json::Value&)) {
    addHandler(boost::bind(handler, _1));
    return true;
}

bool agocontrol::AgoConnection::addHandler(boost::function<Json::Value (const Json::Value&)> handler)
{
    commandHandler = handler;
    return true;
}

bool agocontrol::AgoConnection::addEventHandler(void (*handler)(const std::string&, const Json::Value&)) {
    addEventHandler(boost::bind(handler, _1, _2));
    return true;
}

bool agocontrol::AgoConnection::addEventHandler(boost::function<void (const std::string&, const Json::Value&)> handler)
{
    eventHandler = handler;
    return true;
}


bool agocontrol::AgoConnection::sendMessage(const std::string& subject, const Json::Value& content) {
    Json::Value message;
    message["content"] = content;
    if(!subject.empty())
        message["subject"] = subject;

    transport->sendMessage(message);
    return true;
}

agocontrol::AgoResponse agocontrol::AgoConnection::sendRequest(const Json::Value& content) {
    return sendRequest("", content, std::chrono::seconds(3));
}

agocontrol::AgoResponse agocontrol::AgoConnection::sendRequest(const std::string& subject, const Json::Value& content) {
    return sendRequest("", content, std::chrono::seconds(3));
}

agocontrol::AgoResponse agocontrol::AgoConnection::sendRequest(const std::string& subject, const Json::Value& content, std::chrono::milliseconds timeout) {
    Json::Value message;
    message["content"] = content;
    if(!subject.empty())
        message["subject"] = subject;
    return transport->sendRequest(message, timeout);
}

bool agocontrol::AgoConnection::sendMessage(const Json::Value& content) {
    return sendMessage("",content);
}

bool agocontrol::AgoConnection::emitEvent(const std::string& internalId, const std::string& eventType, const std::string& level, const std::string& unit) {
    Json::Value content;
    content["level"] = parseToJson(level);
    content["unit"] = unit;
    content["uuid"] = internalIdToUuid(internalId);
    return sendMessage(eventType, content);
}
bool agocontrol::AgoConnection::emitEvent(const std::string& internalId, const std::string& eventType, double level, const std::string& unit) {
    Json::Value content;
    content["level"] = level;
    content["unit"] = unit;
    content["uuid"] = internalIdToUuid(internalId);
    return sendMessage(eventType, content);
}
bool agocontrol::AgoConnection::emitEvent(const std::string& internalId, const std::string& eventType, int level, const std::string& unit) {
    Json::Value content;
    content["level"] = level;
    content["unit"] = unit;
    content["uuid"] = internalIdToUuid(internalId);
    return sendMessage(eventType, content);
}

bool agocontrol::AgoConnection::emitEvent(const std::string& internalId, const std::string& eventType, const Json::Value& _content) {
    Json::Value content;
    content = _content;
    content["uuid"] = internalIdToUuid(internalId);
    return sendMessage(eventType, content);
}

std::string agocontrol::AgoConnection::getDeviceType(const std::string& internalId) {
    std::string uuid = internalIdToUuid(internalId);
    if (uuid.size() > 0 && deviceMap.isMember(uuid)) {
        Json::Value device = deviceMap[uuid];
        return device["devicetype"].asString();
    }
    return std::string();
}

/**
 * Return device stale state
 */
int agocontrol::AgoConnection::isDeviceStale(const std::string& internalId)
{
    std::string uuid = internalIdToUuid(internalId);
    if (uuid.size() > 0)
    {
        if(deviceMap.isMember(uuid))
        {
            Json::Value device = deviceMap[uuid];
            return device["stale"].asInt();
        }
        else
        {
            AGO_WARNING() << "internalid '" << internalId << "' doesn't exist in deviceMap";
            return 0;
        }
    }
    else
    {
        return 0;
    }
}

bool agocontrol::AgoConnection::setFilter(bool filter) {
    filterCommands = filter;
    return filterCommands;
}

Json::Value agocontrol::AgoConnection::getInventory() {
    Json::Value content;
    content["command"] = "inventory";
    AgoResponse r = sendRequest(content);

    if(r.isOk()) {
        AGO_TRACE() << "Inventory obtained";
        return r.getData();
    }else{
        AGO_WARNING() << "Failed to obtain inventory: " << r.response;
    }

    // TODO: Some way to report error?
    return Json::Value();
}

std::string agocontrol::AgoConnection::getAgocontroller() {
    std::string agocontroller;
    int retry = 10;
    // TODO: CACHE
    while(agocontroller.empty() && retry-- > 0) {
        Json::Value inventory = getInventory();
        if (inventory.isMember("devices")) {
            Json::Value& devices(inventory["devices"]);
            for (auto it = devices.begin(); it != devices.end(); it++) {
                if ((*it)["devicetype"] == "agocontroller") {
                    AGO_DEBUG() << "Found Agocontroller: " << it.name();
                    agocontroller = it.name();
                }
            }
        }

        if (agocontroller == "" && retry) {
            AGO_WARNING() << "Unable to resolve agocontroller, retrying";
            sleep(1);
        }
    }

    if (agocontroller == "")
        AGO_WARNING() << "Failed to resolve agocontroller, giving up";

    return agocontroller;
}

bool agocontrol::AgoConnection::setGlobalVariable(const std::string& variable, const Json::Value& value) {
    Json::Value setvariable;
    std::string agocontroller = getAgocontroller();
    if (agocontroller != "") {
        setvariable["uuid"] = agocontroller;
        setvariable["command"] = "setvariable";
        setvariable["variable"] = variable;
        setvariable["value"] = value;
        return sendMessage("", setvariable);
    }
    return false;
}

