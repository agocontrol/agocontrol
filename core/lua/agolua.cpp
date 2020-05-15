#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <sstream>
#include <cerrno>

#include <boost/thread/mutex.hpp>
#include <boost/thread/locks.hpp>
#include "boost/filesystem.hpp"

#include "boost/regex.hpp"

#include "lua.hpp"

#ifndef SCRIPTSINFOSFILE
#define SCRIPTSINFOSFILE "maps/agolua.json"
#endif

#ifndef LUA_SCRIPT_DIR
#define LUA_SCRIPT_DIR "lua"
#endif

#include "base64.h"

#include "agoapp.h"

namespace fs = ::boost::filesystem;

using namespace agocontrol;

#define INVENTORY_MAX_AGE 60 //in seconds

const char reAll[] = "--.*--\\[\\[(.*)\\]\\](.*)";
const char reEvent[] = "(event\\.\\w+\\.\\w+)";

enum DebugMsgType {
    DBG_START = 0, //display start message
    DBG_END, //display end message
    DBG_ERROR, //display error message
    DBG_INFO //display info message
};

class AgoLua: public AgoApp {
private:
    Json::Value inventory;
    Json::Value scriptsInfos;
    boost::regex exprAll;
    boost::regex exprEvent;
    std::string agocontroller;
    int filterByEvents;
    fs::path scriptdir;
    Json::Value scriptContexts;
    boost::mutex mutexInventory;
    boost::mutex mutexScriptInfos;
    boost::mutex mutexScriptContexts;
    int64_t lastInventoryUpdate;

    fs::path construct_script_name(fs::path input) ;
    void pushTableFromJson(lua_State *L, const Json::Value& content) ;
    void pullTableToMap(lua_State *L, Json::Value& table);
    void pullTableToList(lua_State *L, Json::Value& list);

    void updateInventory();
    void searchEvents(const fs::path& scriptPath, Json::Value& foundEvents) ;
    void purgeScripts() ;
    void initScript(lua_State* L, Json::Value& content, const std::string& script, Json::Value& context, bool debug);
    void finalizeScript(lua_State* L, Json::Value& content, const std::string& script, Json::Value& context);
    void debugScript(Json::Value content, const std::string script);
    void executeScript(Json::Value content, const fs::path &script);
    bool canExecuteScript(const Json::Value& content, const fs::path &script);
    Json::Value commandHandler(const Json::Value& content) ;
    void eventHandler(const std::string& subject , const Json::Value& content) ;
    bool enableScript(const std::string& script, bool enabled);

    void setupApp();

public:
    AGOAPP_CONSTRUCTOR_HEAD(AgoLua)
        , exprAll(reAll)
        , exprEvent(reEvent)
        , filterByEvents(1)
        {}

    // called from global static wrapper functions, thus public
    int luaSendMessage(lua_State *l);
    int luaSetVariable(lua_State *L);
    int luaGetVariable(lua_State *L);
    int luaGetDeviceInventory(lua_State *L);
    int luaGetInventory(lua_State *L);
    int luaPause(lua_State *L);
    int luaDebugPause(lua_State *L);
    int luaGetDeviceName(lua_State *L);
    int luaDebugPrint(lua_State* L);
};


#define LUA_WRAPPER(method_name) \
    static int method_name ## _wrapper(lua_State *l) { \
        AgoLua *inst = (AgoLua*) lua_touserdata(l, lua_upvalueindex(1));\
        return inst->method_name(l);\
    }

LUA_WRAPPER(luaSendMessage);
LUA_WRAPPER(luaSetVariable);
LUA_WRAPPER(luaGetVariable);
LUA_WRAPPER(luaGetDeviceInventory);
LUA_WRAPPER(luaGetInventory);
LUA_WRAPPER(luaPause);
LUA_WRAPPER(luaDebugPause);
LUA_WRAPPER(luaGetDeviceName);
LUA_WRAPPER(luaDebugPrint);

const luaL_Reg loadedlibs[] = {
    {"_G", luaopen_base},
    {LUA_TABLIBNAME, luaopen_table},
    {LUA_STRLIBNAME, luaopen_string},
    {LUA_MATHLIBNAME, luaopen_math},
    {NULL, NULL}
};

fs::path AgoLua::construct_script_name(fs::path input)
{
    fs::path out = (scriptdir / input);
    // replace == add/append
    out.replace_extension(".lua");
    return out;
}

// read file into string. credits go to "insane coder" - http://stackoverflow.com/questions/2602013/read-whole-ascii-file-into-c-stdstring
static std::string get_file_contents(const fs::path &filename)
{
    std::ifstream in(filename.string(), std::ios::in | std::ios::binary);
    if (in)
    {
        std::string contents;
        in.seekg(0, std::ios::end);
        contents.resize(in.tellg());
        in.seekg(0, std::ios::beg);
        in.read(&contents[0], contents.size());
        in.close();
        return(contents);
    }

    throw fs::filesystem_error( "reading file", filename, boost::system::error_code(errno, boost::system::system_category()) );
}

/**
 * Push lua table from Json object
 */
void AgoLua::pushTableFromJson(lua_State *L, const Json::Value& content)
{
    lua_createtable(L, 0, 0);
    for (auto it = content.begin(); it != content.end(); it++)
    {
        std::string key = it.name();
        switch (it->type())
        {
            case Json::intValue:
                AGO_TRACE() << "Push '" << key << "' as int";
                lua_pushstring(L,key.c_str());
                lua_pushnumber(L,it->asInt());
                lua_settable(L, -3);
                break;
            case Json::uintValue:
                AGO_TRACE() << "Push '" << key << "' as uint";
                lua_pushstring(L,key.c_str());
                lua_pushnumber(L,it->asUInt());
                lua_settable(L, -3);
                break;
            case Json::realValue:
                AGO_TRACE() << "Push '" << key << "' as double";
                lua_pushstring(L,key.c_str());
                lua_pushnumber(L,it->asDouble());
                lua_settable(L, -3);
                break;
            case Json::stringValue:
                AGO_TRACE() << "Push '" << key << "' as string";
                lua_pushstring(L,key.c_str());
                lua_pushstring(L,it->asString().c_str());
                lua_settable(L, -3);
                break;
            case Json::objectValue:
                AGO_TRACE() << "Push '" << key << "' as object";
                lua_pushstring(L,key.c_str());
                pushTableFromJson(L,*it);
                lua_settable(L, -3);
                break;
            case Json::booleanValue:
                AGO_TRACE() << "Push '" << key << "' as bool";
                lua_pushstring(L,key.c_str());
                lua_pushboolean(L,it->asBool());
                lua_settable(L, -3);
                break;
            case Json::nullValue:
                AGO_TRACE() << "Push '" << key << "' as null";
                lua_pushstring(L,key.c_str());
                lua_pushnil(L);
                lua_settable(L, -3);
                break;
            case Json::arrayValue:
                AGO_TRACE() << "Push '" << key << "' as array";
                lua_pushstring(L, key.c_str());
                pushTableFromJson(L, *it);
                lua_settable(L, -3);
                break;
            default:
                AGO_WARNING() << "Push unsupported value type to map. Value dropped from map.";
        }
    }
} 


/**
 * Fill specified cpp variant map with lua table content
 * Before calling this function you need to call lua_getglobal(<lua obj>, <lua table name>)
 */
void AgoLua::pullTableToMap(lua_State *L, Json::Value& table)
{
    lua_pushnil(L);
    while( lua_next(L, -2)!=0 )
    {
        std::string key = lua_tostring(L, -2);
        if( lua_isstring(L, -1) )
        {
            AGO_TRACE() << "Pull '" << key << "' as STRING";
            table[key] = lua_tostring(L, -1);
        }
        else if( lua_isnumber(L, -1) )
        {
            AGO_TRACE() << "Pull '" << key << "' as NUMBER";
            table[key] = lua_tonumber(L, -1);
        }
        else if( lua_isboolean(L, -1) )
        {
            AGO_TRACE() << "Pull '" << key << "' as BOOLEAN";
            bool value = lua_toboolean(L, -1);
            table[key] = value;
        }
        else if( lua_isnoneornil(L, -1) )
        {
            AGO_TRACE() << "Pull '" << key << "' as NULL (dropped!)";
            //drop null field
        }
        else if( lua_istable(L, -1) )
        {
            AGO_TRACE() << "Pull '" << key << "' as TABLE";
            Json::Value newList(Json::arrayValue);
            pullTableToList(L, newList);
            table[key] = newList;
        }

        lua_pop(L, 1);
    }
}

void AgoLua::pullTableToList(lua_State *L, Json::Value& list)
{
    lua_pushnil(L);
    while( lua_next(L, -2)!=0 )
    {
        if( lua_isstring(L, -1) )
        {
            AGO_TRACE() << "Pull list value as STRING";
            list.append(lua_tostring(L, -1));
        }
        else if( lua_isnumber(L, -1) )
        {
            AGO_TRACE() << "Pull list value as NUMBER";
            list.append(lua_tonumber(L, -1));
        }
        else if( lua_isboolean(L, -1) )
        {
            AGO_TRACE() << "Pull list value as BOOLEAN";
            bool value = lua_toboolean(L, -1);
            list.append(value);
        }
        else if( lua_isnoneornil(L, -1) )
        {
            AGO_TRACE() << "Pull list value as NULL (dropped!)";
            //drop null field
        }
        else if( lua_istable(L, -1) )
        {
            AGO_TRACE() << "Pull list value as TABLE";
            Json::Value newList(Json::arrayValue);
            pullTableToList(L, newList);
            list.append(newList);
        }

        lua_pop(L, 1);
    }
}

/**
 * Send message LUA function
 */
int AgoLua::luaSendMessage(lua_State *L)
{
    Json::Value content;
    std::string subject;
    // number of input arguments
    int argc = lua_gettop(L);

    // print input arguments
    for(int i=0; i<argc; i++)
    {
        std::string name, value;
        if (nameval(std::string(lua_tostring(L, lua_gettop(L))),name, value))
        {
            if (name == "subject")
            {
                subject = value;
            }
            content[name]=value;
        }
        lua_pop(L, 1);
    }

    // execute "sendMessage". if subject is set, assume its a sendMessage, else it is a sendRequest which
    // wants a response. The Blockly code does not create anything with subject, so this is purely for
    // plain LUA scripts.
    if(subject.empty()) {
        AGO_DEBUG() << "Sending request: " << content;
        AgoResponse response = agoConnection->sendRequest(content);
        pushTableFromJson(L, response.getResponse());
    }else{
        AGO_DEBUG() << "Sending message on " << subject << ": " << content;
        agoConnection->sendMessage(subject, content);
        // TODO: Would caller expect a response?
    }

    return 1;
}

/**
 * Set variable LUA function
 */
int AgoLua::luaSetVariable(lua_State *L)
{
    Json::Value content;

    //get input arguments
    std::string variable(lua_tostring(L,1));
    std::string value(lua_tostring(L,2));
    content["variable"] = variable;
    content["value"] = value;
    content["command"]="setvariable";
    content["uuid"]=agocontroller;

    AGO_DEBUG() << "Set variable: " << content;
    AgoResponse resp = agoConnection->sendRequest(content);

    //manage result
    if( resp.isOk() )
    {
        // XXX: Not sure what we are supposed to set here; returncode was set in older code
        // but there are no docs on how this can be used.
        lua_pushnumber(L, 1);
    }
    else
    {
        //sendcommand problem
        lua_pushnumber(L, 0);
    }

    //update inventory
    updateInventory();

    boost::lock_guard<boost::mutex> lock(mutexInventory);
    if( inventory.size()>0 && inventory.isMember("devices") && inventory.isMember("variables") )
    {
        //update current inventory to reflect changes without reloading it (too long!!)
        Json::Value& variables(inventory["variables"]);
        if( variables.isMember(variable) )
        {
            variables[variable] = value;
            lua_pushnumber(L, 1);
        }
        else
        {
            //unknown variable
            lua_pushnumber(L, 0);
        }
    }
    else
    {
        //no inventory available
        lua_pushnumber(L, 0);
    }

    return 1;
}

/**
 * Return variable value in LUA
 */
int AgoLua::luaGetVariable(lua_State *L)
{
    //init
    std::string variableName = "";

    //update inventory
    updateInventory();

    boost::lock_guard<boost::mutex> lock(mutexInventory);
    if( inventory.size()>0 && inventory.isMember("devices") )
    {
        //get variable name
        variableName = std::string(lua_tostring(L,1));

        if( variableName.length()>0 && inventory.isMember("variables") )
        {
            const Json::Value& variables(inventory["variables"]);
            if( variables.isMember(variableName) )
            {
                lua_pushstring(L, variables[variableName].asString().c_str());
            }
            else
            {
                //unknown variable
                lua_pushnil(L);
            }
        }
        else
        {
            //bad parameter
            lua_pushnil(L);
        }
    }
    else
    {
        //no inventory available
        lua_pushnil(L);
    }

    return 1;
}

/**
 * Return value from inventory["device"]
 * Callback format: getDeviceInventory(uuid, param)
 */
int AgoLua::luaGetDeviceInventory(lua_State *L)
{
    //init
    std::string uuid = "";
    std::string attribute = "";
    std::string subAttribute = "";

    //update inventory
    updateInventory();

    boost::lock_guard<boost::mutex> lock(mutexInventory);
    if( inventory.isMember("devices") )
    {
        const Json::Value& deviceInventory = inventory["devices"];

        // number of input arguments
        int argc = lua_gettop(L);

        // print input arguments
        for(int i=1; i<=argc; ++i)
        {
            switch(i)
            {
                case 1:
                    uuid = std::string(lua_tostring(L,i));
                    break;
                case 2:
                    attribute = std::string(lua_tostring(L,i));
                    break;
                case 3:
                    subAttribute = std::string(lua_tostring(L,i));
                    break;
                default:
                    //unmanaged parameter
                    break;
            }
        }

        if( subAttribute.length()>0 )
        {
            AGO_DEBUG() << "Get device inventory: inventory['devices'][" << uuid << "][" << attribute << "][" << subAttribute << "]";
        }
        else
        {
            AGO_DEBUG() << "Get device inventory: inventory['devices'][" << uuid << "][" << attribute << "]";
        }

        if( deviceInventory.isMember(uuid) )
        {
            const Json::Value& attributes = deviceInventory[uuid];
            if( attributes.isMember(attribute) )
            {
                //return main device attribute
                lua_pushstring(L, attributes[attribute].asString().c_str());
            }
            else
            {
                //search attribute in device values
                bool found = false;
                if( attributes.isMember("values") )
                {
                    const Json::Value& values = attributes["values"];
                    for( auto it = values.begin(); it!=values.end(); it++ )
                    {
                        //TODO return device value property (quantity, unit, latitude, longitude...)
                        if( it.name()==attribute )
                        {
                            //attribute found, get its subattribute value
                            const Json::Value value = *it;
                            if( !value.isNull() )
                            {
                                lua_pushstring(L, value[subAttribute].asString().c_str());
                                found = true;
                                break;
                            }
                        }
                    }
                }

                //handle item not found
                if( !found )
                {
                    lua_pushnil(L);
                }
            }
        }
        else
        {
            //device not found
            lua_pushnil(L);
        }
    }
    else
    {
        //no inventory available
        lua_pushnil(L);
    }

    return 1;
}

/**
 * Force getting inventory manually
 */
int AgoLua::luaGetInventory(lua_State *L)
{
    //update inventory first
    updateInventory();

    //then return it
    boost::lock_guard<boost::mutex> lock(mutexInventory);
    pushTableFromJson(L, inventory);

    return 1;
}

/**
 * Pause script execution
 */
int AgoLua::luaPause(lua_State *L)
{
    //init
    int duration;

    //get duration
    duration = (int)lua_tointeger(L,1);

    //pause script
    sleep(duration);

    lua_pushnil(L);
    return 1;
}

/**
 * Pause script execution (debug mode)
 */
int AgoLua::luaDebugPause(lua_State* L)
{
    //disable pause in debug mode
    lua_pushnil(L);
    return 1;
}

/**
 * Print function (debug mode)
 */
int AgoLua::luaDebugPrint(lua_State* L)
{
    //init
    Json::Value printResult;
    std::string msg = std::string(lua_tostring(L,1));

    AGO_DEBUG() << "Debug script: print " << msg;
    printResult["type"] = DBG_INFO;
    printResult["msg"] = msg;
    agoConnection->emitEvent("luacontroller", "event.system.debugscript", printResult);
    lua_pushnil(L);
    return 1;
}

/**
 * Get device name according to specified uuid
 */
int AgoLua::luaGetDeviceName(lua_State* L)
{
    //init
    std::string uuid;

    //update inventory
    updateInventory();

    boost::lock_guard<boost::mutex> lock(mutexInventory);
    if( inventory.isMember("devices") )
    {
        //get uuid
        uuid = std::string(lua_tostring(L,1));
        if( uuid.length() > 0 && inventory.isMember("devices") )
        {
            Json::Value& devices = inventory["devices"];
            if( devices.isMember(uuid) )
            {
                Json::Value& device = devices[uuid];
                if( device.isMember("name") )
                {
                    lua_pushstring(L, device["name"].asString().c_str());
                }
                else
                {
                    //no field name for found device
                    lua_pushnil(L);
                }
            }
            else
            {
                //unknown device
                lua_pushnil(L);
            }
        }
        else
        {
            //no uuid, function failed
            lua_pushnil(L);
        }
    }
    else
    {
        //no inventory available
        lua_pushnil(L);
    }

    return 1;
}

/**
 * Update inventory if necessary
 */
void AgoLua::updateInventory()
{
    boost::lock_guard<boost::mutex> lock(mutexInventory);
    time_t now = time(NULL);
    if( difftime(now, lastInventoryUpdate)>=INVENTORY_MAX_AGE || inventory.size()==0 )
    {
        AGO_DEBUG() << "Update inventory...";
        //update inventory and make sure it returns something valid (sometimes timeout occured)
        int attemps = 1;
        while( attemps<=10 )
        {
            inventory = agoConnection->getInventory();
            if( inventory.size()==0 )
            {
                //unable to get inventory, retry
                usleep(250000);
            }
            else
            {
                //inventory is filled, stop statement
                break;
            }
            attemps++;
        }

        AGO_DEBUG() << "Inventory retrieved after " << attemps << " attemps.";

        //final inventory check
        if( inventory.size()==0 )
        {
            //no inventory available, add log
            AGO_ERROR() << "No inventory available!";
        }
        else
        {
            //update last update time
            lastInventoryUpdate = now;
        }
    }

}

/**
 * Search triggered events in specified script
 * @param scriptPath: script path to parse
 * @return foundEvents: fill list with found script events
 * @info based on http://www.boost.org/doc/libs/1_31_0/libs/regex/example/snippets/regex_search_example.cpp
 */
void AgoLua::searchEvents(const fs::path& scriptPath, Json::Value& foundEvents)
{
    //get script content
    std::string content = get_file_contents(scriptPath);

    //parse content
    std::string::const_iterator start, end;
    start = content.begin();
    end = content.end();
    boost::match_results<std::string::const_iterator> what;
    boost::match_flag_type flags = boost::match_default;
    std::string lua = "";
    lua = content; //make non blockly script parseable
    while(boost::regex_search(start, end, what, exprAll, flags))
    {
        // what[0] contains the whole string
        // what[1] contains xml
        // what[2] contains lua
        lua = std::string(what[2]);
        // update search position:
        start = what[0].second;
        // update flags:
        flags |= boost::match_prev_avail;
        flags |= boost::match_not_bob;
    }

    start = lua.begin();
    end = lua.end();
    while(boost::regex_search(start, end, what, exprEvent, flags))
    {
        std::string eventName(what[1]);
        if(std::find(foundEvents.begin(), foundEvents.end(), eventName) == foundEvents.end())
            foundEvents.append(eventName);

        // update search position:
        start = what[0].second;
        // update flags:
        flags |= boost::match_prev_avail;
        flags |= boost::match_not_bob;
    }
}

/**
 * Purge old scripts from scriptsInfos
 */
void AgoLua::purgeScripts()
{
    boost::lock_guard<boost::mutex> lock(mutexScriptInfos);

    //check integrity
    if( !scriptsInfos.isMember("scripts") )
    {
        AGO_TRACE() << "No 'scripts' section in config file";
        return;
    }

    //init
    Json::Value localScripts(Json::arrayValue);
    Json::Value configScripts(Json::arrayValue);
    Json::Value& scripts = scriptsInfos["scripts"];

    //get list of local scripts
    if( fs::exists(scriptdir) )
    {
        fs::recursive_directory_iterator it(scriptdir);
        fs::recursive_directory_iterator endit;
        while( it!=endit )
        {
            if( fs::is_regular_file(*it) && it->path().extension().string()==".lua" && it->path().filename().string()!="helper.lua" )
            {
                localScripts.append(it->path().string());
            }
            ++it;
        }
    };
    AGO_TRACE() << "Local scripts: " << localScripts;

    //get list of config scripts
    for( auto it = scripts.begin(); it!=scripts.end(); it++ )
    {
        configScripts.append(it.name());
    }
    AGO_TRACE() << "Config scripts: " << configScripts;

    //purge obsolete infos
    for( auto it=configScripts.begin(); it!=configScripts.end(); it++ )
    {
        bool found = false;
        for(auto it2 = localScripts.begin(); it2 != localScripts.end(); it2++) {
            if(*it == *it2) {
                found = true;
                break;
            }
        }

        if( !found )
        {
            AGO_DEBUG() << "Remove obsolete script infos '" << *it << "'";
            scripts.removeMember(it->asString());
        }
    }

    //save modified config file
    writeJsonFile(scriptsInfos, getConfigPath(SCRIPTSINFOSFILE));
    readJsonFile(scriptsInfos, getConfigPath(SCRIPTSINFOSFILE));
}

/**
 * Init script execution
 */
void AgoLua::initScript(lua_State* L, Json::Value& content, const std::string& script, Json::Value& context, bool debug)
{
    //load libs
    const luaL_Reg *lib;
    for (lib = loadedlibs; lib->func; lib++)
    {
        luaL_requiref(L, lib->name, lib->func, 1);
        lua_pop(L, 1);
    }
    luaL_openlibs(L);

    {
        //load script context (from local variables)
        boost::lock_guard<boost::mutex> lock(mutexScriptContexts);
        if( !scriptContexts.isMember(script) )
        {
            //no context yet, create empty one
            scriptContexts[script] = context;
        }
        else
        {
            context = scriptContexts[script];
        }
    }
    AGO_TRACE() << "LUA context before:" << context;

    //add mapped functions
#define LUA_REGISTER_WRAPPER(L, lua_name, method_name) \
    lua_pushlightuserdata(L, this); \
    lua_pushcclosure(L, &method_name ## _wrapper, 1); \
    lua_setglobal(L, lua_name)

    LUA_REGISTER_WRAPPER(L, "sendMessage", luaSendMessage);
    LUA_REGISTER_WRAPPER(L, "setVariable", luaSetVariable);
    LUA_REGISTER_WRAPPER(L, "getVariable", luaGetVariable);
    LUA_REGISTER_WRAPPER(L, "getDeviceInventory", luaGetDeviceInventory);
    LUA_REGISTER_WRAPPER(L, "getInventory", luaGetInventory);
    if( !debug )
    {
        LUA_REGISTER_WRAPPER(L, "pause", luaPause);
    }
    else
    {
        LUA_REGISTER_WRAPPER(L, "pause", luaDebugPause);
    }
    LUA_REGISTER_WRAPPER(L, "getDeviceName", luaGetDeviceName);
    if( debug )
    {
        LUA_REGISTER_WRAPPER(L, "print", luaDebugPrint);
    }

    pushTableFromJson(L, content);
    lua_setglobal(L, "content");
    pushTableFromJson(L, context);
    lua_setglobal(L, "context");
}

/**
 * Finalize script execution
 */
void AgoLua::finalizeScript(lua_State* L, Json::Value& content, const std::string& script, Json::Value& context)
{
    //handle context value
    lua_getglobal(L, "context");
    pullTableToMap(L, context);
    {
        boost::lock_guard<boost::mutex> lock(mutexScriptContexts);
        scriptContexts[script] = context;
    }
    AGO_TRACE() << "LUA context after:" << context;
}

/**
 * Debug script (threaded)
 */
void AgoLua::debugScript(Json::Value content, const std::string script)
{
    //init
    Json::Value debugResult;
    Json::Value context(Json::objectValue);
    lua_State *L;

    try
    {
        //create new lua
        L = luaL_newstate();

        //init script
        initScript(L, content, "debug", context, true);

        //execute script
        AGO_TRACE() << "Debugging script";
        debugResult["type"] = DBG_START;
        debugResult["msg"] = "Script debugging started";
        agoConnection->emitEvent("luacontroller", "event.system.debugscript", debugResult);
        int status = luaL_loadstring(L, script.c_str());
        std::string err_prefix = "Failed to load script: ";
        if(status == LUA_OK)
        {
            err_prefix = "Failed to execute script: ";
            status = lua_pcall(L, 0, LUA_MULTRET, 0);
        }

        if(status != 0) {
            std::string err = lua_tostring(L, -1);
            lua_pop(L, 1); // remove error message
            AGO_ERROR() << err_prefix << err;
            debugResult["type"] = DBG_ERROR;
            debugResult["msg"] = err_prefix + err;
            agoConnection->emitEvent("luacontroller", "event.system.debugscript", debugResult);
        }
 
        //finalize script
        finalizeScript(L, content, "debug", context);
        lua_close(L);
        AGO_TRACE() << "Debug execution finished.";
        debugResult["type"] = DBG_END;
        debugResult["msg"] = "Script debugging terminated";
        agoConnection->emitEvent("luacontroller", "event.system.debugscript", debugResult);
    }
    catch(...)
    {
        debugResult["type"] = DBG_ERROR;
        debugResult["msg"] = "Script debugging crashed";
        agoConnection->emitEvent("luacontroller", "event.system.debugscript", debugResult);
    }
}

/**
 * Enable/disable specified script
 * @param script: script full path
 * @param enable: enable(1) or disabled(0)
 */
bool AgoLua::enableScript(const std::string& script, bool enabled)
{
    boost::lock_guard<boost::mutex> lock(mutexScriptInfos);

    //get script infos
    Json::Value& scripts(scriptsInfos["scripts"]);
    if( !scripts.isMember(script) )
    {
        //no script infos yet, nothing to do here
        AGO_DEBUG() << "enableScript: no script infos found";
        return false;
    }
    else
    {
        //update enable flag
        Json::Value& infos(scripts[script]);
        infos["enabled"] = enabled;
        writeJsonFile(scriptsInfos, getConfigPath(SCRIPTSINFOSFILE));
        AGO_DEBUG() << "enableScript: enabled flag for script '" << script << "' updated to " << enabled;
    }

    return true;
}

/**
 * Execute LUA script (blocking).
 * Called in separate thread.
 */
void AgoLua::executeScript(Json::Value content, const fs::path &script)
{
    //init
    Json::Value context(Json::objectValue);
    lua_State *L;
    L = luaL_newstate();

    //init script
    initScript(L, content, script.string(), context, false);

    //execute script
    AGO_TRACE() << "Loading " << script;
    int status = luaL_loadfile(L, script.c_str());
    std::string err_prefix = "Failed to load script: ";
    if(status == LUA_OK)
    {
        err_prefix = "Failed to execute script: ";
        status = lua_pcall(L, 0, LUA_MULTRET, 0);
    }
    if(status != 0)
    {
        // Error msg includes filename
        AGO_ERROR() << err_prefix << lua_tostring(L, -1);
        lua_pop(L, 1);
    }

    // finalize script
    finalizeScript(L, content, script.string(), context);
    lua_close(L);
    AGO_TRACE() << "Execution of " << script << " finished.";
}

/**
 * Return true if script can be executed
 */
bool AgoLua::canExecuteScript(const Json::Value& content, const fs::path &script)
{
    boost::lock_guard<boost::mutex> lock(mutexScriptInfos);

    //check integrity
    if( !scriptsInfos.isMember("scripts") )
    {
        return false;
    }

    //check if file modified
    Json::Value& scripts(scriptsInfos["scripts"]);
    Json::Value infos;
    std::time_t updated = boost::filesystem::last_write_time(script);
    bool parseScript = false;
    if( !scripts.isMember(script.string()) )
    {
        //force script parsing
        parseScript = true;
    }
    else
    {
        //script already referenced, check last modified date
        if( scripts.isMember(script.string()) )
        {
            infos = scripts[script.string()];
            if( infos["updated"].asInt() != updated )
            {
                //script modified, parse again content
                parseScript = true;
            }
        }
    }

    if( parseScript )
    {
        AGO_DEBUG() << "Update script infos (" << script << ")";
        infos["updated"] = (int32_t)updated;
        if( !infos.isMember("enabled") )
            infos["enabled"] = true;

        Json::Value events(Json::arrayValue);
        searchEvents(script, events);
        infos["events"] = events;

        AGO_DEBUG() << "Script " << script << " uses the following events: " << events;

        // Update info; it's a clone
        scripts[script.string()] = infos;
        writeJsonFile(scriptsInfos, getConfigPath(SCRIPTSINFOSFILE));

        //reset script context if exists
        boost::lock_guard<boost::mutex> lock(mutexScriptContexts);
        if( scriptContexts.isMember(script.string()) )
        {
            Json::Value& context(scriptContexts[script.string()]);
            context.clear();
        }
    }

    bool executeScript = true;

    //check if script is enabled
    if( infos.isMember("enabled") )
    {
        bool enabled = infos["enabled"].asBool();
        if( !enabled )
        {
            AGO_DEBUG() << "Script '" << script << "' disabled by user";
            executeScript = false;
        }
    }

    //check if current triggered event is caught in script
    if( executeScript )
    {
        executeScript = false;
        if( filterByEvents==1 )
        {
            const Json::Value& events(infos["events"]);
            if( events.size() > 0 )
            {
                for(auto it=events.begin(); it!=events.end(); it++ )
                {
                    if( (*it) == content["subject"] )
                    {
                        executeScript = true;
                        break;
                    }
                }
            }
            else
            {
                //no events detected in script, trigger it everytime :S
                executeScript = true;
            }
        }
        else
        {
            //config option disable events filtering
            executeScript = true;
        }

        if( !executeScript )
        {
            AGO_DEBUG() << "Not executing: '" << script << "' ignores event " << content["subject"];
        }else
            AGO_DEBUG() << "Executing: '" << script << "' handles event " << content["subject"];
    }

    return executeScript;
}

/**
 * Agocontrol command handler
 */
Json::Value AgoLua::commandHandler(const Json::Value& content)
{
    Json::Value returnData;

    std::string internalid = content["internalid"].asString();
    if (internalid == "luacontroller")
    {
        if (content["command"]=="getscriptlist")
        {
            Json::Value scriptlist (Json::arrayValue);
            const Json::Value& scripts(scriptsInfos["scripts"]);
            if (fs::exists(scriptdir))
            {
                fs::recursive_directory_iterator it(scriptdir);
                fs::recursive_directory_iterator endit;
                while (it != endit)
                {
                    if (fs::is_regular_file(*it) && (it->path().extension().string() == ".lua") && (it->path().filename().string() != "helper.lua"))
                    {
                        Json::Value item;
                        //get script name
                        fs::path script = construct_script_name(it->path().filename());
                        std::string scriptName = it->path().stem().string();
                        item["name"] = scriptName;
                        bool enabled = true;
                        //get script infos
                        if( scripts.isMember(script.string()) )
                        {
                            const Json::Value& infos = scripts[script.string()];
                            if( infos.isMember("enabled") )
                                enabled = infos["enabled"].asBool();
                        }
                        item["enabled"] = enabled;
                        scriptlist.append(item);
                    }
                    ++it;
                }
            }
            returnData["scriptlist"] = scriptlist;
            return responseSuccess(returnData);
        }
        else if (content["command"] == "getscript")
        {
            checkMsgParameter(content, "name", Json::stringValue);

            try
            {
                // if a path is passed, strip it for security reasons
                fs::path input(content["name"].asString());
                fs::path script = construct_script_name(input.stem());
                std::string scriptcontent = get_file_contents(script);
                AGO_DEBUG() << "Reading script " << script;
                returnData["script"]=base64_encode(reinterpret_cast<const unsigned char*>(scriptcontent.c_str()), scriptcontent.length());
                returnData["name"]=content["name"].asString();
                return responseSuccess(returnData);
            }
            catch( const fs::filesystem_error& e )
            {
                AGO_ERROR() << "Exception during file reading " << e.what();
                return responseFailed("Unable to read script");
            }
        }
        else if (content["command"] == "setscript" )
        {
            checkMsgParameter(content, "name", Json::stringValue);

            try {
                // if a path is passed, strip it for security reasons
                fs::path input(content["name"].asString());
                fs::path script = construct_script_name(input.stem());

                std::ofstream file;
                file.open(script.c_str());
                if(file) {
                    file << content["script"].asString();
                    file.close();
                    return responseSuccess();
                }
                AGO_ERROR() << "failed to open " << script << ": " << strerror(errno);
                return responseFailed(std::string("Unable to write script file: ") + strerror(errno));
            }
            catch( const fs::filesystem_error& e )
            {
                AGO_ERROR() << "Exception during file writing " << e.what();
                return responseFailed("Unable to write script");
            }
        }
        else if (content["command"] == "delscript")
        {
            checkMsgParameter(content, "name", Json::stringValue);

            try
            {
                // if a path is passed, strip it for security reasons
                fs::path input(content["name"].asString());
                fs::path target = construct_script_name(input.stem());
                if (fs::remove (target))
                {
                    return responseSuccess();
                }
                else
                {
                    return responseFailed("no such script");
                }
            }
            catch( const fs::filesystem_error& e )
            {
                AGO_ERROR() << "Exception during file deleting " << e.what();
                return responseFailed("Unable to delete script");
            }
        }
        else if (content["command"] == "renscript")
        {
            checkMsgParameter(content, "oldname", Json::stringValue);
            checkMsgParameter(content, "newname", Json::stringValue);
        
            try
            {
                // if a path is passed, strip it for security reasons
                fs::path input(content["oldname"].asString());
                fs::path source = construct_script_name(input.stem());

                fs::path output(content["newname"].asString());
                fs::path target = construct_script_name(output.stem());

                //check if destination file already exists
                if( !fs::exists(target) )
                {
                    //rename script
                    fs::rename(source, target);
                    return responseSuccess();
                }
                else
                {
                    return responseFailed("Script with new name already exists. Script not renamed");
                }
            }
            catch( const fs::filesystem_error& e )
            {
                AGO_ERROR() << "Exception during file renaming" << e.what();
                return responseFailed("Unable to rename script");
            }
        }
        else if( content["command"]=="debugscript" )
        {
            AGO_DEBUG() << "debug received: " << content;
            checkMsgParameter(content, "script", Json::stringValue);
            checkMsgParameter(content, "data", Json::objectValue);

            std::string script = content["script"].asString();
            const Json::Value& data(content["data"]);
            AGO_DEBUG() << "Debug script: script=" << script << " content=" << data;

            boost::thread t( boost::bind(&AgoLua::debugScript, this, data, script) );
            t.detach();

            AGO_INFO() << "Command 'debugscript': debug started";
            return responseSuccess();
        }
        else if (content["command"] == "uploadfile")
        {
            //import script
            checkMsgParameter(content, "filepath", Json::stringValue);
            checkMsgParameter(content, "filename", Json::stringValue);

            //check file
            fs::path source(content["filepath"].asString());
            if( fs::is_regular_file(status(source)) && source.extension().string()==".lua")
            {
                try
                {
                    std::string filename;
                    if( content["filename"].asString().find("blockly_")!=0 )
                    {
                        // prepend "blockly_" string
                        filename = "blockly_";
                    }
                    filename += content["filename"].asString();

                    fs::path target = scriptdir / filename;

                    //check if desination file already exists
                    if( !fs::exists(target) )
                    {
                        //move file
                        AGO_DEBUG() << "import " << source << " to " << target;
                        fs::copy_file(source, target);
                        return responseSuccess();
                    }
                    else
                    {
                        AGO_DEBUG() << "Script already exists, nothing overwritten";
                        return responseFailed("Script already exists. Script not imported");
                    }
                }
                catch( const std::exception& e )
                {
                    AGO_ERROR() << "Exception during script import" << e.what();
                    return responseFailed("Unable to import script");
                }
            }
            else
            {
                //invalid file, reject it
                AGO_ERROR() << "Unsupported file uploaded";
                return responseFailed("Unsupported file");
            }
        }
        else if (content["command"] == "downloadfile")
        {
            //export script
            AGO_DEBUG() << "download file command received: " << content;
            checkMsgParameter(content, "filename", Json::stringValue);

            std::string file = "blockly_" + content["filename"].asString();
            fs::path target = construct_script_name(file);
            AGO_DEBUG() << "file to download " << target;

            //check if file exists
            if( fs::exists(target) )
            {
                //file exists, return full path
                AGO_DEBUG() << "Send fullpath of file to download " << target;
                returnData["filepath"] = target.string();
                return responseSuccess(returnData);
            }
            else
            {
                //requested file doesn't exists
                AGO_ERROR() << "File to download doesn't exist";
                return responseFailed("File doesn't exist");
            }
        }
        else if( content["command"]=="getcontacts" )
        {
            //return default contacts
            returnData["email"] = getConfigOption("email", "", "system", "system");
            returnData["phone"] = getConfigOption("phone", "", "system", "system");
            return responseSuccess(returnData);
        }
        else if( content["command"]=="enablescript" )
        {
            //enable/disable script
            AGO_DEBUG() << "enable/disable script command received: " << content;
            checkMsgParameter(content, "enabled", Json::booleanValue);
            checkMsgParameter(content, "name", Json::stringValue);

            fs::path input(content["name"].asString());
            fs::path script = construct_script_name(input.stem());
            bool enabled = content["enabled"].asBool();
            if( enableScript(script.string(), enabled) )
            {
                return responseSuccess();
            }
            else
            {
                return responseFailed("Unable to enable/disable script");
            }
        }
        else
        {
            return responseUnknownCommand();
        }
    }
    else
    {
        //execute scripts
        bool found=false;
        if (fs::exists(scriptdir))
        {
            fs::recursive_directory_iterator it(scriptdir);
            fs::recursive_directory_iterator endit;
            while (it != endit)
            {
                if (fs::is_regular_file(*it) && (it->path().extension().string() == ".lua") &&
                        (it->path().filename().string() != "helper.lua"))
                {
                    if( canExecuteScript(content, it->path()) )
                    {
                        found = true;
                        boost::thread t( boost::bind(&AgoLua::executeScript, this, content, it->path()) );
                        t.detach();
                    }
                }
                ++it;
            }
        }

        if(!found) {
            return responseError(RESPONSE_ERR_NOT_FOUND, "script not found");
        }

        return responseSuccess();
    }
}

/**
 * Agocontrol event handler
 */
void AgoLua::eventHandler(const std::string& subject , const Json::Value& content)
{
    if( subject=="event.device.announce" || subject=="event.device.discover" )
    {
        return;
    }
    else
    {
        //daily script infos purge
        if( subject=="event.environment.timechanged" &&
            content.isMember("minute") && content["minute"].asInt()==0 &&
            content.isMember("hour") && content["hour"].asInt()==0 )
        {
            AGO_DEBUG() << "Purge obsolete scripts";
            purgeScripts();
        }

        // execute scripts
        Json::Value content_ = content;
        content_["subject"] = subject;
        commandHandler(content_);
    }
}

void AgoLua::setupApp()
{
    agocontroller = agoConnection->getAgocontroller();

    //get config
    std::string optString = getConfigOption("filterByEvents", "1");
    sscanf(optString.c_str(), "%d", &filterByEvents);

    scriptdir = ensureDirExists(getConfigPath(LUA_SCRIPT_DIR));

    //load script infos file
    readJsonFile(scriptsInfos, getConfigPath(SCRIPTSINFOSFILE));
    if (!scriptsInfos.isMember("scripts"))
    {
        Json::Value scripts(Json::objectValue);
        scriptsInfos["scripts"] = scripts;
        writeJsonFile(scriptsInfos, getConfigPath(SCRIPTSINFOSFILE));
    }

    agoConnection->addDevice("luacontroller", "luacontroller");
    addCommandHandler();
    addEventHandler();
}

AGOAPP_ENTRY_POINT(AgoLua);

