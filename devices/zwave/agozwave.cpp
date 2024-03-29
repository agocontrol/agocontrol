/*
   Copyright (C) 2013 Harald Klein <hari@vt100.at>

   This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

   See the GNU General Public License for more details.

*/

#include <iostream>
#include <sstream>
#include <string.h>

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

#include <limits.h>
#include <float.h>
#include <time.h>


#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "agoapp.h"

#include <openzwave/Options.h>
#include <openzwave/Manager.h>
#include <openzwave/Driver.h>
#include <openzwave/Node.h>
#include <openzwave/Group.h>
#include <openzwave/Notification.h>
#include <openzwave/platform/Log.h>
#include <openzwave/value_classes/ValueStore.h>
#include <openzwave/value_classes/Value.h>
#include <openzwave/value_classes/ValueBool.h>

#include "ZWApi.h"
#include "ZWaveNode.h"

#define CONFIG_BASE_PATH "/etc/openzwave/"
#define CONFIG_MANUFACTURER_SPECIFIC CONFIG_BASE_PATH "manufacturer_specific.xml"

// Helper method to cap an int to 0x00-0xFF
#define CAP_8BIT_INT(value) (max(min(value, 0xFF), 0x00))

/* Use with #if when we want to exclude certain parts to minimum OpenZWave versions.
 * This relies on OPENZWAVE_VERSION_ defines from CMake, they are not official.
 * Note that gitrev is 0 when not built from git.
 * To get a minimium gitrev, you can use 'git describe --long --tags <somecommit>'.
 */
#define HAVE_ZWAVE_VERSION(major,minor,gitrev) \
    (OPENZWAVE_VERSION_MAJOR > (major) || \
        (OPENZWAVE_VERSION_MAJOR == (major) && \
            (OPENZWAVE_VERSION_MINOR > (minor) || \
                (OPENZWAVE_VERSION_MINOR == (minor) && OPENZWAVE_VERSION_REVISION >= (gitrev) ) \
            ) \
        ) \
    )

using namespace agocontrol;
using namespace OpenZWave;

static pthread_mutex_t g_criticalSection;
static pthread_cond_t  initCond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t initMutex = PTHREAD_MUTEX_INITIALIZER;

class AgoZwave: public AgoApp {
private:
    int unitsystem;
    uint32 g_homeId;
    bool g_initFailed;
    Json::Value sentEvents;

    typedef struct {
        uint32			m_homeId;
        uint8			m_nodeId;
        bool			m_polled;
        list<ValueID>	m_values;
    } NodeInfo;
    
    list<NodeInfo*> g_nodes;
    map<ValueID, Json::Value> valueCache;
    ZWaveNodes devices;

    const char *controllerErrorStr(Driver::ControllerError err);
    NodeInfo* getNodeInfo(Notification const* _notification);
    NodeInfo* getNodeInfo(uint32 homeId, uint8 nodeId);
    ValueID* getValueID(int nodeid, int instance, std::string label);
    Json::Value getCommandClassConfigurationParameter(OpenZWave::ValueID* valueID);
    Json::Value getCommandClassWakeUpParameter(OpenZWave::ValueID* valueID);
    bool setCommandClassParameter(uint32 homeId, uint8 nodeId, uint8 commandClassId, uint8 index, std::string newValue);
    void requestAllNodeConfigParameters();
    bool filterEvent(const char *internalId, const char *eventType, std::string level);
    bool emitFilteredEvent(const char *internalId, const char *eventType, const char *level, const char *unit);
    bool emitFilteredEvent(const char *internalId, const char *eventType, double level, const char *unit);
    bool emitFilteredEvent(const char *internalId, const char *eventType, int level, const char *unit);

    Json::Value commandHandler(const Json::Value& content);
    void setupApp();
    void cleanupApp();
    std::string getHRCommandClassId(uint8_t commandClassId);
    std::string getHRNotification(Notification::NotificationType notificationType);

public:
    AGOAPP_CONSTRUCTOR_HEAD(AgoZwave)
        , unitsystem(0)
        , g_homeId(0)
        , g_initFailed(false)
        , sentEvents(Json::objectValue)
     //   , initCond(PTHREAD_COND_INITIALIZER)
     //   , initMutex(PTHREAD_MUTEX_INITIALIZER)
        {}


    void _OnNotification (Notification const* _notification);
    void _controller_update(Driver::ControllerState state,  Driver::ControllerError err);
};

void controller_update(Driver::ControllerState state,  Driver::ControllerError err, void *context) {
    AgoZwave *inst = static_cast<AgoZwave*>(context);
    if (inst != NULL) inst->_controller_update(state, err); 
}

void on_notification(Notification const* _notification, void *context) {
    AgoZwave *inst = static_cast<AgoZwave*>(context);
    inst->_OnNotification(_notification);
}

class MyLog : public i_LogImpl {
    void Write( LogLevel _level, uint8 const _nodeId, char const* _format, va_list _args );
    void QueueDump();
    void QueueClear();
    void SetLoggingState(OpenZWave::LogLevel, OpenZWave::LogLevel, OpenZWave::LogLevel);
    void SetLogFileName(const std::string&);
    std::string GetNodeString(uint8 const _nodeId);
};

void MyLog::QueueDump() {
}
void MyLog::QueueClear() {
}
void MyLog::SetLoggingState(OpenZWave::LogLevel, OpenZWave::LogLevel, OpenZWave::LogLevel) {
}
void MyLog::SetLogFileName(const std::string&) {
}
void MyLog::Write( LogLevel _level, uint8 const _nodeId, char const* _format, va_list _args ) {
    char lineBuf[1024] = {};
    if( _format != NULL && _format[0] != '\0' )
    {
        va_list saveargs;
        va_copy( saveargs, _args );
        vsnprintf( lineBuf, sizeof(lineBuf), _format, _args );
        va_end( saveargs );
    }
    std::string nodeString = GetNodeString(_nodeId);

    if (_level == LogLevel_StreamDetail) AGO_TRACE() << "OZW " << nodeString << std::string(lineBuf);
    else if (_level == LogLevel_Debug) AGO_DEBUG() << "OZW " << nodeString << std::string(lineBuf);
    else if (_level == LogLevel_Detail) AGO_DEBUG() << "OZW " << nodeString << std::string(lineBuf);
    else if (_level == LogLevel_Info) AGO_INFO() << "OZW " << nodeString << std::string(lineBuf);
    else if (_level == LogLevel_Alert) AGO_WARNING() << "OZW " << nodeString << std::string(lineBuf);
    else if (_level == LogLevel_Warning) AGO_WARNING() << "OZW " << nodeString << std::string(lineBuf);
    else if (_level == LogLevel_Error) AGO_ERROR() << "OZW " << nodeString << std::string(lineBuf);
    else if (_level == LogLevel_Fatal) AGO_FATAL() << "OZW " << nodeString << std::string(lineBuf);
    else if (_level == LogLevel_Always) AGO_FATAL() << "OZW " << nodeString << std::string(lineBuf);
    else AGO_FATAL() << "OZW (Unknown level) << " << nodeString << std::string(lineBuf);
}

// Copied directly from open-zwave LogImpl
std::string MyLog::GetNodeString(uint8 const _nodeId)
{
    if( _nodeId == 0 )
    {
        return "";
    }
    else if( _nodeId == 255 )
    {
        // should make distinction between broadcast and controller better for SwitchAll broadcast
        return "contrlr, ";
    }
    else
    {
        char buf[20];
        snprintf( buf, sizeof(buf), "Node%03d, ", _nodeId );
        return buf;
    }
}

const char *AgoZwave::controllerErrorStr (Driver::ControllerError err) {
    switch (err) {
        case Driver::ControllerError_None:
            return "None";
        case Driver::ControllerError_ButtonNotFound:
            return "Button Not Found";
        case Driver::ControllerError_NodeNotFound:
            return "Node Not Found";
        case Driver::ControllerError_NotBridge:
            return "Not a Bridge";
        case Driver::ControllerError_NotPrimary:
            return "Not Primary Controller";
        case Driver::ControllerError_IsPrimary:
            return "Is Primary Controller";
        case Driver::ControllerError_NotSUC:
            return "Not Static Update Controller";
        case Driver::ControllerError_NotSecondary:
            return "Not Secondary Controller";
        case Driver::ControllerError_NotFound:
            return "Not Found";
        case Driver::ControllerError_Busy:
            return "Controller Busy";
        case Driver::ControllerError_Failed:
            return "Failed";
        case Driver::ControllerError_Disabled:
            return "Disabled";
        case Driver::ControllerError_Overflow:
            return "Overflow";
        default:
            return "Unknown error";
    }
}

void AgoZwave::_controller_update(Driver::ControllerState state,  Driver::ControllerError err) {
    Json::Value eventmap;
    eventmap["statecode"]=state;
    switch(state) {
        case Driver::ControllerState_Normal:
            AGO_INFO() << "controller state update: no command in progress";
            eventmap["state"]="normal";
            eventmap["description"]="Normal: No command in progress";
            // nothing to do
            break;
        case Driver::ControllerState_Waiting:
            AGO_INFO() << "controller state update: waiting for user action";
            eventmap["state"]="awaitaction";
            eventmap["description"]="Waiting for user action";
            // waiting for user action
            break;
        case Driver::ControllerState_Cancel:
            AGO_INFO() << "controller state update: command was cancelled";
            eventmap["state"]="cancel";
            eventmap["description"]="Command was cancelled";
            break;
        case Driver::ControllerState_Error:
            AGO_ERROR() << "controller state update: command returned error";
            eventmap["state"]="error";
            eventmap["description"]="Command returned error";
            eventmap["error"] = err;
            eventmap["errorstring"] = controllerErrorStr(err);
            break;
        case Driver::ControllerState_Sleeping:
            AGO_INFO() << "controller state update: device went to sleep";
            eventmap["state"]="sleep";
            eventmap["description"]="Device went to sleep";
            break;

        case Driver::ControllerState_InProgress:
            AGO_INFO() << "controller state update: communicating with other device";
            eventmap["state"]="inprogress";
            eventmap["description"]="Communication in progress";
            // communicating with device
            break;
        case Driver::ControllerState_Completed:
            AGO_INFO() << "controller state update: command has completed successfully";
            eventmap["state"]="success";
            eventmap["description"]="Command completed";
            break;
        case Driver::ControllerState_Failed:
            AGO_ERROR() << "controller state update: command has failed";
            eventmap["state"]="failed";
            eventmap["description"]="Command failed";
            // houston..
            break;
        case Driver::ControllerState_NodeOK:
            AGO_INFO() << "controller state update: node ok";
            eventmap["state"]="nodeok";
            eventmap["description"]="Node OK";
            break;
        case Driver::ControllerState_NodeFailed:
            AGO_ERROR() << "controller state update: node failed";
            eventmap["state"]="nodefailed";
            eventmap["description"]="Node failed";
            break;
        default:
            AGO_INFO() << "controller state update: unknown controller update";
            eventmap["state"]="unknown";
            eventmap["description"]="Unknown controller update";
            break;
    }

    agoConnection->emitEvent("zwavecontroller", "event.zwave.controllerstate", eventmap);

    if (err != Driver::ControllerError_None)  {
        AGO_ERROR() << "Controller error: " << controllerErrorStr(err);
    }
}

/**
 * Return Human Readable CommandClassId
 */
string AgoZwave::getHRCommandClassId(uint8_t commandClassId)
{
    std::string output;
    switch(commandClassId)
    {
        case COMMAND_CLASS_MARK:
            output="COMMAND_CLASS_MARK";
            break;
        case COMMAND_CLASS_BASIC:
            output="COMMAND_CLASS_BASIC";
            break;
        case COMMAND_CLASS_VERSION:
            output="COMMAND_CLASS_VERSION";
            break;
        case COMMAND_CLASS_BATTERY:
            output="COMMAND_CLASS_BATTERY";
            break;
        case COMMAND_CLASS_WAKE_UP:
            output="COMMAND_CLASS_WAKE_UP";
            break;
        case COMMAND_CLASS_CONTROLLER_REPLICATION:
            output="COMMAND_CLASS_CONTROLLER_REPLICATION";
            break;
        case COMMAND_CLASS_SWITCH_MULTILEVEL:
            output="COMMAND_CLASS_SWITCH_MULTILEVEL";
            break;
        case COMMAND_CLASS_SWITCH_ALL:
            output="COMMAND_CLASS_SWITCH_ALL";
            break;
        case COMMAND_CLASS_SENSOR_BINARY:
            output="COMMAND_CLASS_SENSOR_BINARY";
            break;
        case COMMAND_CLASS_SENSOR_MULTILEVEL:
            output="COMMAND_CLASS_SENSOR_MULTILEVEL";
            break;
        case COMMAND_CLASS_SENSOR_ALARM:
            output="COMMAND_CLASS_SENSOR_ALARM";
            break;
        case COMMAND_CLASS_ALARM:
            output="COMMAND_CLASS_ALARM";
            break;
        case COMMAND_CLASS_MULTI_CMD:
            output="COMMAND_CLASS_MULTI_CMD";
            break;
        case COMMAND_CLASS_CLIMATE_CONTROL_SCHEDULE:
            output="COMMAND_CLASS_CLIMATE_CONTROL_SCHEDULE";
            break;
        case COMMAND_CLASS_CLOCK:
            output="COMMAND_CLASS_CLOCK";
            break;
        case COMMAND_CLASS_ASSOCIATION:
            output="COMMAND_CLASS_ASSOCIATION";
            break;
        case COMMAND_CLASS_CONFIGURATION:
            output="COMMAND_CLASS_CONFIGURATION";
            break;
        case COMMAND_CLASS_MANUFACTURER_SPECIFIC:
            output="COMMAND_CLASS_MANUFACTURER_SPECIFIC";
            break;
        case COMMAND_CLASS_APPLICATION_STATUS:
            output="COMMAND_CLASS_APPLICATION_STATUS";
            break;
        case COMMAND_CLASS_ASSOCIATION_COMMAND_CONFIGURATION:
            output="COMMAND_CLASS_ASSOCIATION_COMMAND_CONFIGURATION";
            break;
        case COMMAND_CLASS_AV_CONTENT_DIRECTORY_MD:
            output="COMMAND_CLASS_AV_CONTENT_DIRECTORY_MD";
            break;
        case COMMAND_CLASS_AV_CONTENT_SEARCH_MD:
            output="COMMAND_CLASS_AV_CONTENT_SEARCH_MD";
            break;
        case COMMAND_CLASS_AV_RENDERER_STATUS:
            output="COMMAND_CLASS_AV_RENDERER_STATUS";
            break;
        case COMMAND_CLASS_AV_TAGGING_MD:
            output="COMMAND_CLASS_AV_TAGGING_MD";
            break;
        case COMMAND_CLASS_BASIC_WINDOW_COVERING:
            output="COMMAND_CLASS_BASIC_WINDOW_COVERING";
            break;
        case COMMAND_CLASS_CHIMNEY_FAN:
            output="COMMAND_CLASS_CHIMNEY_FAN";
            break;
        case COMMAND_CLASS_COMPOSITE:
            output="COMMAND_CLASS_COMPOSITE";
            break;
        case COMMAND_CLASS_DOOR_LOCK:
            output="COMMAND_CLASS_DOOR_LOCK";
            break;
        case COMMAND_CLASS_ENERGY_PRODUCTION:
            output="COMMAND_CLASS_ENERGY_PRODUCTION";
            break;
        case COMMAND_CLASS_FIRMWARE_UPDATE_MD:
            output="COMMAND_CLASS_FIRMWARE_UPDATE_MD";
            break;
        case COMMAND_CLASS_GEOGRAPHIC_LOCATION:
            output="COMMAND_CLASS_GEOGRAPHIC_LOCATION";
            break;
        case COMMAND_CLASS_GROUPING_NAME:
            output="COMMAND_CLASS_GROUPING_NAME";
            break;
        case COMMAND_CLASS_HAIL:
            output="COMMAND_CLASS_HAIL";
            break;
        case COMMAND_CLASS_INDICATOR:
            output="COMMAND_CLASS_INDICATOR";
            break;
        case COMMAND_CLASS_IP_CONFIGURATION:
            output="COMMAND_CLASS_IP_CONFIGURATION";
            break;
        case COMMAND_CLASS_LANGUAGE:
            output="COMMAND_CLASS_LANGUAGE";
            break;
        case COMMAND_CLASS_LOCK:
            output="COMMAND_CLASS_LOCK";
            break;
        case COMMAND_CLASS_MANUFACTURER_PROPRIETARY:
            output="COMMAND_CLASS_MANUFACTURER_PROPRIETARY";
            break;
        case COMMAND_CLASS_METER_PULSE:
            output="COMMAND_CLASS_METER_PULSE";
            break;
        case COMMAND_CLASS_METER:
            output="COMMAND_CLASS_METER";
            break;
        case COMMAND_CLASS_MTP_WINDOW_COVERING:
            output="COMMAND_CLASS_MTP_WINDOW_COVERING";
            break;
        case COMMAND_CLASS_MULTI_INSTANCE_ASSOCIATION:
            output="COMMAND_CLASS_MULTI_INSTANCE_ASSOCIATION";
            break;
        case COMMAND_CLASS_MULTI_INSTANCE:
            output="COMMAND_CLASS_MULTI_INSTANCE";
            break;
        case COMMAND_CLASS_NO_OPERATION:
            output="COMMAND_CLASS_NO_OPERATION";
            break;
        case COMMAND_CLASS_NODE_NAMING:
            output="COMMAND_CLASS_NODE_NAMING";
            break;
        case COMMAND_CLASS_NON_INTEROPERABLE:
            output="COMMAND_CLASS_NON_INTEROPERABLE";
            break;
        case COMMAND_CLASS_POWERLEVEL:
            output="COMMAND_CLASS_POWERLEVEL";
            break;
        case COMMAND_CLASS_PROPRIETARY:
            output="COMMAND_CLASS_PROPRIETARY";
            break;
        case COMMAND_CLASS_PROTECTION:
            output="COMMAND_CLASS_PROTECTION";
            break;
        case COMMAND_CLASS_REMOTE_ASSOCIATION_ACTIVATE:
            output="COMMAND_CLASS_REMOTE_ASSOCIATION_ACTIVATE";
            break;
        case COMMAND_CLASS_REMOTE_ASSOCIATION:
            output="COMMAND_CLASS_REMOTE_ASSOCIATION";
            break;
        case COMMAND_CLASS_SCENE_ACTIVATION:
            output="COMMAND_CLASS_SCENE_ACTIVATION";
            break;
        case COMMAND_CLASS_SCENE_ACTUATOR_CONF:
            output="COMMAND_CLASS_SCENE_ACTUATOR_CONF";
            break;
        case COMMAND_CLASS_SCENE_CONTROLLER_CONF:
            output="COMMAND_CLASS_SCENE_CONTROLLER_CONF";
            break;
        case COMMAND_CLASS_SCREEN_ATTRIBUTES:
            output="COMMAND_CLASS_SCREEN_ATTRIBUTES";
            break;
        case COMMAND_CLASS_SCREEN_MD:
            output="COMMAND_CLASS_SCREEN_MD";
            break;
        case COMMAND_CLASS_SECURITY:
            output="COMMAND_CLASS_SECURITY";
            break;
        case COMMAND_CLASS_SENSOR_CONFIGURATION:
            output="COMMAND_CLASS_SENSOR_CONFIGURATION";
            break;
        case COMMAND_CLASS_SILENCE_ALARM:
            output="COMMAND_CLASS_SILENCE_ALARM";
            break;
        case COMMAND_CLASS_SIMPLE_AV_CONTROL:
            output="COMMAND_CLASS_SIMPLE_AV_CONTROL";
            break;
        case COMMAND_CLASS_SWITCH_BINARY:
            output="COMMAND_CLASS_SWITCH_BINARY";
            break;
        case COMMAND_CLASS_SWITCH_TOGGLE_BINARY:
            output="COMMAND_CLASS_SWITCH_TOGGLE_BINARY";
            break;
        case COMMAND_CLASS_SWITCH_TOGGLE_MULTILEVEL:
            output="COMMAND_CLASS_SWITCH_TOGGLE_MULTILEVEL";
            break;
        case COMMAND_CLASS_THERMOSTAT_FAN_MODE:
            output="COMMAND_CLASS_THERMOSTAT_FAN_MODE";
            break;
        case COMMAND_CLASS_THERMOSTAT_FAN_STATE:
            output="COMMAND_CLASS_THERMOSTAT_FAN_STATE";
            break;
        case COMMAND_CLASS_THERMOSTAT_HEATING:
            output="COMMAND_CLASS_THERMOSTAT_HEATING";
            break;
        case COMMAND_CLASS_THERMOSTAT_MODE:
            output="COMMAND_CLASS_THERMOSTAT_MODE";
            break;
        case COMMAND_CLASS_THERMOSTAT_OPERATING_STATE:
            output="COMMAND_CLASS_THERMOSTAT_OPERATING_STATE";
            break;
        case COMMAND_CLASS_THERMOSTAT_SETBACK:
            output="COMMAND_CLASS_THERMOSTAT_SETBACK";
            break;
        case COMMAND_CLASS_THERMOSTAT_SETPOINT:
            output="COMMAND_CLASS_THERMOSTAT_SETPOINT";
            break;
        case COMMAND_CLASS_TIME_PARAMETERS:
            output="COMMAND_CLASS_TIME_PARAMETERS";
            break;
        case COMMAND_CLASS_TIME:
            output="COMMAND_CLASS_TIME";
            break;
        case COMMAND_CLASS_USER_CODE:
            output="COMMAND_CLASS_USER_CODE";
            break;
        case COMMAND_CLASS_ZIP_ADV_CLIENT:
            output="COMMAND_CLASS_ZIP_ADV_CLIENT";
            break;
        //case COMMAND_CLASS_ZIP_ADV_SERVER:
        //    output="COMMAND_CLASS_ZIP_ADV_SERVER";
        //    break;
        case COMMAND_CLASS_ZIP_ADV_SERVICES:
            output="COMMAND_CLASS_ZIP_ADV_SERVICES";
            break;
        case COMMAND_CLASS_ZIP_CLIENT:
            output="COMMAND_CLASS_ZIP_CLIENT";
            break;
        case COMMAND_CLASS_ZIP_SERVER:
            output="COMMAND_CLASS_ZIP_SERVER";
            break;
        case COMMAND_CLASS_ZIP_SERVICES:
            output="COMMAND_CLASS_ZIP_SERVICES";
            break;
        case COMMAND_CLASS_COLOR:
            output="COMMAND_CLASS_COLOR";
            break;
        default:
            std::stringstream temp;
            temp << "COMMAND_UNKNOWN[" << (int)commandClassId << "]";
            output = temp.str();
    }
    return output;
}

/**
 * Return human readable notification type.
 */
string AgoZwave::getHRNotification(Notification::NotificationType notificationType)
{
    std::string output;
    switch( notificationType )
    {
        case Notification::Type_ValueAdded:
            output="Type_ValueAdded";
            break;
        case Notification::Type_ValueRemoved:
            output="Type_ValueRemoved";
            break;
        case Notification::Type_ValueChanged:
            output="Type_ValueChanged";
            break;
        case Notification::Type_ValueRefreshed:
            output="Type_ValueRefreshed";
            break;
        case Notification::Type_Group:
            output="Type_Group";
            break;
        case Notification::Type_NodeNew:
            output="Type_NodeNew";
            break;
        case Notification::Type_NodeAdded:
            output="Type_NodeAdded";
            break;
        case Notification::Type_NodeRemoved:
            output="Type_NodeRemoved";
            break;
        case Notification::Type_NodeProtocolInfo:
            output="Type_NodeProtocolInfo";
            break;
        case Notification::Type_NodeNaming:
            output="Type_NodeNaming";
            break;
        case Notification::Type_NodeEvent:
            output="Type_NodeEvent";
            break;
        case Notification::Type_PollingDisabled:
            output="Type_PollingDisabled";
            break;
        case Notification::Type_PollingEnabled:
            output="Type_PollingEnabled";
            break;
        case Notification::Type_SceneEvent:
            output="Type_SceneEvent";
            break;
        case Notification::Type_CreateButton:
            output="Type_CreateButton";
            break;
        case Notification::Type_DeleteButton:
            output="Type_DeleteButton";
            break;
        case Notification::Type_ButtonOn:
            output="Type_ButtonOn";
            break;
        case Notification::Type_ButtonOff:
            output="Type_ButtonOff";
            break;
        case Notification::Type_DriverReady:
            output="Type_DriverReady";
            break;
        case Notification::Type_DriverFailed:
            output="Type_DriverFailed";
            break;
        case Notification::Type_DriverReset:
            output="Type_DriverReset";
            break;
        case Notification::Type_EssentialNodeQueriesComplete:
            output="Type_EssentialNodeQueriesComplete";
            break;
        case Notification::Type_NodeQueriesComplete:
            output="Type_NodeQueriesComplete";
            break;
        case Notification::Type_AwakeNodesQueried:
            output="Type_AwakeNodesQueried";
            break;
        case Notification::Type_AllNodesQueriedSomeDead:
            output="Type_AllNodesQueriedSomeDead";
            break;
        case Notification::Type_AllNodesQueried:
            output="Type_AllNodesQueried";
            break;
        case Notification::Type_Notification:
            output="Type_Notification";
            break;
        case Notification::Type_DriverRemoved:
            output="Type_DriverRemoved";
            break;
#if HAVE_ZWAVE_VERSION(1,3,397)
        case Notification::Type_ControllerCommand:
            output="Type_ControllerCommand";
            break;
        case Notification::Type_NodeReset:
            output="Type_NodeReset";
            break;
#endif
#if HAVE_ZWAVE_VERSION(1,5,208)
        case Notification::Type_UserAlerts:
            output="Type_UserAlerts";
            break;
#endif
#if HAVE_ZWAVE_VERSION(1,5,214)
        case Notification::Type_ManufacturerSpecificDBReady:
            output="Type_ManufacturerSpecificDBReady";
            break;
#endif
        default:
            output="Type_Unknown";
    }
    return output;
}

/**
 * Emit event like in agoconnection but filtered duplicated event within the same second
 */
bool AgoZwave::filterEvent(const char *internalId, const char *eventType, std::string level)
{
    bool filtered = false;
    std::string sInternalId = std::string(internalId);
    std::string sEventType = std::string(eventType);

    Json::Value infos;
    uint64_t now = (uint64_t)(time(NULL));
    if( sentEvents.isMember(sInternalId) )
    {
        //add new entry
        AGO_TRACE() << "filterEvent: create new entry for internalid " << sInternalId;
        infos["eventype"] = sEventType;
        infos["level"] = level;
        infos["timestamp"] = now;
        sentEvents[sInternalId] = infos;

        //no filtering
        filtered = false;
    }
    else
    {
        infos = sentEvents[sInternalId];
        //check level
        if( infos.isMember("level") )
        {
            std::string oldLevel = infos["level"].asString();
            if( oldLevel==level )
            {
                if( infos.isMember("timestamp") )
                {
                    //same level received, check timestamp
                    uint64_t oldTimestamp = infos["timestamp"].asUInt64();
                    //1 seconds seems to be enough
                    if( now<=(oldTimestamp+1) )
                    {
                        //same event sent too quickly, drop it
                        AGO_TRACE() << "filterEvent: event " << sEventType  << " for " << sInternalId << " is filtered";
                        filtered = true;
                    }
                    else
                    {
                        //last event was sent some time ago, no filtering
                        AGO_TRACE() << "filterEvent: duplicated event " << sEventType << " detected for " << sInternalId << "but timeout expired";
                        filtered = false;
                    }
                }
                else
                {
                    //should not happen
                    AGO_TRACE() << "filterEvent: infos[timestamp] isn't exist";
                    filtered = false;
                }
            }
            else
            {
                //level is different, no filtering
                AGO_TRACE() << "filterEvent: level " << level << " is different from old one " << oldLevel << " for " << sInternalId;
                filtered = false;
            }
        }
        else
        {
            //should not happen
            AGO_TRACE() << "filterEvent: infos[level] isn't exist";
            filtered = false;
        }

        //update map
        if( !filtered )
        {
            infos["level"] = level;
            infos["timestamp"] = now;
            sentEvents[sInternalId] = infos;
        }
    }

    AGO_TRACE() << "filterEvent: sentEvent: " << sentEvents;

    return filtered;
}

bool AgoZwave::emitFilteredEvent(const char *internalId, const char *eventType, const char *level, const char *unit)
{
    std::string sLevel = std::string(level);
    if( !filterEvent(internalId, eventType, sLevel) )
    {
        return agoConnection->emitEvent(internalId, eventType, level, unit);
    }
    else
    {
        AGO_DEBUG() << "Event '" << eventType << "' from '" << internalId << "' at level '" << level << "' is filtered";
        return true;
    }
}

bool AgoZwave::emitFilteredEvent(const char *internalId, const char *eventType, double level, const char *unit)
{
    std::stringstream sLevel;
    sLevel << level;
    if( !filterEvent(internalId, eventType, sLevel.str()) )
    {
        return agoConnection->emitEvent(internalId, eventType, level, unit);
    }
    else
    {
        AGO_DEBUG() << "Event '" << eventType << "' from '" << internalId << "' at level '" << level << "' is filtered";
        return true;
    }
}

bool AgoZwave::emitFilteredEvent(const char *internalId, const char *eventType, int level, const char *unit)
{
    std::stringstream sLevel;
    sLevel << level;
    if( !filterEvent(internalId, eventType, sLevel.str()) )
    {
        return agoConnection->emitEvent(internalId, eventType, level, unit);
    }
    else
    {
        AGO_DEBUG() << "Event '" << eventType << "' from '" << internalId << "' at level '" << level << "' is filtered";
        return true;
    }
}

//-----------------------------------------------------------------------------
// <getNodeInfo>
// Callback that is triggered when a value, group or node changes
//-----------------------------------------------------------------------------
AgoZwave::NodeInfo* AgoZwave::getNodeInfo (Notification const* _notification)
{
    uint32 const homeId = _notification->GetHomeId();
    uint8 const nodeId = _notification->GetNodeId();
    for( list<NodeInfo*>::iterator it = g_nodes.begin(); it != g_nodes.end(); ++it )
    {
        NodeInfo* nodeInfo = *it;
        if( ( nodeInfo->m_homeId == homeId ) && ( nodeInfo->m_nodeId == nodeId ) )
        {
            return nodeInfo;
        }
    }

    return NULL;
}

AgoZwave::NodeInfo* AgoZwave::getNodeInfo(uint32 homeId, uint8 nodeId)
{
    NodeInfo* result = NULL;

    for( list<NodeInfo*>::iterator it = g_nodes.begin(); it != g_nodes.end(); ++it )
    {
        NodeInfo* nodeInfo = *it;
        if( nodeInfo->m_homeId==homeId && nodeInfo->m_nodeId==nodeId )
        {
            result = nodeInfo;
            break;
        }
    }

    return result;
}


ValueID* AgoZwave::getValueID(int nodeid, int instance, std::string label)
{
    for( list<NodeInfo*>::iterator it = g_nodes.begin(); it != g_nodes.end(); ++it )
    {
        for (list<ValueID>::iterator it2 = (*it)->m_values.begin(); it2 != (*it)->m_values.end(); it2++ )
        {
            if ( ((*it)->m_nodeId == nodeid) && ((*it2).GetInstance() == instance) )
            {
                std::string valuelabel = OpenZWave::Manager::Get()->GetValueLabel((*it2));
                if (label == valuelabel)
                {
                    // AGO_TRACE() << "Found ValueID: " << (*it2).GetId();
                    return &(*it2);
                }
            }
        }
    }
    return NULL;
}

/**
 * Get COMMAND_CLASS_CONFIGURATION parameter
 * @return map containing all parameter infos
 */
Json::Value AgoZwave::getCommandClassConfigurationParameter(OpenZWave::ValueID* valueID)
{
    //init
    Json::Value param(Json::objectValue);
    if( valueID==NULL )
    {
        return param;
    }

    //add invalid parameter property
    //give info to ui that parameter is invalid
    param["invalid"] = false;

    //get parameter type
    std::vector<string> ozwItems;
    Json::Value items(Json::arrayValue);
    switch( valueID->GetType() )
    {
        case ValueID::ValueType_Decimal:
            //Represents a non-integer value as a string, to avoid floating point accuracy issues. 
            param["type"] = "float";
            break;
        case ValueID::ValueType_Bool:
            //Boolean, true or false 
            param["type"] = "bool";
            break;
        case ValueID::ValueType_Byte:
            //8-bit unsigned value 
            param["type"] = "byte";
            break;
        case ValueID::ValueType_Short:
            //16-bit signed value 
            param["type"] = "short";
            break;
        case ValueID::ValueType_Int:
            //32-bit signed value 
            param["type"] = "int";
            break;
        case ValueID::ValueType_Button:
            //A write-only value that is the equivalent of pressing a button to send a command to a device
            param["type"] = "button";
            param["invalid"] = true;
            break;
        case ValueID::ValueType_List:
            //List from which one item can be selected 
            param["type"] = "list";

            //get list items
            Manager::Get()->GetValueListItems(*valueID, &ozwItems);
            for( auto it3 = ozwItems.begin(); it3 != ozwItems.end(); it3++ )
            {
                items.append(*it3);
            }
            param["items"] = items;
            break;
        case ValueID::ValueType_Schedule:
            //Complex type used with the Climate Control Schedule command class
            param["type"] = "notsupported";
            param["invalid"] = true;
            break;
        case ValueID::ValueType_String:
            //Text string
            param["type"] = "string";
            break;
        case ValueID::ValueType_Raw:
            //A collection of bytes 
            param["type"] = "notsupported";
            param["invalid"] = true;
            break;
        default:
            param["invalid"] = true;
            param["type"] = "unknown";
    }

    //get parameter value
    std::string currentValue = "";
    if( Manager::Get()->GetValueAsString(*valueID, &currentValue) )
    {
        param["currentvalue"] = currentValue;
    }
    else
    {
        //unable to get current value
        param["invalid"] = true;
        param["currentvalue"] = "";
    }

    //get other infos
    param["label"] = Manager::Get()->GetValueLabel(*valueID);
    param["units"] = Manager::Get()->GetValueUnits(*valueID);
    param["help"] = Manager::Get()->GetValueHelp(*valueID);
    param["min"] = Manager::Get()->GetValueMin(*valueID);
    param["max"] = Manager::Get()->GetValueMax(*valueID);
    param["readonly"] = Manager::Get()->IsValueReadOnly(*valueID);
    param["commandclassid"] = COMMAND_CLASS_CONFIGURATION;
    param["instance"] = valueID->GetInstance();
    param["index"] = valueID->GetIndex();


    return param;
}

/**
 * Get COMMAND_CLASS_WAKE_UP parameter
 * @return map containing all parameter infos
 */
Json::Value AgoZwave::getCommandClassWakeUpParameter(OpenZWave::ValueID* valueID)
{
    Json::Value param(Json::objectValue);
    std::string currentValue = "";

    if( Manager::Get()->GetValueAsString(*valueID, &currentValue) )
    {
        //everything's good, fill param
        param["currentvalue"] = currentValue;
        param["type"] = "int";
        param["label"] = Manager::Get()->GetValueLabel(*valueID);
        param["units"] = Manager::Get()->GetValueUnits(*valueID);
        param["help"] = Manager::Get()->GetValueHelp(*valueID);
        param["readonly"] = Manager::Get()->IsValueReadOnly(*valueID);
        param["min"] = Manager::Get()->GetValueMin(*valueID);
        param["max"] = Manager::Get()->GetValueMax(*valueID);
        param["invalid"] = 0;
        param["commandclassid"] = COMMAND_CLASS_WAKE_UP;
        param["instance"] = valueID->GetInstance();
        param["index"] = valueID->GetIndex();
    }

    return param;
}

/**
 * Set device parameter
 * @return true if parameter found and set successfully
 */
bool AgoZwave::setCommandClassParameter(uint32 homeId, uint8 nodeId, uint8 commandClassId, uint8 index, std::string newValue)
{
    bool result = false;

    //get node info
    NodeInfo* nodeInfo = getNodeInfo(homeId, nodeId);

    if( nodeInfo!=NULL )
    {
        for( auto it=nodeInfo->m_values.begin(); it!=nodeInfo->m_values.end(); it++ )
        {
            AGO_DEBUG() << "--> commandClassId=" << getHRCommandClassId(it->GetCommandClassId()) << " index=" << std::dec << (int)it->GetIndex();
            if( it->GetCommandClassId()==commandClassId && it->GetIndex()==index )
            {
                //parameter found

                //check if value has been modified
                std::string oldValue;
                Manager::Get()->GetValueAsString(*it, &oldValue);

                if( oldValue!=newValue )
                {                  
                    if( it->GetType()==ValueID::ValueType_List )
                    {
                        //set list value
                        result = Manager::Get()->SetValueListSelection(*it, newValue);
                        if( !result )
                        {
                            AGO_ERROR() << "Unable to set value list selection with newValue=" << newValue;
                        }
                    }
                    else
                    {
                        //set value
                        result = Manager::Get()->SetValue(*it, newValue);
                        if( !result )
                        {
                            AGO_ERROR() << "Unable to set value with newValue=" << newValue;
                        }
                    }
                }
                else
                {
                    //oldValue == newValue. Nothing done but return true anyway
                    AGO_DEBUG() << "setCommandClassParameter: try to set parameter with same value [" << oldValue << "=" << newValue << "]. Nothing done.";
                    result = true;
                }

                //stop statement
                break;
            }
        }
    }

    //handle error
    if( result==false )
    {
        if( nodeInfo==NULL )
        {
            //node not found!
            AGO_ERROR() << "Node not found! (homeId=" << homeId << " nodeId=" << (int)nodeId << " commandClassId=" << (int)commandClassId << " index=" << (int)index << ")";
        }
        else
        {
            //ValueID not found!
            AGO_ERROR() << "ValueID not found! (homeId=" << homeId << " nodeId=" << (int)nodeId << " commandClassId=" << (int)commandClassId << " index=" << (int)index << ")";
        }
    }

    return result;
}

/**
 * Request config parameters for all nodes
 */
void AgoZwave::requestAllNodeConfigParameters()
{
    for( list<NodeInfo*>::iterator it=g_nodes.begin(); it!=g_nodes.end(); ++it )
    {
        NodeInfo* nodeInfo = *it;
        Manager::Get()->RequestAllConfigParams(nodeInfo->m_homeId, nodeInfo->m_nodeId);
    }
}


//-----------------------------------------------------------------------------
// <OnNotification>
// Callback that is triggered when a value, group or node changes
//-----------------------------------------------------------------------------
void AgoZwave::_OnNotification (Notification const* _notification)
{
    Json::Value eventmap;
    // Must do this inside a critical section to avoid conflicts with the main thread
    pthread_mutex_lock( &g_criticalSection );

    AGO_DEBUG() << "Notification " << getHRNotification(_notification->GetType()) << " received";

    switch( _notification->GetType() )
    {
        case Notification::Type_ValueAdded:
        {
            if( NodeInfo* nodeInfo = getNodeInfo( _notification ) )
            {
                // Add the new value to our list
                nodeInfo->m_values.push_back( _notification->GetValueID() );
                uint8 basic = Manager::Get()->GetNodeBasic(_notification->GetHomeId(),_notification->GetNodeId());
                uint8 generic = Manager::Get()->GetNodeGeneric(_notification->GetHomeId(),_notification->GetNodeId());
                /*uint8 specific =*/ Manager::Get()->GetNodeSpecific(_notification->GetHomeId(),_notification->GetNodeId());
                ValueID id = _notification->GetValueID();
                std::string label = Manager::Get()->GetValueLabel(id);
                std::stringstream tempstream;
                tempstream << (int) _notification->GetNodeId();
                tempstream << "/";
                tempstream << (int) id.GetInstance();
                std::string nodeinstance = tempstream.str();
                tempstream << "-";
                tempstream << label;
                std::string tempstring = tempstream.str();
                ZWaveNode *device;

                if (id.GetGenre() == ValueID::ValueGenre_Config)
                {
                    AGO_INFO() << "Configuration parameter Value Added: Home=" << std::hex <<  _notification->GetHomeId()
                        << " Node=" << std::dec << (int) _notification->GetNodeId()
                        << " Genre=" << std::dec << (int) id.GetGenre()
                        << " Class=" << getHRCommandClassId(id.GetCommandClassId())
                        << " Instance=" << (int)id.GetInstance()
                        << " Index=" << (int)id.GetIndex()
                        << " Type=" << (int)id.GetType()
                        << " Label=" << label;
                }
                else if (basic == BASIC_TYPE_CONTROLLER)
                {
                    if ((device = devices.findId(nodeinstance)) != NULL)
                    {
                        device->addValue(label, id);
                        device->setDevicetype("remote");
                    }
                    else
                    {
                        device = new ZWaveNode(nodeinstance, "remote");
                        device->addValue(label, id);
                        devices.add(device);
                        AGO_DEBUG() << "Controller: add new remote [" << device->getId() << ", " << device->getDevicetype() << "]";
                        agoConnection->addDevice(device->getId(), device->getDevicetype());
                    }
                }
                else
                {
                    switch (generic)
                    {
                        case GENERIC_TYPE_THERMOSTAT:
                            if ((device = devices.findId(nodeinstance)) == NULL)
                            {
                                device = new ZWaveNode(nodeinstance, "thermostat");
                                devices.add(device);
                                AGO_DEBUG() << "Thermostat: add new thermostat [" << device->getId() << ", " << device->getDevicetype() << "]";
                                agoConnection->addDevice(device->getId(), device->getDevicetype());
                            }
                            break;
                        case GENERIC_TYPE_SWITCH_MULTILEVEL:
                            if ((device = devices.findId(nodeinstance)) == NULL)
                            {
                                device = new ZWaveNode(nodeinstance, "dimmer");
                                devices.add(device);
                                AGO_DEBUG() << "Switch multilevel: add new dimmer [" << device->getId() << ", " << device->getDevicetype() << "]";
                                agoConnection->addDevice(device->getId(), device->getDevicetype());
                            }
                            break;
                    }

                    switch(id.GetCommandClassId())
                    {
                        case COMMAND_CLASS_CONFIGURATION:
                            AGO_DEBUG() << "COMMAND_CLASS_CONFIGURATION received";
                            break;
                        case COMMAND_CLASS_BATTERY:
                            if( label=="Battery Level" )
                            {
                                if ((device = devices.findId(tempstring)) != NULL)
                                {
                                    AGO_DEBUG() << "Battery: append batterysensor [" << label << "]";
                                    device->addValue(label, id);
                                }
                                else
                                {
                                    device = new ZWaveNode(tempstring, "batterysensor");
                                    device->addValue(label, id);
                                    devices.add(device);
                                    AGO_DEBUG() << "Battery: add new batterysensor [" << device->getId() << ", " << device->getDevicetype() << "]";
                                    agoConnection->addDevice(device->getId(), device->getDevicetype());
                                }
                            }
                            break;
                        case COMMAND_CLASS_COLOR:
                            if (label == "Color")
                            {
                                if ((device = devices.findId(nodeinstance)) != NULL)
                                {
                                    device->addValue(label, id);
                                    device->setDevicetype("dimmerrgb");
                                    agoConnection->addDevice(device->getId(), device->getDevicetype());
                                }
                                else
                                {
                                    device = new ZWaveNode(nodeinstance, "dimmerrgb");
                                    device->addValue(label, id);
                                    devices.add(device);
                                    AGO_DEBUG() << "Color: add new dimmerrgb [" << device->getId() << ", " << device->getDevicetype() << "]";
                                    agoConnection->addDevice(device->getId(), device->getDevicetype());
                                }
                            }
                            break;
                        case COMMAND_CLASS_SWITCH_MULTILEVEL:
                            if (label == "Level")
                            {
                                if ((device = devices.findId(nodeinstance)) != NULL)
                                {
                                    device->addValue(label, id);
                                    // device->setDevicetype("dimmer");
                                }
                                else
                                {
                                    device = new ZWaveNode(nodeinstance, "dimmer");
                                    device->addValue(label, id);
                                    devices.add(device);
                                    AGO_DEBUG() << "Switch multilevel: add new dimmer [" << device->getId() << ", " << device->getDevicetype() << "]";
                                    agoConnection->addDevice(device->getId(), device->getDevicetype());
                                }
                                // Manager::Get()->EnablePoll(id);
                            }
                            break;
                        case COMMAND_CLASS_SWITCH_BINARY:
                            if (label == "Switch")
                            {
                                if ((device = devices.findId(nodeinstance)) != NULL)
                                {
                                    device->addValue(label, id);
                                }
                                else
                                {
                                    device = new ZWaveNode(nodeinstance, "switch");
                                    device->addValue(label, id);
                                    devices.add(device);
                                    AGO_DEBUG() << "Switch binary: add new switch [" << device->getId() << ", " << device->getDevicetype() << "]";
                                    agoConnection->addDevice(device->getId(), device->getDevicetype());
                                }
                                // Manager::Get()->EnablePoll(id);
                            }
                            break;
                        case COMMAND_CLASS_SENSOR_BINARY:
                            if (label == "Sensor")
                            {
                                if ((device = devices.findId(tempstring)) != NULL)
                                {
                                    device->addValue(label, id);
                                }
                                else
                                {
                                    device = new ZWaveNode(tempstring, "binarysensor");
                                    device->addValue(label, id);
                                    devices.add(device);
                                    AGO_DEBUG() << "Sensor binary: add new binarysensor [" << device->getId() << ", " << device->getDevicetype() << "]";
                                    agoConnection->addDevice(device->getId(), device->getDevicetype());
                                }
                                // Manager::Get()->EnablePoll(id);
                            }
                            break;
                        case COMMAND_CLASS_SENSOR_MULTILEVEL:
                            if (label == "Luminance")
                            {
                                device = new ZWaveNode(tempstring, "brightnesssensor");
                                device->addValue(label, id);
                                devices.add(device);
                                AGO_DEBUG() << "Sensor multilevel: add new brightnesssensor [" << device->getId() << ", " << device->getDevicetype() << "]";
                                agoConnection->addDevice(device->getId(), device->getDevicetype());
                            }
                            else if (label == "Ultraviolet")
                            {
                                device = new ZWaveNode(tempstring, "uvsensor");
                                device->addValue(label, id);
                                devices.add(device);
                                AGO_DEBUG() << "Sensor multilevel: add new uvsensor [" << device->getId() << ", " << device->getDevicetype() << "]";
                                agoConnection->addDevice(device->getId(), device->getDevicetype());
                            }
                            else if (label == "Temperature")
                            {
                                if (generic == GENERIC_TYPE_THERMOSTAT)
                                {
                                    if ((device = devices.findId(nodeinstance)) != NULL)
                                    {
                                        device->addValue(label, id);
                                    }
                                    else
                                    {
                                        device = new ZWaveNode(nodeinstance, "thermostat");
                                        device->addValue(label, id);
                                        devices.add(device);
                                        AGO_DEBUG() << "Sensor multilevel: add new thermostat [" << device->getId() << ", " << device->getDevicetype() << "]";
                                        agoConnection->addDevice(device->getId(), device->getDevicetype());
                                    }
                                }
                                else
                                {
                                    device = new ZWaveNode(tempstring, "temperaturesensor");
                                    device->addValue(label, id);
                                    devices.add(device);
                                    AGO_DEBUG() << "Sensor multilevel: add new temperaturesensor [" << device->getId() << ", " << device->getDevicetype() << "]";
                                    agoConnection->addDevice(device->getId(), device->getDevicetype());
                                }
                            }
                            else if( label.find("Humidity")!=std::string::npos )
                            {
                                device = new ZWaveNode(tempstring, "humiditysensor");
                                device->addValue(label, id);
                                devices.add(device);
                                AGO_DEBUG() << "Sensor multilevel: add new humiditysensor [" << device->getId() << ", " << device->getDevicetype() << "]";
                                agoConnection->addDevice(device->getId(), device->getDevicetype());
                            }
                            else
                            {
                                AGO_WARNING() << "Unhandled label for SENSOR_MULTILEVEL. Adding generic multilevelsensor for label: " << label;
                                if ((device = devices.findId(nodeinstance)) != NULL)
                                {
                                    device->addValue(label, id);
                                }
                                else
                                {
                                    device = new ZWaveNode(nodeinstance, "multilevelsensor");
                                    device->addValue(label, id);
                                    devices.add(device);
                                    AGO_DEBUG() << "Sensor multilevel: add new multisensor [" << device->getId() << ", " << device->getDevicetype() << "]";
                                    agoConnection->addDevice(device->getId(), device->getDevicetype());
                                }
                            }
                            // Manager::Get()->EnablePoll(id);
                            break;
                        case COMMAND_CLASS_METER:
                            if (label == "Power")
                            {
                                device = new ZWaveNode(tempstring, "powermeter");
                                device->addValue(label, id);
                                devices.add(device);
                                AGO_DEBUG() << "Meter: add new powermeter [" << device->getId() << ", " << device->getDevicetype() << "]";
                                agoConnection->addDevice(device->getId(), device->getDevicetype());
                            }
                            else if (label == "Energy")
                            {
                                device = new ZWaveNode(tempstring, "energymeter");
                                device->addValue(label, id);
                                devices.add(device);
                                AGO_DEBUG() << "Meter: add new energymeter [" << device->getId() << ", " << device->getDevicetype() << "]";
                                agoConnection->addDevice(device->getId(), device->getDevicetype());
                            }
                            else
                            {
                                AGO_WARNING() << "unhandled label for CLASS_METER. Adding generic multilevelsensor for label: " << label;
                                if ((device = devices.findId(nodeinstance)) != NULL)
                                {
                                    device->addValue(label, id);
                                }
                                else
                                {
                                    device = new ZWaveNode(nodeinstance, "multilevelsensor");
                                    device->addValue(label, id);
                                    devices.add(device);
                                    AGO_DEBUG() << "Meter: add new multilevelsensor [" << device->getId() << ", " << device->getDevicetype() << "]";
                                    agoConnection->addDevice(device->getId(), device->getDevicetype());
                                }
                            }
                            // Manager::Get()->EnablePoll(id);
                            break;
                        case COMMAND_CLASS_BASIC_WINDOW_COVERING:
                            // if (label == "Open") {
                            if ((device = devices.findId(nodeinstance)) != NULL)
                            {
                                device->addValue(label, id);
                                device->setDevicetype("drapes");
                            }
                            else
                            {
                                device = new ZWaveNode(nodeinstance, "drapes");
                                device->addValue(label, id);
                                devices.add(device);
                                AGO_DEBUG() << "Basic window covering: add new drapes [" << device->getId() << ", " << device->getDevicetype() << "]";
                                agoConnection->addDevice(device->getId(), device->getDevicetype());
                            }
                            // Manager::Get()->EnablePoll(id);
                            //}
                            break;
                        case COMMAND_CLASS_THERMOSTAT_SETPOINT:
                        case COMMAND_CLASS_THERMOSTAT_MODE:
                        case COMMAND_CLASS_THERMOSTAT_FAN_MODE:
                        case COMMAND_CLASS_THERMOSTAT_FAN_STATE:
                        case COMMAND_CLASS_THERMOSTAT_OPERATING_STATE:
                            AGO_DEBUG() << "adding thermostat label: " << label;
                            if ((device = devices.findId(nodeinstance)) != NULL)
                            {
                                device->addValue(label, id);
                                device->setDevicetype("thermostat");
                            }
                            else
                            {
                                device = new ZWaveNode(nodeinstance, "thermostat");
                                device->addValue(label, id);
                                devices.add(device);
                                AGO_DEBUG() << "Thermostat: add new thermostat [" << device->getId() << ", " << device->getDevicetype() << "]";
                                agoConnection->addDevice(device->getId(), device->getDevicetype());
                            }
                            break;
                        case COMMAND_CLASS_ALARM:
                        case COMMAND_CLASS_SENSOR_ALARM:
                            if (label == "Sensor" || label == "Alarm Level" || label == "Flood")
                            {
                                if ((device = devices.findId(tempstring)) != NULL)
                                {
                                    device->addValue(label, id);
                                }
                                else
                                {
                                    device = new ZWaveNode(tempstring, "binarysensor");
                                    device->addValue(label, id);
                                    devices.add(device);
                                    AGO_DEBUG() << "Alarm: add new binarysensor [" << device->getId() << ", " << device->getDevicetype() << "]";
                                    agoConnection->addDevice(device->getId(), device->getDevicetype());
                                }
                                // Manager::Get()->EnablePoll(id);
                            }
                            break;
                        case COMMAND_CLASS_DOOR_LOCK:
                            AGO_DEBUG() << "adding door lock label: " << label;
                            if ((device = devices.findId(nodeinstance)) != NULL)
                            {
                                device->addValue(label, id);
                                device->setDevicetype("doorlock");
                            }
                            else
                            {
                                device = new ZWaveNode(nodeinstance, "doorlock");
                                device->addValue(label, id);
                                devices.add(device);
                                AGO_DEBUG() << "Doorlock: add new doorlock [" << device->getId() << ", " << device->getDevicetype() << "]";
                                agoConnection->addDevice(device->getId(), device->getDevicetype());
                            }
                            break;
                        default:
                            AGO_INFO() << "Notification: Unassigned Value Added Home=" << std::hex << _notification->GetHomeId()
                            << " Node=" << std::dec << (int)_notification->GetNodeId()
                            << " Genre=" << std::dec << (int)id.GetGenre()
                            << " Class=" << getHRCommandClassId(id.GetCommandClassId())
                            << " Instance=" << std::dec << (int)id.GetInstance()
                            << " Index=" << std::dec << (int)id.GetIndex()
                            << " Type=" << (int)id.GetType()
                            << " Label: " << label;

                    }
                }
            }else{
                ValueID id = _notification->GetValueID();
                string label = Manager::Get()->GetValueLabel(id);
                AGO_DEBUG() << "ValueAdded for unknown node Home="
                    << _notification->GetHomeId()
                    << ", Node=" <<  (int)_notification->GetNodeId()
                    << " Label= " << label;
            }
            break;
        }
        case Notification::Type_ValueRemoved:
        {
            if( NodeInfo* nodeInfo = getNodeInfo( _notification ) )
            {
                // Remove the value from out list
                for( list<ValueID>::iterator it = nodeInfo->m_values.begin(); it != nodeInfo->m_values.end(); ++it )
                {
                    if( (*it) == _notification->GetValueID() )
                    {
                        nodeInfo->m_values.erase( it );
                        break;
                    }
                }
            }
            break;
        }

        case Notification::Type_ValueRefreshed:
            AGO_DEBUG() << "Notification='ValueRefreshed'";
            break;

        case Notification::Type_ValueChanged:
        {
            if( /*NodeInfo* nodeInfo =*/ getNodeInfo( _notification ) )
            {
                // One of the node values has changed
                // TBD...
                // nodeInfo = nodeInfo;
                ValueID id = _notification->GetValueID();
                std::string str;
                AGO_INFO() << "Notification='Value Changed' Home=" << std::hex << _notification->GetHomeId()
                    << " Node=" << std::dec << (int)_notification->GetNodeId()
                    << " Genre=" << std::dec << (int)id.GetGenre()
                    << " Class=" << getHRCommandClassId(id.GetCommandClassId()) << std::dec
                    << " Type=" << id.GetType();

                if (Manager::Get()->GetValueAsString(id, &str))
                {
                    ZWaveNode* device = NULL;
                    Json::Value cachedValue = parseToJson(str);
                    valueCache[id] = cachedValue;
                    std::string label = Manager::Get()->GetValueLabel(id);
                    std::string units = Manager::Get()->GetValueUnits(id);

                    // TODO: send proper types and don't convert everything to string
                    std::string level = str;
                    std::string eventtype = "";
                    if (str == "True")
                    {
                        level="255";
                    }
                    if (str == "False")
                    {
                        level="0";
                    }
                    AGO_DEBUG() << "Value='" << str << "' Label='" << label << "' Units=" << units;
                    if ((label == "Basic") || (label == "Switch") || (label == "Level"))
                    {
                        eventtype="event.device.statechanged";
                    }
                    else if (label == "Luminance")
                    {
                        eventtype="event.environment.brightnesschanged";
                    }
                    else if (label == "Ultraviolet")
                    {
                        eventtype="event.environment.ultravioletchanged";
                    }
                    else if (label == "Temperature")
                    {
                        eventtype="event.environment.temperaturechanged";
                        //fix unit
                        if( units=="F" )
                        {
                            units = "degF";
                        }
                        else
                        {
                            units = "degC";
                        }
                        //convert value according to configured metrics
                        if (units=="F" && unitsystem==0)
                        {
                            str = std::to_string((std::stof(str)-32)*5/9);
                            level = str;
                        }
                        else if (units =="C" && unitsystem==1)
                        {
                            str = std::to_string(std::stof(str)*9/5 + 32);
                            level = str;
                        }
                    }
                    else if (label == "Relative Humidity")
                    {
                        eventtype="event.environment.humiditychanged";
                    }
                    else if (label == "Battery Level")
                    {
                        eventtype="event.device.batterylevelchanged";
                    }
                    else if (label == "Alarm Level")
                    {
                        eventtype="event.security.alarmlevelchanged";
                    }
                    else if (label == "Alarm Type")
                    {
                        eventtype="event.security.alarmtypechanged";
                    }
                    else if (label == "Sensor")
                    {
                        eventtype="event.security.sensortriggered";
                    }
                    else if (label == "Energy")
                    {
                        eventtype="event.environment.energychanged";
                    }
                    else if (label == "Power")
                    {
                        eventtype="event.environment.powerchanged";
                    }
                    else if (label == "Mode")
                    {
                        eventtype="event.environment.modechanged";
                    }
                    else if (label == "Fan Mode")
                    {
                        eventtype="event.environment.fanmodechanged";
                    }
                    else if (label == "Fan State")
                    {
                        eventtype="event.environment.fanstatechanged";
                    }
                    else if (label == "Operating State")
                    {
                        eventtype="event.environment.operatingstatechanged";
                    }
                    else if (label == "Cooling 1")
                    {
                        eventtype="event.environment.coolsetpointchanged";
                    }
                    else if (label == "Heating 1")
                    {
                        eventtype="event.environment.heatsetpointchanged";
                    }
                    else if (label == "Fan State")
                    {
                        eventtype="event.environment.fanstatechanged";
                    }
                    else if (label == "Flood")
                    {
                        eventtype="event.security.sensortriggered";
                    }

                    if (eventtype != "")
                    {
                        //search device
                        if( device==NULL )
                        {
                            device = devices.findValue(id);
                        }

                        if (device != NULL)
                        {
                            AGO_DEBUG() << "Sending " << eventtype << " from child " << device->getId();
                            emitFilteredEvent(device->getId().c_str(), eventtype.c_str(), level.c_str(), units.c_str());	
                        }
                    }
                }
            }
            break;
        }
        case Notification::Type_Group:
        {
            if( NodeInfo* nodeInfo = getNodeInfo( _notification ) )
            {
                // One of the node's association groups has changed
                // TBD...
                nodeInfo = nodeInfo;
                eventmap["description"]="Node association added";
                eventmap["state"]="associationchanged";
                eventmap["nodeid"] = _notification->GetNodeId();
                eventmap["homeid"] = _notification->GetHomeId();
                agoConnection->emitEvent("zwavecontroller", "event.zwave.associationchanged", eventmap);
            }
            break;
        }

        case Notification::Type_NodeNew:
        {
            // A brand new node was just beginning to be added.
            // NodeAdded will be sent right after this, but that is sent on zwave init too.
            eventmap["description"]="New node added";
            eventmap["nodeid"] = _notification->GetNodeId();
            eventmap["homeid"] = _notification->GetHomeId();
            agoConnection->emitEvent("zwavecontroller", "event.zwave.nodenew", eventmap);
            break;
        }

        case Notification::Type_NodeAdded:
        {
            // Add the new node to our list
            NodeInfo* nodeInfo = new NodeInfo();
            nodeInfo->m_homeId = _notification->GetHomeId();
            nodeInfo->m_nodeId = _notification->GetNodeId();
            nodeInfo->m_polled = false;
            g_nodes.push_back( nodeInfo );

            // Note: this is sent when agozwave is restarted too!
            // There is also a NodeNew event, but this is only triggered when device is found during startup?
            eventmap["description"]="Node added";
            eventmap["state"]="nodeadded";
            eventmap["nodeid"] = _notification->GetNodeId();
            eventmap["homeid"] = _notification->GetHomeId();
            agoConnection->emitEvent("zwavecontroller", "event.zwave.networkchanged", eventmap);
            break;
        }

        case Notification::Type_NodeRemoved:
        {
            // Remove the node from our list
            uint32 const homeId = _notification->GetHomeId();
            uint8 const nodeId = _notification->GetNodeId();
            eventmap["description"]="Node removed";
            eventmap["state"]="noderemoved";
            eventmap["nodeid"] = _notification->GetNodeId();
            eventmap["homeid"] = _notification->GetHomeId();
            agoConnection->emitEvent("zwavecontroller", "event.zwave.networkchanged", eventmap);
            for( list<NodeInfo*>::iterator it = g_nodes.begin(); it != g_nodes.end(); ++it )
            {
                NodeInfo* nodeInfo = *it;
                if( ( nodeInfo->m_homeId == homeId ) && ( nodeInfo->m_nodeId == nodeId ) )
                {
                    g_nodes.erase( it );
                    break;
                }
            }
            break;
        }

        case Notification::Type_NodeEvent:
        {
            if( /*NodeInfo* nodeInfo =*/ getNodeInfo( _notification ) )
            {
                // We have received an event from the node, caused by a
                // basic_set or hail message.
                ValueID id = _notification->GetValueID();
                AGO_DEBUG() << "NodeEvent: HomeId=" << id.GetHomeId()
                    << " NodeId=" << id.GetNodeId()
                    << " Genre=" << id.GetGenre()
                    << " CommandClassId=" << getHRCommandClassId(id.GetCommandClassId())
                    << " Instance=" << (int)id.GetInstance()
                    << " Index="<< id.GetIndex()
                    << " Type="<< id.GetType()
                    << " Id="<< id.GetId();
                std::string label = Manager::Get()->GetValueLabel(id);
                std::stringstream level;
                level << (int) _notification->GetByte();
                std::string eventtype = "event.device.statechanged";
                ZWaveNode *device = devices.findValue(id);
                if (device != NULL)
                {
                    AGO_DEBUG() << "Sending " << eventtype << " from child " << device->getId();
                    emitFilteredEvent(device->getId().c_str(), eventtype.c_str(), level.str().c_str(), "");
                }
                else
                {
                    AGO_WARNING() << "no agocontrol device found for node event: Label=" << label << " Level=" << level.str();
                }

            }
            break;
        }
        case Notification::Type_SceneEvent:
        {
            if( /*NodeInfo* nodeInfo =*/ getNodeInfo( _notification ) )
            {
                int scene = _notification->GetSceneId();
                std::stringstream tempstream;
                tempstream << (int) _notification->GetNodeId();
                tempstream << "/1";

                std::string nodeinstance = tempstream.str();
                std::string eventtype = "event.device.scenechanged";
                ZWaveNode *device;
                if ((device = devices.findId(nodeinstance)) != NULL)
                {
                    AGO_DEBUG() << "Sending " << eventtype << " for scene " << scene << " event from child " << device->getId();
                    Json::Value content;
                    content["scene"]=scene;
                    agoConnection->emitEvent(device->getId(), eventtype, content);
                }
                else
                {
                    AGO_WARNING() << "no agocontrol device found for scene event: Node=" << nodeinstance << " Scene=" << scene;
                }

            }
            break;
        }
        case Notification::Type_PollingDisabled:
        {
            if( NodeInfo* nodeInfo = getNodeInfo( _notification ) )
            {
                nodeInfo->m_polled = false;
            }
            break;
        }

        case Notification::Type_PollingEnabled:
        {
            if( NodeInfo* nodeInfo = getNodeInfo( _notification ) )
            {
                nodeInfo->m_polled = true;
            }
            break;
        }

        case Notification::Type_DriverReady:
        {
            g_homeId = _notification->GetHomeId();
            break;
        }


        case Notification::Type_DriverFailed:
        {
            g_initFailed = true;
            pthread_cond_broadcast(&initCond);
            break;
        }

        case Notification::Type_AwakeNodesQueried:
        case Notification::Type_AllNodesQueried:
        case Notification::Type_AllNodesQueriedSomeDead:
        {
            pthread_cond_broadcast(&initCond);
            break;
        }
        case Notification::Type_DriverReset:
        case Notification::Type_Notification:
        {
            // XXX: Must not call this for Type_DriverReset
            uint8 _notificationCode = _notification->GetNotification();
            ValueID id = _notification->GetValueID();
            ZWaveNode *device = devices.findValue(id);
            Json::Value eventmap;
            std::stringstream message;
            switch (_notificationCode) {
                case Notification::Code_MsgComplete:
                    //completed message
                    //update lastseen
                    break;
                case Notification::Code_Timeout:
                    AGO_ERROR() << "Z-wave command did time out for nodeid " << (int)_notification->GetNodeId();
                    message << "Z-wave command did time out for nodeid " << (int)_notification->GetNodeId();
                    eventmap["message"] = message.str();
                    if( device )
                    {
                        agoConnection->emitEvent(device->getId(), "event.system.error", eventmap);
                    }
                    break;
                case Notification::Code_NoOperation:
                    break;
                case Notification::Code_Awake:
                    break;
                case Notification::Code_Sleep:
                    break;
                case Notification::Code_Dead:
                    AGO_ERROR() << "Z-wave nodeid " << (int)_notification->GetNodeId() << " is presumed dead!";
                    if( device )
                    {
                        agoConnection->suspendDevice(device->getId());
                    }
                    break;
                case Notification::Code_Alive:
                    AGO_ERROR() << "Z-wave nodeid " << (int)_notification->GetNodeId() << " is alive again";
                    if( device )
                    {
                        agoConnection->resumeDevice(device->getId());
                    }
                    break;
                default:
                    AGO_INFO() << "Z-wave reports an uncatch notification for nodeid " << (int)_notification->GetNodeId();
                    break;
            }
            break;
        }
        case Notification::Type_NodeNaming:
        case Notification::Type_NodeProtocolInfo:
        case Notification::Type_EssentialNodeQueriesComplete:
        case Notification::Type_NodeQueriesComplete:
        {
            if( /*NodeInfo* nodeInfo = */getNodeInfo( _notification ) )
            {
                eventmap["nodeid"] = _notification->GetNodeId();
                eventmap["homeid"] = _notification->GetHomeId();
                eventmap["state"] = getHRNotification(_notification->GetType());

                AGO_DEBUG() << "Sending " << eventmap["state"] << " for node " << eventmap["nodeid"];
                agoConnection->emitEvent("zwavecontroller", "event.zwave.querystage", eventmap);
            }
            break;
        }
        case Notification::Type_DriverRemoved:
            break;
#if HAVE_ZWAVE_VERSION(1,3,397)
        case Notification::Type_ControllerCommand:
            break;
        case Notification::Type_NodeReset:
            break;
#endif
#if HAVE_ZWAVE_VERSION(1,5,208)
        case Notification::Type_UserAlerts:
            break;
#endif
#if HAVE_ZWAVE_VERSION(1,5,214)
        case Notification::Type_ManufacturerSpecificDBReady:
            break;
#endif
        default:
        {
            AGO_WARNING() << "Unhandled OpenZWave Event: " << _notification->GetType();
        }
    }

    pthread_mutex_unlock( &g_criticalSection );
}



Json::Value AgoZwave::commandHandler(const Json::Value& content)
{
    checkMsgParameter(content, "internalid", Json::stringValue);
    checkMsgParameter(content, "command", Json::stringValue);

    std::string internalid = content["internalid"].asString();
    std::string command = content["command"].asString();

    if (internalid == "zwavecontroller")
    {
        AGO_TRACE() << "z-wave specific controller command received";
        if (command == "addnode")
        {
            Manager::Get()->BeginControllerCommand(g_homeId, Driver::ControllerCommand_AddDevice, controller_update, this, true);
            return responseSuccess();
        }
        else if (command == "removenode")
        {
            if (content.isMember("node"))
            {
                checkMsgParameter(content, "node", Json::intValue);
                int mynode = content["node"].asInt();
                Manager::Get()->BeginControllerCommand(g_homeId, Driver::ControllerCommand_RemoveFailedNode, controller_update, this, true, mynode);
            }
            else
            {
                Manager::Get()->BeginControllerCommand(g_homeId, Driver::ControllerCommand_RemoveDevice, controller_update, this, true);
            }
            return responseSuccess();
        }
        else if (command == "healnode")
        {
            checkMsgParameter(content, "node", Json::intValue);
            int mynode = content["node"].asInt();
            Manager::Get()->HealNetworkNode(g_homeId, mynode, true);
            return responseSuccess();
        }
        else if (command == "healnetwork")
        {
            Manager::Get()->HealNetwork(g_homeId, true);
            return responseSuccess();
        }
#if HAVE_ZWAVE_VERSION(1,3,201)
        else if (command == "transferprimaryrole")
        {
            Manager::Get()->TransferPrimaryRole(g_homeId);
            return responseSuccess();
        }
#endif
        else if (command == "refreshnode")
        {
            checkMsgParameter(content, "node", Json::intValue);
            int mynode = content["node"].asInt();
            Manager::Get()->RefreshNodeInfo(g_homeId, mynode);
            return responseSuccess();
        }
        else if (command == "getstatistics")
        {
            Driver::DriverData data;
            Manager::Get()->GetDriverStatistics( g_homeId, &data );
            Json::Value statistics;
            statistics["SOF"] = data.m_SOFCnt;
            statistics["ACK waiting"] = data.m_ACKWaiting;
            statistics["Read Aborts"] = data.m_readAborts;
            statistics["Bad Checksums"] = data.m_badChecksum;
            statistics["Reads"] = data.m_readCnt;
            statistics["Writes"] = data.m_writeCnt;
            statistics["CAN"] = data.m_CANCnt;
            statistics["NAK"] = data.m_NAKCnt;
            statistics["ACK"] = data.m_ACKCnt;
            statistics["OOF"] = data.m_OOFCnt;
            statistics["Dropped"] = data.m_dropped;
            statistics["Retries"] = data.m_retries;
            Json::Value returnval;
            returnval["statistics"]=statistics;
            return responseSuccess(returnval);
        }
        else if (command == "testnode")
        {
            checkMsgParameter(content, "node", Json::intValue);
            int mynode = content["node"].asInt();
            int count = 10;
            if (content.isMember("count"))
            {
                checkMsgParameter(content, "count", Json::intValue);
                count = content["count"].asInt();
            }
            Manager::Get()->TestNetworkNode(g_homeId, mynode, count);
            return responseSuccess();
        }
        else if (command == "testnetwork")
        {
            int count = 10;
            if (content.isMember("count"))
            {
                checkMsgParameter(content, "count", Json::intValue);
                count=content["count"].asInt();
            }
            Manager::Get()->TestNetwork(g_homeId, count);
            return responseSuccess();
        }
        else if (command == "getnodes")
        {
            Json::Value nodelist;
            for( list<NodeInfo*>::iterator it = g_nodes.begin(); it != g_nodes.end(); ++it )
            {
                NodeInfo* nodeInfo = *it;
                std::string index;
                Json::Value node;
                Json::Value neighborsList(Json::arrayValue);
                Json::Value internalIds(Json::arrayValue);
                Json::Value status;
                Json::Value params(Json::arrayValue);

                uint8* neighbors;
                uint32 numNeighbors = Manager::Get()->GetNodeNeighbors(nodeInfo->m_homeId,nodeInfo->m_nodeId,&neighbors);
                if (numNeighbors)
                {
                    for(uint32 i=0; i<numNeighbors; i++)
                    {
                        neighborsList.append(neighbors[i]);
                    }
                    delete [] neighbors;
                }
                node["neighbors"]=neighborsList;

                for (list<ValueID>::iterator it2 = (*it)->m_values.begin(); it2 != (*it)->m_values.end(); it2++ )
                {
                    OpenZWave::ValueID currentValueID = *it2;
                    ZWaveNode *device = devices.findValue(currentValueID);
                    if (device != NULL)
                    {
                        // Avoid dups
                        if(std::find(internalIds.begin(), internalIds.end(), device->getId()) == internalIds.end())
                            internalIds.append(device->getId());
                    }

                    //get node specific parameters
                    uint8_t commandClassId = it2->GetCommandClassId();
                    if( commandClassId==COMMAND_CLASS_CONFIGURATION )
                    {
                        Json::Value param = getCommandClassConfigurationParameter(&currentValueID);
                        if( param.size()>0 )
                        {
                            params.append(param);
                        }
                    }
                    else if( commandClassId==COMMAND_CLASS_WAKE_UP )
                    {
                        //need to know wake up interval
                        if( currentValueID.GetGenre()==ValueID::ValueGenre_System && currentValueID.GetInstance()==1 )
                        {
                            if( Manager::Get()->GetValueLabel(currentValueID)=="Wake-up Interval" )
                            {
                                Json::Value param = getCommandClassWakeUpParameter(&currentValueID);
                                if( param.size()>0 )
                                {
                                    params.append(param);
                                }
                            }
                        }
                    }
                }
                node["internalids"] = internalIds;

                node["manufacturer"]=Manager::Get()->GetNodeManufacturerName(nodeInfo->m_homeId,nodeInfo->m_nodeId);
                node["version"]=Manager::Get()->GetNodeVersion(nodeInfo->m_homeId,nodeInfo->m_nodeId);
                node["basic"]=Manager::Get()->GetNodeBasic(nodeInfo->m_homeId,nodeInfo->m_nodeId);
                node["generic"]=Manager::Get()->GetNodeGeneric(nodeInfo->m_homeId,nodeInfo->m_nodeId);
                node["specific"]=Manager::Get()->GetNodeSpecific(nodeInfo->m_homeId,nodeInfo->m_nodeId);
                node["product"]=Manager::Get()->GetNodeProductName(nodeInfo->m_homeId,nodeInfo->m_nodeId);
                node["type"]=Manager::Get()->GetNodeType(nodeInfo->m_homeId,nodeInfo->m_nodeId);
                node["producttype"]=Manager::Get()->GetNodeProductType(nodeInfo->m_homeId,nodeInfo->m_nodeId);
                node["numgroups"]=Manager::Get()->GetNumGroups(nodeInfo->m_homeId,nodeInfo->m_nodeId);

                //fill node status
                status["querystage"]=Manager::Get()->GetNodeQueryStage(nodeInfo->m_homeId,nodeInfo->m_nodeId);
                status["awake"]=(Manager::Get()->IsNodeAwake(nodeInfo->m_homeId,nodeInfo->m_nodeId) ? 1 : 0);
                status["listening"]=(Manager::Get()->IsNodeListeningDevice(nodeInfo->m_homeId,nodeInfo->m_nodeId) || 
                        Manager::Get()->IsNodeFrequentListeningDevice(nodeInfo->m_homeId,nodeInfo->m_nodeId) ? 1 : 0);
                status["failed"]=(Manager::Get()->IsNodeFailed(nodeInfo->m_homeId,nodeInfo->m_nodeId) ? 1 : 0);
                node["status"]=status;

                //fill node parameters
                node["params"] = params;

                uint8 nodeid = nodeInfo->m_nodeId;
                index = std::to_string(nodeid);
                nodelist[index.c_str()] = node;
            }
            Json::Value returnval;
            returnval["nodelist"]=nodelist;
            return responseSuccess(returnval);
        }
        else if (command == "addassociation")
        {
            checkMsgParameter(content, "node", Json::intValue);
            checkMsgParameter(content, "group", Json::intValue);
            checkMsgParameter(content, "target", Json::intValue);
            int mynode = content["node"].asInt();
            int mygroup = content["group"].asInt();
            int mytarget = content["target"].asInt();
            AGO_DEBUG() << "adding association: Node=" << mynode << " Group=" << mygroup << " Target=" << mytarget;
            Manager::Get()->AddAssociation(g_homeId, mynode, mygroup, mytarget);
            return responseSuccess();
        }
        else if (command == "getassociations")
        {
            checkMsgParameter(content, "node", Json::intValue);
            checkMsgParameter(content, "group", Json::intValue);
            Json::Value associationsmap;
            int mygroup = content["group"].asInt();
            int mynode = content["node"].asInt();
            uint8_t *associations;
            uint32_t numassoc = Manager::Get()->GetAssociations(g_homeId, mynode, mygroup, &associations);
            for (uint32_t assoc = 0; assoc < numassoc; assoc++)
            {
                associationsmap[std::to_string(assoc)] = associations[assoc];
            }
            if (numassoc >0)
                delete associations;

            Json::Value returnval;
            returnval["associations"] = associationsmap;
            returnval["label"] = Manager::Get()->GetGroupLabel(g_homeId, mynode, mygroup);
            return responseSuccess(returnval);
        }
        else if (command == "getallassociations")
        {
            checkMsgParameter(content, "node", Json::intValue);
            int mynode = content["node"].asInt();
            int numGroups = Manager::Get()->GetNumGroups(g_homeId, mynode);

            Json::Value groups(Json::arrayValue);
            for(int group=1; group <= numGroups; group++) {
                Json::Value g;
                uint8_t *associations;
                uint32_t numassoc = Manager::Get()->GetAssociations(g_homeId, mynode, group, &associations);

                Json::Value associationsList(Json::arrayValue);
                for (uint32_t assoc = 0; assoc < numassoc; assoc++)
                    associationsList.append(associations[assoc]);

                if (numassoc >0)
                    delete[] associations;

                g["group"] = group;
                g["associations"] = associationsList;
                g["label"] = Manager::Get()->GetGroupLabel(g_homeId, mynode, group);

                groups.append(g);
            }

            Json::Value returnval;
            returnval["groups"] = groups;
            return responseSuccess(returnval);
        }
        else if (command == "removeassociation")
        {
            checkMsgParameter(content, "node", Json::intValue);
            checkMsgParameter(content, "group", Json::intValue);
            checkMsgParameter(content, "target", Json::intValue);
            int mynode = content["node"].asInt();
            int mygroup = content["group"].asInt();
            int mytarget = content["target"].asInt();
            AGO_DEBUG() << "removing association: Node=" << mynode << " Group=" << mygroup << " Target=" << mytarget;
            Manager::Get()->RemoveAssociation(g_homeId, mynode, mygroup, mytarget);
            return responseSuccess();
        }
        else if (command == "setconfigparam")
        {
            checkMsgParameter(content, "node", Json::intValue);
            checkMsgParameter(content, "index", Json::intValue);
            int nodeId = content["node"].asInt();
            int index = content["index"].asInt();
            if (!content.isMember("commandclassid"))
            {
                // old-fashioned direct setconfigparam
                checkMsgParameter(content, "value", Json::intValue);
                checkMsgParameter(content, "size", Json::intValue);
                int size = content["size"].asInt();
                int value = content["value"].asInt();

                AGO_DEBUG() << "setting config param: nodeId=" << nodeId << " size=" << size << " index=" << index << " value=" << value;
                Manager::Get()->SetConfigParam(g_homeId, nodeId, index, value, size);
                return responseSuccess();
            }
            else
            {
                // new style used by the GUI
                checkMsgParameter(content, "value", Json::stringValue);
                checkMsgParameter(content, "commandclassid", Json::intValue);
                std::string value = content["value"].asString();
                int commandClassId = content["commandclassid"].asInt();
                AGO_DEBUG() << "setting config param: nodeId=" << nodeId << " commandClassId=" << commandClassId << " index=" << index << " value=" << value;
                if (setCommandClassParameter(g_homeId, nodeId, commandClassId, index, value))
                    return responseSuccess();
                else return responseError(RESPONSE_ERR_INTERNAL, "Cannot set command class parameter");
            }
        }
        else if( command=="requestallconfigparams" )
        {
            checkMsgParameter(content, "node", Json::intValue);
            int node = content["node"].asInt();
            AGO_DEBUG() << "RequestAllConfigParams: node=" << node;
            Manager::Get()->RequestAllConfigParams(g_homeId, node);
            return responseSuccess();
        }
        else if (command == "downloadconfig")
        {
            if (Manager::Get()->BeginControllerCommand(g_homeId, Driver::ControllerCommand_ReceiveConfiguration, controller_update, NULL, true))
            {
                return responseSuccess();
            } else return responseError(RESPONSE_ERR_INTERNAL, "Cannot download z-wave config to controller");
        }
        else if (command == "cancel")
        {
            Manager::Get()->CancelControllerCommand(g_homeId);
            return responseSuccess();
        }
        else if (command == "saveconfig")
        {
            Manager::Get()->WriteConfig( g_homeId );
            return responseSuccess();
        }
        else if (command == "allon")
        {
            Manager::Get()->SwitchAllOn(g_homeId );
            return responseSuccess();
        }
        else if (command == "alloff")
        {
            Manager::Get()->SwitchAllOff(g_homeId );
            return responseSuccess();
        }
        else if (command == "reset")
        {
            Manager::Get()->ResetController(g_homeId);
            return responseSuccess();
        }
        else if (command == "enablepolling")
        {
            checkMsgParameter(content, "nodeid", Json::stringValue);
            checkMsgParameter(content, "value", Json::stringValue);
            checkMsgParameter(content, "intensity", Json::intValue);

            AGO_DEBUG() << "Enable polling for " << content["nodeid"] << " Value " << content["value"] << " with intensity " << content["intensity"];
            ZWaveNode *device = devices.findId(content["nodeid"].asString());
            if (device != NULL)
            {
                ValueID *tmpValueID = NULL;
                tmpValueID = device->getValueID(content["value"].asString());
                if (tmpValueID == NULL)
                    return responseError(RESPONSE_ERR_INTERNAL, "Value label '"+content["value"].asString()+"' not found for device " + internalid);
                AGO_DEBUG() << "Enable polling for " << content["nodeid"] << " Value " << content["value"] << " with intensity " << content["intensity"];
                Manager::Get()->EnablePoll(*tmpValueID, content["intensity"].asInt());
                Manager::Get()->WriteConfig( g_homeId );
                return responseSuccess();
            } else 
            {
                return responseError(RESPONSE_ERR_INTERNAL, "Cannot find device");
            }
        }
        else if (command == "disablepolling")
        {
            checkMsgParameter(content, "nodeid", Json::stringValue);
            checkMsgParameter(content, "value", Json::stringValue);

            ZWaveNode *device = devices.findId(content["nodeid"].asString());
            if (device != NULL)
            {
                ValueID *tmpValueID = NULL;
                tmpValueID = device->getValueID(content["value"].asString());
                if (tmpValueID == NULL)
                    return responseError(RESPONSE_ERR_INTERNAL, "Value label '"+content["value"].asString()+"' not found for device " + internalid);
                AGO_DEBUG() << "Disable polling for " << content["nodeid"] << " Value " << content["value"];
                Manager::Get()->DisablePoll(*tmpValueID);
                Manager::Get()->WriteConfig( g_homeId );
                return responseSuccess();
            } else 
            {
                return responseError(RESPONSE_ERR_INTERNAL, "Cannot find device");
            }
        }
        else
        {
            return responseUnknownCommand();
        }
    }
    else
    {
        ZWaveNode *device = devices.findId(internalid);
        if (device != NULL)
        {
            std::string devicetype = device->getDevicetype();
            AGO_TRACE() << "command received for " << internalid << "(" << devicetype << ")"; 
            ValueID *tmpValueID = NULL;

            if (devicetype == "switch")
            {
                tmpValueID = device->getValueID("Switch");
                if (tmpValueID == NULL) return responseError(RESPONSE_ERR_INTERNAL, "Cannot determine OpenZWave 'Switch' label");
                if (command == "on" )
                {
                    if (Manager::Get()->SetValue(*tmpValueID , true)) return responseSuccess();
                    else return responseError(RESPONSE_ERR_INTERNAL, "Cannot set OpenZWave device value");
                }
                else if (command == "off" )
                {
                    if (Manager::Get()->SetValue(*tmpValueID , false)) return responseSuccess();
                    else return responseError(RESPONSE_ERR_INTERNAL, "Cannot set OpenZWave device value");
                }
            }
            else if(devicetype == "dimmer")
            {
                tmpValueID = device->getValueID("Level");
                if (tmpValueID == NULL) return responseError(RESPONSE_ERR_INTERNAL, "Cannot determine OpenZWave 'Level' label");
                if (command == "on" )
                {
                    if (Manager::Get()->SetValue(*tmpValueID , (uint8) 255)) return responseSuccess();
                    else return responseError(RESPONSE_ERR_INTERNAL, "Cannot set OpenZWave device value");
                }
                else if (command == "setlevel")
                {
                    checkMsgParameter(content, "level", Json::intValue);
                    uint8 level = content["level"].asInt();
                    if (level > 99) level=99;
                    if (Manager::Get()->SetValue(*tmpValueID, level)) return responseSuccess();
                    else return responseError(RESPONSE_ERR_INTERNAL, "Cannot set OpenZWave device value");
                }
                else if (command == "off" )
                {
                    if (Manager::Get()->SetValue(*tmpValueID , (uint8) 0)) return responseSuccess();
                    else return responseError(RESPONSE_ERR_INTERNAL, "Cannot set OpenZWave device value");
                }
            }
            else if(devicetype == "dimmerrgb")
            {
                if (command == "on" )
                {
                    tmpValueID = device->getValueID("Level");
                    if (tmpValueID == NULL) return responseError(RESPONSE_ERR_INTERNAL, "Cannot determine OpenZWave 'Level' label");
                    if (Manager::Get()->SetValue(*tmpValueID , (uint8) 255)) return responseSuccess();
                    else return responseError(RESPONSE_ERR_INTERNAL, "Cannot set OpenZWave device value");
                }
                else if (command == "setlevel")
                {
                    tmpValueID = device->getValueID("Level");
                    if (tmpValueID == NULL) return responseError(RESPONSE_ERR_INTERNAL, "Cannot determine OpenZWave 'Level' label");
                    checkMsgParameter(content, "level", Json::uintValue);
                    uint8 level = content["level"].asUInt();
                    if (level > 99) level=99;
                    if (Manager::Get()->SetValue(*tmpValueID, level)) return responseSuccess();
                    else return responseError(RESPONSE_ERR_INTERNAL, "Cannot set OpenZWave device value");
                }
                else if (command == "setcolor")
                {
                    tmpValueID = device->getValueID("Color");
                    if (tmpValueID == NULL) return responseError(RESPONSE_ERR_INTERNAL, "Cannot determine OpenZWave 'Color' label");
                    checkMsgParameter(content, "red", Json::intValue);
                    checkMsgParameter(content, "green", Json::intValue);
                    checkMsgParameter(content, "blue", Json::intValue);

                    int red = CAP_8BIT_INT(content["red"].asInt());
                    int green = CAP_8BIT_INT(content["green"].asInt());
                    int blue = CAP_8BIT_INT(content["blue"].asInt());

                    std::stringstream colorString;
                    colorString << "#";
                    colorString << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << red;
                    colorString << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << green;
                    colorString << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << blue;
                    colorString << "00";
                    colorString << "00";
                    AGO_DEBUG() << "setting color string: " << colorString.str();
                    if (Manager::Get()->SetValue(*tmpValueID, colorString.str())) return responseSuccess();
                    else return responseError(RESPONSE_ERR_INTERNAL, "Cannot set OpenZWave device color value");
                }
                else if (command == "off" )
                {
                    tmpValueID = device->getValueID("Level");
                    if (tmpValueID == NULL) return responseError(RESPONSE_ERR_INTERNAL, "Cannot determine OpenZWave 'Level' label");
                    if (Manager::Get()->SetValue(*tmpValueID , (uint8) 0)) return responseSuccess();
                    else return responseError(RESPONSE_ERR_INTERNAL, "Cannot set OpenZWave device value");
                }
            }
            else if (devicetype == "drapes")
            {
                if (command == "on")
                {
                    tmpValueID = device->getValueID("Level");
                    if (tmpValueID == NULL) return responseError(RESPONSE_ERR_INTERNAL, "Cannot determine OpenZWave 'Level' label");
                    if (Manager::Get()->SetValue(*tmpValueID , (uint8) 255)) return responseSuccess();
                    else return responseError(RESPONSE_ERR_INTERNAL, "Cannot set OpenZWave device value");
                }
                else if (command == "open" )
                {
                    tmpValueID = device->getValueID("Open");
                    if (tmpValueID == NULL) return responseError(RESPONSE_ERR_INTERNAL, "Cannot determine OpenZWave 'Open' label");
                    if (Manager::Get()->SetValue(*tmpValueID , true)) return responseSuccess();
                    else return responseError(RESPONSE_ERR_INTERNAL, "Cannot set OpenZWave device value");
                }
                else if (command == "close" )
                {
                    tmpValueID = device->getValueID("Close");
                    if (tmpValueID == NULL) return responseError(RESPONSE_ERR_INTERNAL, "Cannot determine OpenZWave 'Close' label");
                    if (Manager::Get()->SetValue(*tmpValueID , true)) return responseSuccess();
                    else return responseError(RESPONSE_ERR_INTERNAL, "Cannot set OpenZWave device value");
                }
                else if (command == "stop" )
                {
                    tmpValueID = device->getValueID("Stop");
                    if (tmpValueID == NULL) return responseError(RESPONSE_ERR_INTERNAL, "Cannot determine OpenZWave 'Stop' label");
                    if (Manager::Get()->SetValue(*tmpValueID , true)) return responseSuccess();
                    else return responseError(RESPONSE_ERR_INTERNAL, "Cannot set OpenZWave device value");

                }
                else if (command == "off" )
                {
                    tmpValueID = device->getValueID("Level");
                    if (tmpValueID == NULL) return responseError(RESPONSE_ERR_INTERNAL, "Cannot determine OpenZWave 'Level' label");
                    if (Manager::Get()->SetValue(*tmpValueID , (uint8) 0)) return responseSuccess();
                    else return responseError(RESPONSE_ERR_INTERNAL, "Cannot set OpenZWave device value");
                }
            }
            else if (devicetype == "thermostat")
            {
                if (command == "settemperature")
                {
                    // XXX: Should do realValue instead
                    checkMsgParameter(content, "temperature", Json::stringValue);

                    std::string mode = content["mode"].asString();
                    if  (mode == "") mode = "auto";
                    if (mode == "cool") tmpValueID = device->getValueID("Cooling 1");
                    else if ((mode == "auto") || (mode == "heat")) tmpValueID = device->getValueID("Heating 1");
                    if (tmpValueID == NULL) return responseError(RESPONSE_ERR_INTERNAL, "Cannot determine OpenZWave label for thermostat mode");

                    float temp = 0.0;
                    if (content["temperature"] == "-1")
                    {
                        try
                        {
                            AGO_TRACE() << "rel temp -1:" << valueCache[*tmpValueID];
                            temp = valueCache[*tmpValueID].asFloat();
                            temp = temp - 1.0;
                        }
                        catch (...)
                        {
                            AGO_WARNING() << "can't determine current value for relative temperature change";
                            return responseError(RESPONSE_ERR_INTERNAL, "Cannot determine current  value for relative temperature change");
                        }
                    }
                    else if (content["temperature"] == "+1")
                    {
                        try
                        {
                            AGO_TRACE() << "rel temp +1: " << valueCache[*tmpValueID];
                            temp = valueCache[*tmpValueID].asFloat();
                            temp = temp + 1.0;
                        }
                        catch (...)
                        {
                            AGO_WARNING() << "can't determine current value for relative temperature change";
                            return responseError(RESPONSE_ERR_INTERNAL, "Cannot determine current  value for relative temperature change");
                        }
                    }
                    else
                    {
                        temp = std::stof(content["temperature"].asString());
                    }
                    AGO_TRACE() << "setting temperature: " << temp;
                    if (Manager::Get()->SetValue(*tmpValueID, temp)) return responseSuccess();
                    else return responseError(RESPONSE_ERR_INTERNAL, "Cannot set OpenZWave device value");
                }
                else if (command == "setthermostatmode")
                {
                    std::string mode = content["mode"].asString();
                    tmpValueID = device->getValueID("Mode");
                    if (tmpValueID == NULL) return responseError(RESPONSE_ERR_INTERNAL, "Cannot determine OpenZWave 'Mode' label");
                    if (mode=="heat")
                    {
                        if (Manager::Get()->SetValue(*tmpValueID , "Heat")) return responseSuccess();
                        else return responseError(RESPONSE_ERR_INTERNAL, "Cannot set OpenZWave device value");
                    }
                    else if (mode=="cool")
                    {
                        if (Manager::Get()->SetValue(*tmpValueID , "Cool")) return responseSuccess();
                        else return responseError(RESPONSE_ERR_INTERNAL, "Cannot set OpenZWave device value");
                    }
                    else if (mode == "off")
                    {
                        if (Manager::Get()->SetValue(*tmpValueID , "Off")) return responseSuccess();
                        else return responseError(RESPONSE_ERR_INTERNAL, "Cannot set OpenZWave device value");
                    }
                    else if (mode == "auxheat")
                    {
                        if (Manager::Get()->SetValue(*tmpValueID , "Aux Heat")) return responseSuccess();
                        else return responseError(RESPONSE_ERR_INTERNAL, "Cannot set OpenZWave device value");
                    }
                    else
                    {
                        if (Manager::Get()->SetValueListSelection(*tmpValueID , "Auto")) return responseSuccess();
                        else return responseError(RESPONSE_ERR_INTERNAL, "Cannot set OpenZWave device value");
                    }
                }
                else if (command == "setthermostatfanmode")
                {
                    std::string mode = content["mode"].asString();
                    tmpValueID = device->getValueID("Fan Mode");
                    if (tmpValueID == NULL) return responseError(RESPONSE_ERR_INTERNAL, "Cannot determine OpenZWave 'Mode' label");
                    if (mode=="circulate")
                    {
                        if (Manager::Get()->SetValueListSelection(*tmpValueID , "Circulate")) return responseSuccess();
                        else return responseError(RESPONSE_ERR_INTERNAL, "Cannot set OpenZWave device value");
                    }
                    else if (mode=="on" || mode=="onlow")
                    {
                        if (Manager::Get()->SetValueListSelection(*tmpValueID , "On Low")) return responseSuccess();
                        else return responseError(RESPONSE_ERR_INTERNAL, "Cannot set OpenZWave device value");
                    }
                    else if (mode=="onhigh")
                    {
                        if (Manager::Get()->SetValueListSelection(*tmpValueID , "On High")) return responseSuccess();
                        else return responseError(RESPONSE_ERR_INTERNAL, "Cannot set OpenZWave device value");
                    }
                    else if (mode=="autohigh")
                    {
                        if (Manager::Get()->SetValueListSelection(*tmpValueID , "Auto High")) return responseSuccess();
                        else return responseError(RESPONSE_ERR_INTERNAL, "Cannot set OpenZWave device value");
                    }
                    else
                    {
                        if (Manager::Get()->SetValueListSelection(*tmpValueID , "Auto Low")) return responseSuccess();
                        else return responseError(RESPONSE_ERR_INTERNAL, "Cannot set OpenZWave device value");
                    }
                }
            } else if (devicetype == "doorlock") {
                if (command == "open")
                {
                    tmpValueID = device->getValueID("Locked");
                    if (tmpValueID == NULL) return responseError(RESPONSE_ERR_INTERNAL, "Cannot determine OpenZWave 'Locked' label");
                    if (Manager::Get()->SetValue(*tmpValueID , true)) return responseSuccess();
                    else return responseError(RESPONSE_ERR_INTERNAL, "Cannot set OpenZWave device value");
                } else if (command == "close")
                {
                    tmpValueID = device->getValueID("Locked");
                    if (tmpValueID == NULL) return responseError(RESPONSE_ERR_INTERNAL, "Cannot determine OpenZWave 'Locked' label");
                    if (Manager::Get()->SetValue(*tmpValueID , false)) return responseSuccess();
                    else return responseError(RESPONSE_ERR_INTERNAL, "Cannot set OpenZWave device value");

                }
            }

            AGO_WARNING() << "Recieved unknown command for device " << content << " of type " << devicetype;
        }
        else
            AGO_WARNING() << "Recieved command for unknown device " << content;
    }
    return responseUnknownCommand();
}

void AgoZwave::setupApp()
{
    std::string device;

    device=getConfigOption("device", "/dev/usbzwave");
    if (getConfigSectionOption("system", "units", "SI") != "SI")
    {
        unitsystem=1;
    }

    pthread_mutexattr_t mutexattr;

    pthread_mutexattr_init ( &mutexattr );
    pthread_mutexattr_settype( &mutexattr, PTHREAD_MUTEX_RECURSIVE );

    pthread_mutex_init( &g_criticalSection, &mutexattr );
    pthread_mutexattr_destroy( &mutexattr );

    pthread_mutex_lock( &initMutex );

    AGO_INFO() << "Starting OZW driver ver " << Manager::getVersionAsString();
    // init open zwave
    MyLog *myLog = new MyLog;
    // OpenZWave::Log* pLog = OpenZWave::Log::Create(myLog);
    OpenZWave::Log::SetLoggingClass(myLog);
    // OpenZWave::Log* pLog = OpenZWave::Log::Create("/var/log/zwave.log", true, false, OpenZWave::LogLevel_Info, OpenZWave::LogLevel_Debug, OpenZWave::LogLevel_Error);
    // pLog->SetLogFileName("/var/log/zwave.log"); // Make sure, in case Log::Create already was called before we got here
    // pLog->SetLoggingState(OpenZWave::LogLevel_Info, OpenZWave::LogLevel_Debug, OpenZWave::LogLevel_Error);

    if(Options::Create( "/etc/openzwave/", getConfigPath("/ozw/").string(), "" ) == NULL)
    {
        AGO_ERROR() << "Failed to configure OpenZWave";
        throw StartupError();
    }
    if (getConfigOption("returnroutes", "true")=="true")
    {
        Options::Get()->AddOptionBool("PerformReturnRoutes", false );
    }
    Options::Get()->AddOptionBool("ConsoleOutput", true ); 
    if (getConfigOption("sis", "true")=="true")
    {
        Options::Get()->AddOptionBool("EnableSIS", true ); 
    }
    if (getConfigOption("assumeawake", "false")=="false")
    {
        Options::Get()->AddOptionBool("AssumeAwake", false ); 
    }
    if (getConfigOption("validatevaluechanges", "false")=="true")
    {
        Options::Get()->AddOptionBool("ValidateValueChanges", true);
    }

    Options::Get()->AddOptionInt( "SaveLogLevel", LogLevel_Debug );
    Options::Get()->AddOptionInt( "QueueLogLevel", LogLevel_Debug );
    Options::Get()->AddOptionInt( "DumpTrigger", LogLevel_Error );

    int retryTimeout = std::stoi(getConfigOption("retrytimeout","2000"));
    OpenZWave::Options::Get()->AddOptionInt("RetryTimeout", retryTimeout);

    Options::Get()->Lock();

    Manager::Create();
    Manager::Get()->AddWatcher( on_notification, this );
    Manager::Get()->SetPollInterval(std::stoi(getConfigOption("pollinterval", "300000")),true);
    Manager::Get()->AddDriver(device);

    // Now we just wait for the driver to become ready
    AGO_INFO() << "waiting for OZW driver to become ready";
    pthread_cond_wait( &initCond, &initMutex );

    if( !g_initFailed )
    {
        Manager::Get()->WriteConfig( g_homeId );
        Driver::DriverData data;
        Manager::Get()->GetDriverStatistics( g_homeId, &data );

        //get device parameters
        requestAllNodeConfigParameters();

        AGO_DEBUG() << "\n\n\n=================================";
        AGO_INFO() << "OZW startup complete";
        AGO_DEBUG() << devices.toString();

        agoConnection->addDevice("zwavecontroller", "zwavecontroller");
        addCommandHandler();
    }
    else
    {
        AGO_FATAL() << "unable to initialize OZW";
        Manager::Destroy();
        pthread_mutex_destroy( &g_criticalSection );
        throw StartupError();
    }
}

void AgoZwave::cleanupApp() {
    Manager::Destroy();
    pthread_mutex_destroy( &g_criticalSection );
}

AGOAPP_ENTRY_POINT(AgoZwave);
