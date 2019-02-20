/*
   Copyright (C) 2012 Harald Klein <hari@vt100.at>

   This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

   See the GNU General Public License for more details.

   this is the core resolver component for ago control
   */

#include <iostream>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include <termios.h>
#include <errno.h>
#include <stdlib.h>
#if !defined(__FreeBSD__) && !defined(__APPLE__)
#include <sys/sysinfo.h>
#endif

#include <sstream>
#include <map>
#include <deque>

#include <uuid/uuid.h>
#include <boost/asio/deadline_timer.hpp>

#include "agoapp.h"

#include "build_config.h"

#ifndef SCHEMADIR
#define SCHEMADIR "schema.d"
#endif

#ifndef INVENTORYDBFILE
#define INVENTORYDBFILE "db/inventory.db"
#endif

#ifndef VARIABLESMAPFILE
#define VARIABLESMAPFILE "maps/variablesmap.json"
#endif

#ifndef DEVICESMAPFILE
#define DEVICESMAPFILE "maps/devices.json"
#endif

#ifndef DEVICEPARAMETERSMAPFILE
#define DEVICEPARAMETERSMAPFILE "maps/deviceparameters.json"
#endif

#include "schema.h"
#include "inventory.h"

using namespace agocontrol;
namespace fs = ::boost::filesystem;
namespace pt = ::boost::posix_time;

class AgoResolver: public AgoApp {
private:
    Json::Value inventory; // used to hold device registrations
    Json::Value schema;
    Json::Value systeminfo; // holds system information
    Json::Value variables; // holds global variables
    Json::Value environment; // holds global environment like position, weather conditions, ..
    Json::Value deviceparameters; // holds device parameters

    std::unique_ptr<Inventory> inv;
    unsigned int discoverdelay;
    bool persistence;

    bool saveDevicemap();
    void loadDevicemap();
    bool saveDeviceParametersMap();
    void loadDeviceParametersMap();
    void get_sysinfo();
    bool emitNameEvent(const std::string& uuid, const std::string& eventType, const std::string& name);
    bool emitFloorplanEvent(const std::string& uuid, const std::string& eventType, const std::string& floorplan, int x, int y);
    void handleEvent(Json::Value& device, const std::string& subject, const Json::Value& content);
    Json::Value getDefaultParameters();

    void scanSchemaDir(const fs::path &schemaPrefix) ;

    Json::Value commandHandler(const Json::Value& content) ;
    void eventHandler(const std::string& subject , const Json::Value& content) ;

    boost::asio::deadline_timer discoveryTimer;
    void discoverFunction(const boost::system::error_code& error) ;

    boost::asio::deadline_timer staleTimer;
    void staleFunction(const boost::system::error_code& error);

    void setupApp();
    void doShutdown();
public:
    AGOAPP_CONSTRUCTOR_HEAD(AgoResolver)
        , inventory(Json::objectValue)
        , schema(Json::objectValue)
        , systeminfo(Json::objectValue)
        , variables(Json::objectValue)
        , environment(Json::objectValue)
        , deviceparameters(Json::objectValue)
        , discoveryTimer(ioService())
        , staleTimer(ioService()) {}

};

/**
 * Save device map (only if persistence option activated (default not))
 */
bool AgoResolver::saveDevicemap()
{
    if (persistence)
    {
        AGO_TRACE() << "Saving device-map";
        return writeJsonFile(inventory, getConfigPath(DEVICESMAPFILE));
    }
    return true;
}

/**
 * Load device map
 */
void AgoResolver::loadDevicemap()
{
    readJsonFile(inventory, getConfigPath(DEVICESMAPFILE));
    AGO_TRACE() << "Inventory: " << inventory;
}

/**
 * Save device parameters map
 */
bool AgoResolver::saveDeviceParametersMap()
{
    AGO_TRACE() << "Saving device parameters map";
    return writeJsonFile(deviceparameters, getConfigPath(DEVICEPARAMETERSMAPFILE));
}

/**
 * Load device parameters map
 */
void AgoResolver::loadDeviceParametersMap()
{
    //first of all load file
    readJsonFile(deviceparameters, getConfigPath(DEVICEPARAMETERSMAPFILE));

    //then synchronize inventory with parameters
    /*bool save = false;
    for( auto it = inventory.begin(); it!=inventory.end(); it++ )
    {
        if( !it->isNull() )
        {
            std::string uuid = it.name();
            if( deviceparameters.isMember(uuid) )
            {
                Json::Value& device = *it;
                device["parameters"] = deviceparameters[uuid];
            }
            else
            {
                //device has no parameters yet, add structure with default parameters
                AGO_DEBUG() << "No parameters for device " << uuid;
                deviceparameters[uuid] = getDefaultParameters();
                save = true;
            }
        }
    }
    if( save )
    {
        AGO_DEBUG() << "save device parameters map";
        saveDeviceParametersMap();
    }*/
    AGO_DEBUG() << deviceparameters;
}

/**
 * Return default parameters map
 */
Json::Value AgoResolver::getDefaultParameters()
{
    Json::Value params;
    params["staleTimeout"] = (uint64_t)0;
    return params;
}

void AgoResolver::get_sysinfo() {
#if !defined(__FreeBSD__) && !defined(__APPLE__)
    /* Note on FreeBSD exclusion. Sysinfo.h does not exist, but the code below
     * does not really use it anyway.. so just skip it.
     */
    struct sysinfo s_info;
    int error;
    error = sysinfo(&s_info);
    if(error != 0) {
        AGO_ERROR() << "sysinfo error: " << error;
    } else { /*
                systeminfo["uptime"] = s_info.uptime;
                systeminfo["loadavg1"] = s_info.loads[0];
                systeminfo["loadavg5"] = s_info.loads[1];
                systeminfo["loadavg15"] = s_info.loads[2];
                systeminfo["totalram"] = s_info.totalram;
                systeminfo["freeram"] = s_info.freeram;
                systeminfo["procs"] = s_info.procs;
                */
    }
#endif
}

bool AgoResolver::emitNameEvent(const std::string& uuid, const std::string& eventType, const std::string& name)
{
    Json::Value content;
    content["name"] = name;
    content["uuid"] = uuid;
    return agoConnection->sendMessage(eventType, content);
}

bool AgoResolver::emitFloorplanEvent(const std::string& uuid, const std::string& eventType, const std::string& floorplan, int x, int y) {
    Json::Value content;
    content["uuid"] = uuid;
    content["floorplan"] = floorplan;
    content["x"] = x;
    content["y"] = y;
    return agoConnection->sendMessage(eventType, content);
}
#if 0
std::string valuesToString(Json::Value *values) {
    std::string result;
    for (auto it = values->begin(); it != values->end(); ++it) {
        result += (*it).asString();
        if ((it != values->end()) && (next(it) != values->end()))
            result += "/";
    }
    return result;
}
#endif

// handles events that update the state or values of a device
void AgoResolver::handleEvent(Json::Value& device, const std::string& subject, const Json::Value& content)
{
    bool save = false;

    //check if device is valid
    if (!device.isMember("values"))
    {
        AGO_ERROR() << "device[values] is empty in handleEvent()";
        return;
    }

    Json::Value& values(device["values"]);

    if ((subject == "event.device.statechanged") || (subject == "event.security.sensortriggered"))
    {
        values["state"] = content["level"];
        device["state"] = content["level"];
        // (*device)["state"] = valuesToString(values);
        save = true;
    }
    else if (subject == "event.environment.positionchanged")
    {
        Json::Value value;
        std::stringstream timestamp;

        value["unit"] = content["unit"];
        value["latitude"] = content["latitude"];
        value["longitude"] = content["longitude"];

        timestamp << time(NULL);
        value["timestamp"] = timestamp.str();

        values["position"] = value;
        save = true;

    }
    else if ( ((subject.find("event.environment.")!=std::string::npos) && (subject.find("changed")!=std::string::npos))
            || (subject=="event.device.batterylevelchanged") )
    {
        Json::Value value;
        std::stringstream timestamp;
        std::string quantity = subject;

        //remove useless part of event (changed, event, environment ...)
        if( subject.find("event.environment.")!=std::string::npos )
        {
            //event.environment.XXXchanged
            replaceString(quantity, "event.environment.", "");
            replaceString(quantity, "changed", "");
        }
        else if( subject=="event.device.batterylevelchanged" )
        {
            replaceString(quantity, "event.device.", "");
            replaceString(quantity, "changed", "");
        }

        value["unit"] = content["unit"];
        value["level"] = content["level"];

        timestamp << time(NULL);
        value["timestamp"] = timestamp.str();

        values[quantity] = value;
        save = true;
    }

    //update lastseen
    device["lastseen"] = (uint64_t)time(NULL);

    //save devicemap if necessary
    if( save )
    {
        saveDevicemap();
    }
}

Json::Value AgoResolver::commandHandler(const Json::Value& content)
{
    std::string internalid = content["internalid"].asString();
    Json::Value responseData;

    if (internalid == "agocontroller")
    {
        if (content["command"] == "setroomname")
        {
            std::string roomUuid = content["room"].asString();
            // if no uuid is provided, we need to generate one for a new room
            if (roomUuid == "") roomUuid = generateUuid();
            if (inv->setRoomName(roomUuid, content["name"].asString()))
            {
                // return room UUID
                responseData["uuid"] = roomUuid;
                emitNameEvent(roomUuid, "event.system.roomnamechanged", content["name"].asString());
                return responseSuccess(responseData);
            }
            else
            {
                return responseFailed("Failed to store change");
            }
        }
        else if (content["command"] == "setdeviceroom")
        {
            checkMsgParameter(content, "device", Json::stringValue);
            checkMsgParameter(content, "room", Json::stringValue, true);

            if (inv->setDeviceRoom(content["device"].asString(), content["room"].asString()))
            {
                // update room in local device map
                std::string uuid = content["device"].asString();
                std::string room = inv->getDeviceRoom(uuid);

                if (inventory.isMember(uuid))
                {
                    inventory[uuid]["room"]= room;
                }

                return responseSuccess();
            }
            else
            {
                return responseFailed("Failed to store change");
            }
        }
        else if (content["command"] == "setdevicename")
        {
            checkMsgParameter(content, "device", Json::stringValue, true);

            std::string uuid = content["device"].asString();
            std::string name = content["name"].asString();
            if(name == "") {
                inv->deleteDevice(uuid);
                emitNameEvent(uuid, "event.system.devicenamechanged", "");
                return responseSuccess();
            }else if (inv->setDeviceName(uuid, name))
            {
                // update name in local device map
                name = inv->getDeviceName(uuid);
                if (inventory.isMember(uuid))
                {
                    inventory[uuid]["name"]= name;
                }
                saveDevicemap();
                emitNameEvent(uuid, "event.system.devicenamechanged", name);

                return responseSuccess();
            }
            else
            {
                return responseFailed("Failed to store change");
            }
        }
        else if( content["command"]=="deletedevice" )
        {
            // Compound command which removes device from inventory map and deletes from DB
            checkMsgParameter(content, "device", Json::stringValue); //device uuid
            std::string uuid = content["device"].asString();

            AGO_INFO() << "removing device: uuid=" << uuid;
            Json::Value ignored;
            if (inventory.removeMember(uuid, &ignored))
            {
                saveDevicemap();
            }

            inv->deleteDevice(uuid);
            // TODO: Some event?
            return responseSuccess("Device removed");
        }
        else if (content["command"] == "deleteroom")
        {
            checkMsgParameter(content, "room", Json::stringValue);
            std::string uuid = content["room"].asString();
            if (inv->deleteRoom(uuid))
            {
                emitNameEvent(uuid, "event.system.roomdeleted", "");
                return responseSuccess();
            }
            else
            {
                return responseFailed("Failed to delete room");
            }
        }
        else if (content["command"] == "setfloorplanname")
        {
            std::string uuid = content["floorplan"].asString();
            // if no uuid is provided, we need to generate one for a new floorplan
            if (uuid == "")
            {
                uuid = generateUuid();
            }

            if (inv->setFloorplanName(uuid, content["name"].asString()))
            {
                emitNameEvent(content["floorplan"].asString(), "event.system.floorplannamechanged", content["name"].asString());
                responseData["uuid"] = uuid;
                return responseSuccess(responseData);
            }
            else
            {
                return responseFailed("Failed to store change");
            }
        }
        else if (content["command"] == "setdevicefloorplan")
        {
            checkMsgParameter(content, "device", Json::stringValue);
            checkMsgParameter(content, "floorplan", Json::stringValue);
            checkMsgParameter(content, "x", Json::intValue);
            checkMsgParameter(content, "y", Json::intValue);

            if (inv->setDeviceFloorplan(content["device"].asString(), content["floorplan"].asString(), content["x"].asInt(), content["y"].asInt()))
            {
                emitFloorplanEvent(content["device"].asString(),
                        "event.system.floorplandevicechanged",
                        content["floorplan"].asString(),
                        content["x"].asInt(),
                        content["y"].asInt());
                return responseSuccess();
            }
            else
            {
                return responseFailed("Failed to store floorplan changes");
            }
        }
        else if( content["command"]=="deldevicefloorplan" )
        {
            checkMsgParameter(content, "device", Json::stringValue);
            checkMsgParameter(content, "floorplan", Json::stringValue);
            inv->delDeviceFloorplan(content["device"].asString(), content["floorplan"].asString());

            return responseSuccess();
        }
        else if (content["command"] == "deletefloorplan")
        {
            checkMsgParameter(content, "floorplan", Json::stringValue);

            if (inv->deleteFloorplan(content["floorplan"].asString()))
            {
                emitNameEvent(content["floorplan"].asString(), "event.system.floorplandeleted", "");
                return responseSuccess();
            }
            else
            {
                return responseFailed("Failed to store change");
            }
        }
        else if (content["command"] == "setvariable")
        {
            checkMsgParameter(content, "variable", Json::stringValue);
            checkMsgParameter(content, "value");

            variables[content["variable"].asString()] = content["value"].asString();
            if (writeJsonFile(variables, getConfigPath(VARIABLESMAPFILE)))
            {
                return responseSuccess();
            }
            else
            {
                return responseFailed("Failed to store change");
            }
        }
        else if (content["command"] == "delvariable")
        {
            checkMsgParameter(content, "variable", Json::stringValue);

            Json::Value ignored;
            if(variables.removeMember(content["variable"].asString(), &ignored))
            {
                if (!writeJsonFile(variables, getConfigPath(VARIABLESMAPFILE)))
                {
                    return responseFailed("Failed to store change");
                }
            }

            return responseSuccess();
        }
        else if (content["command"] == "getdevice")
        {
            checkMsgParameter(content, "device", Json::stringValue);

            std::string uuid = content["device"].asString();
            if (inventory.isMember(uuid))
            {
                responseData["device"] = inventory[uuid];
                return responseSuccess(responseData);
            }
            else
            {
                return responseError(RESPONSE_ERR_NOT_FOUND, "Device does not exist in inventory");
            }
        }
        else if (content["command"] == "getconfigtree")
        {
            responseData["config"] = getConfigTree();
            return responseSuccess(responseData);
        }
        else if (content["command"] == "setconfig")
        {
            // XXX: No access checks at all... may overwrite whatever
            checkMsgParameter(content, "section", Json::stringValue);
            checkMsgParameter(content, "option", Json::stringValue);
            checkMsgParameter(content, "value");
            checkMsgParameter(content, "app", Json::stringValue);

            if (setConfigSectionOption(content["section"].asString(),
                        content["option"].asString(),
                        content["value"].asString(),
                        content["app"].asString()))
            {
                AGO_INFO() << "Changed config option by request:"
                    << " section = " << content["section"].asString()
                    << " option = " << content["option"].asString()
                    << " value = " << content["value"].asString()
                    << " app = " << content["app"].asString();
                return responseSuccess();
            }
            else
            {
                return responseFailed("Failed to write config parameter");
            }
        }
        else if( content["command"] == "getconfig" )
        {
            checkMsgParameter(content, "section", Json::stringValue);
            checkMsgParameter(content, "option", Json::stringValue);
            checkMsgParameter(content, "app", Json::stringValue);
            std::string value = getConfigSectionOption(content["section"].asString(), content["option"].asString(),
                                                       "", content["app"].asString());
            Json::Value response;
            response["value"] = value;
            return responseSuccess(response);
        }
        else if( content["command"]=="setdeviceparameters" )
        {
            //set specific device parameters
            checkMsgParameter(content, "device", Json::stringValue); //device uuid
            checkMsgParameter(content, "parameters", Json::objectValue);

            std::string uuid = content["device"].asString();
            Json::Value parameters = content["parameters"];

            //update both inventory...
            if(inventory.isMember(uuid)) {
                Json::Value& device(inventory[uuid]);

                device["parameters"] = parameters;
                if( !saveDevicemap() )
                {
                    return responseFailed("Failed to write config parameter (device map)");
                }
                else
                {
                    AGO_DEBUG() << "device map saved";
                }
            }
            else
            {
                //trying to update non existing device
                AGO_WARNING() << "Unable to set parameters of non existing device " << uuid;
            }

            //...and parameters map
            if( deviceparameters.isMember(uuid) )
            {
                //update parameters
                deviceparameters[uuid] = parameters;
                if( !saveDeviceParametersMap() )
                {
                    return responseFailed("Failed to write config parameter (parameters map)");
                }
                else
                {
                    AGO_DEBUG() << "device parameters map saved";
                }
            }
            else
            {
                AGO_DEBUG() << "not found";
            }
            AGO_DEBUG() << deviceparameters;

            return responseSuccess("Parameters saved");
        }


        return responseUnknownCommand();
    }
    else
    {
        // Global handler for "inventory" command
        if (content["command"] == "inventory")
        {
            responseData["devices"] = inventory;
            responseData["schema"] = schema;
            responseData["rooms"] = inv->getRooms();
            responseData["floorplans"] = inv->getFloorplans();
            get_sysinfo();
            responseData["system"] = systeminfo;
            responseData["variables"] = variables;
            responseData["environment"] = environment;

            return responseSuccess(responseData);
        }
        else
        {
            // XXX: Fix filtering instead so we are not called at all...
            // This is dropped in aogclient loop unless command is "inventory"..
            return responseUnknownCommand();
        }
    }

    // We have no devices registered but our own; if we get here something
    // is broken internally
    throw std::logic_error("Should not go here");
}

void AgoResolver::eventHandler(const std::string& subject, const Json::Value& content)
{
    if( subject=="event.device.announce" || subject=="event.device.discover" )
    {
        std::string uuid = content["uuid"].asString();
        if (uuid != "")
        {
            bool wasKnown = inv->isDeviceRegistered(uuid);

            // AGO_TRACE() << "preparing device: uuid=" << uuid;
            if (!inventory.isMember(uuid)) {
                // device is newly announced, set default state and values
                Json::Value device;
                device["lastseen"] = (uint64_t)time(NULL);
                device["state"] = 0;
                device["values"] = Json::Value(Json::objectValue);
                device["stale"] = 0;

                if( deviceparameters.isMember(uuid) )
                {
                    //load device parameters from map
                    AGO_DEBUG() << "load device params from map";
                    device["parameters"] = deviceparameters[uuid];
                }
                else
                {
                    //no device parameters, add default one
                    AGO_DEBUG() << "load device params from empty";
                    device["parameters"] = getDefaultParameters();
                }

                AGO_INFO() << "adding device: uuid=" << uuid << " type: " << device["devicetype"].asString();
                inventory[uuid] = device;
            }

            Json::Value& device (inventory[uuid]);

            device["devicetype"] = content["devicetype"].asString();
            device["internalid"] = content["internalid"].asString();
            device["handled-by"] = content["handled-by"].asString();

            // AGO_TRACE() << "getting name from inventory";
            device["name"] = inv->getDeviceName(uuid);

            if (device["name"].asString() == "") {
                if (device["devicetype"] == "agocontroller") {
                    device["name"] = "agocontroller";
                }
                else if (content.isMember("initial_name") && !wasKnown)
                {
                    // First seen, set name.
                    std::string name(content["initial_name"].asString());

                    // TODO: Should this not be uuid rather than device?
                    if(inv->setDeviceName(content["device"].asString(), name)) {
                        device["name"] = name;
                    }
                }
            }

            // AGO_TRACE() << "getting room from inventory";
            device["room"] = inv->getDeviceRoom(uuid);

            // TODO: Should postpone these; if we do a discovery we will write
            // these to disk for every device..
            saveDevicemap();
            saveDeviceParametersMap();
        }
    }
    else if (subject == "event.device.remove")
    {
        std::string uuid = content["uuid"].asString();
        if (uuid != "")
        {
            AGO_INFO() << "removing device: uuid=" << uuid;
            Json::Value ignored;
            if (inventory.removeMember(uuid, &ignored))
            {
                saveDevicemap();
            }
        }
    }
    else if (subject == "event.environment.timechanged")
    {
        variables["hour"] = content["hour"].asString();
        variables["day"] = content["day"].asString();
        variables["weekday"] = content["weekday"].asString();
        variables["minute"] = content["minute"].asString();
        variables["month"] = content["month"].asString();
    }
    else
    {
        if (subject == "event.environment.positionchanged")
        {
            environment["latitude"] = content["latitude"];
            environment["longitude"] = content["longitude"];
        }

        if (content["uuid"].asString() != "")
        {
            std::string uuid = content["uuid"].asString();
            // see if we have that device in the inventory already, if yes handle the event
            if (inventory.isMember(uuid))
            {
                handleEvent(inventory[uuid], subject, content);
            }
        }
    }
}

void AgoResolver::discoverFunction(const boost::system::error_code& error) {
    if(error) {
        return;
    }

    Json::Value discovercmd;
    discovercmd["command"] = "discover";
    AGO_TRACE() << "Sending discover message";
    agoConnection->sendMessage("",discovercmd);

    discoveryTimer.expires_from_now(pt::seconds(discoverdelay));
    discoveryTimer.async_wait(boost::bind(&AgoResolver::discoverFunction, this, _1));
}

/**
 * Check for stale devices (according to lastseen value)
 */
void AgoResolver::staleFunction(const boost::system::error_code& error)
{
    if( error )
    {
        return;
    }

    //check stale for each devices
    AGO_TRACE() << "staleFunction";
    bool save = false;
    uint64_t now = time(NULL);
    for( auto it = inventory.begin(); it!=inventory.end(); it++ )
    {
        Json::Value& device(*it);
        //AGO_TRACE() << "Checking staleness of " << it.name();
        Json::Value& parameters(device["parameters"]);
        if( parameters.isMember("staleTimeout") )
        {
            int timeout = parameters["staleTimeout"].asInt();
            if(!device["stale"].isIntegral()) {
                // Shouldn't happen unless we have a bug somewhere.
                AGO_WARNING() << "Unexpected non-int 'stale' (instead "
                    << device["stale"].type()
                    << ") element in device " << it.name() << ": " << device.toStyledString();
                device["stale"] = 0;
            }

            int stale = device["stale"].asInt();
            std::string uuid = it.name();
            if( timeout > 0 )
            {
                AGO_TRACE() << "stale=" << stale << " now=" << now << " to+ls=" << (timeout+device["lastseen"].asUInt64());
                //need to check stale
                if( now > (timeout+device["lastseen"].asUInt64()) )
                {
                    //device is stale
                    if( stale==0 )
                    {
                        AGO_DEBUG() << "Device " << it.name() << " is dead";
                        device["stale"] = (uint8_t)1;
                        agoConnection->emitDeviceStale(uuid, 1);

                        save = true;
                    }
                    // else device already stale
                }
                else
                {
                    //device is not stale, check previous status
                    if( stale==1 )
                    {
                        //disable stale status
                        AGO_DEBUG() << "Device " << it.name() << " is alive";
                        device["stale"] = (uint8_t)0;
                        agoConnection->emitDeviceStale(uuid, 0);
                        save = true;
                    }
                    // else device was not stale
                }
            }
        }
        else
        {
            //add staleTimeout field
            AGO_TRACE() << "add missing staleTimeout field on " << it.name();
            parameters["staleTimeout"] = 0;
            save = true;
        }
    }

    if( save )
        saveDevicemap();

    //relaunch timer
    staleTimer.expires_from_now(pt::seconds(60)); //check stale status every minutes
    staleTimer.async_wait(boost::bind(&AgoResolver::staleFunction, this, _1));
}

void AgoResolver::scanSchemaDir(const fs::path &schemaPrefix) {
    // generate vector of all schema files
    std::vector<fs::path> schemaArray;
    fs::path schemadir(schemaPrefix);
    if (fs::exists(schemadir)) {
        fs::recursive_directory_iterator it(schemadir);
        fs::recursive_directory_iterator endit;
        while (it != endit) {
            if (fs::is_regular_file(*it) && (it->path().extension().string() == ".yaml")) {
                schemaArray.push_back(it->path().filename());
            }
            ++it;
        }
    }
    if (schemaArray.size() < 1) {
        throw ConfigurationError("Can't find any schemas in " + schemaPrefix.string());
    }

    // load schema files in proper order
    std::sort(schemaArray.begin(), schemaArray.end());

    fs::path schemaFile = schemaPrefix / schemaArray.front();
    AGO_DEBUG() << "parsing schema file:" << schemaFile;
    schema = parseSchema(schemaFile);
    for (size_t i = 1; i < schemaArray.size(); i++) {
        schemaFile = schemaPrefix / schemaArray[i];
        AGO_DEBUG() << "parsing additional schema file:" << schemaFile;
        schema = mergeMap(schema, parseSchema(schemaFile));
    }
}

void AgoResolver::setupApp()
{
    addCommandHandler();
    addEventHandler();
    agoConnection->setFilter(false);

    fs::path schemaPrefix;

    // XXX: Why is this in system and not resolver config?
    schemaPrefix = getConfigSectionOption("system", "schemapath", getConfigPath(SCHEMADIR));
    discoverdelay = atoi(getConfigSectionOption("system", "discoverdelay", "1800").c_str()); //refresh inventory every 30 mins
    persistence = (atoi(getConfigSectionOption("system","devicepersistence", "0").c_str()) == 1);

    systeminfo["uuid"] = getConfigSectionOption("system", "uuid", "00000000-0000-0000-000000000000");
    systeminfo["version"] = AGOCONTROL_VERSION;

    scanSchemaDir(schemaPrefix);

    AGO_DEBUG() << "reading inventory";
    try
    {
        inv.reset(new Inventory(getConfigPath(INVENTORYDBFILE)));
    }
    catch(std::exception& e)
    {
        AGO_ERROR() << "Failed to load inventory: " << e.what();
        throw ConfigurationError(std::string("Failed to load inventory: ") + e.what());
    }

    readJsonFile(variables, getConfigPath(VARIABLESMAPFILE));
    loadDeviceParametersMap();
    if (persistence)
    {
        AGO_TRACE() << "reading devicemap";
        loadDevicemap();
    }

    agoConnection->addDevice("agocontroller","agocontroller");

    // Wait 2s before first discovery
    discoveryTimer.expires_from_now(pt::seconds(2));
    discoveryTimer.async_wait(boost::bind(&AgoResolver::discoverFunction, this, _1));

    // Wait 10s before first stale check
    staleTimer.expires_from_now(pt::seconds(10));
    staleTimer.async_wait(boost::bind(&AgoResolver::staleFunction, this, _1));
}

void AgoResolver::doShutdown() {
    //stop timers
    staleTimer.cancel();
    discoveryTimer.cancel();
    AgoApp::doShutdown();
}

AGOAPP_ENTRY_POINT(AgoResolver);
