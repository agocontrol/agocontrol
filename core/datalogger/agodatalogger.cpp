#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>

#include <string>
#include <iostream>
#include <sstream>
#include <cerrno>

#include <cppdb/frontend.h>
#include <stdarg.h>

#include <boost/date_time/posix_time/time_parsers.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <json/json.h>

#include <rrd.h>
#include "base64.h"

#include "agoapp.h"
#include "agojson.h"

#ifndef DBFILE
#define DBFILE "datalogger.db"
#endif

#ifndef DEVICEMAPFILE
#define DEVICEMAPFILE "/maps/datalogger.json"
#endif

#define JOURNAL_ALL "all"
#define JOURNAL_DEBUG "debug"
#define JOURNAL_INFO "info"
#define JOURNAL_WARNING "warning"
#define JOURNAL_ERROR "error"

using namespace agocontrol;
using namespace boost::posix_time;
using namespace boost::gregorian;
namespace fs = ::boost::filesystem;

using namespace agocontrol;

class AgoDataLogger: public AgoApp {
private:
    cppdb::session sql;
    std::string dbname;
    //inventory
    void updateInventory();
    bool checkInventory();

    //utils
    std::string string_format(const std::string fmt, ...);
    int string_real_length(const std::string str);
    std::string string_prepend_spaces(std::string source, size_t newSize);
    static void debugSqlite(void* foo, const char* msg);
    void computeRendering();
    void commandGetData(const Json::Value& content, Json::Value& returnData);
    bool commandGetGraph(const Json::Value& content, Json::Value& returnData);

    //database
    bool createTableIfNotExist(std::string tablename, std::list<std::string> createqueries);
    Json::Value getDatabaseInfos();
    bool purgeTable(std::string table, int timestamp);
    bool isTablePurgeAllowed(std::string table);
    void getGraphData(const Json::Value& uuids, int start, int end, std::string environment, Json::Value& result);
    bool getGraphDataFromSqlite(const Json::Value& uuids, int start, int end, std::string environment, Json::Value& result);
    bool getGraphDataFromRrd(const Json::Value& uuids, int start, int end, Json::Value& result);

    //rrd
    bool prepareGraph(std::string uuid, int multiId, Json::Value& data);
    void dumpGraphParams(const char** params, const int num_params);
    bool addGraphParam(const std::string& param, char** params, int* index);
    void addDefaultParameters(int start, int end, std::string vertical_unit, int width, int height, char** params, int* index);
    void addDefaultThumbParameters(int duration, int width, int height, char** params, int* index);
    void addSingleGraphParameters(Json::Value& data, char** params, int* index);
    void addMultiGraphParameters(Json::Value& data, char** params, int* index);
    void addThumbGraphParameters(Json::Value& data, char** params, int* index);
    bool generateGraph(const Json::Value& uuids, int start, int end, int thumbDuration, unsigned char** img, unsigned long* size);

    //journal
    bool eventHandlerJournal(std::string message, std::string type);
    bool getMessagesFromJournal(const Json::Value& content, Json::Value& messages);
        
    //system
    void saveDeviceMapFile();
    Json::Value commandHandler(const Json::Value& content);
    void eventHandler(const std::string& subject , const Json::Value& content);
    void eventHandlerRRDtool(std::string subject, std::string uuid, Json::Value content);
    void eventHandlerSQL(std::string subject, std::string uuid, Json::Value content);
    void dailyPurge();
    void setupApp();
public:
    AGOAPP_CONSTRUCTOR(AgoDataLogger);
};

Json::Value inventory;
Json::Value units;
bool dataLogging = 1;
bool gpsLogging = 1;
bool rrdLogging = 1;
int purgeDelay = 0; //in months
std::string desiredRendering = "plots"; // could be "image" or "plots"
std::string rendering = "plots";
//GraphDataSource graphDataSource = SQLITE;
const char* colors[] = {"#800080", "#0000FF", "#008000", "#FF00FF", "#000080", "#FF0000", "#00FF00", "#00FFFF", "#800000", "#808000", "#008080", "#C0C0C0", "#808080", "#000000", "#FFFF00"};
Json::Value devicemap;
std::list<std::string> allowedPurgeTables;

/**
 * Update inventory
 */
void AgoDataLogger::updateInventory()
{
    bool unitsLoaded = false;

    //test unit content
    if( units.size()>0 )
        unitsLoaded = true;

    //get inventory
    inventory = agoConnection->getInventory();

    //get units
    if( !unitsLoaded && inventory.isMember("schema") )
    {
        for( auto it = inventory["schema"]["units"].begin(); it!=inventory["schema"]["units"].end(); it++ )
        {
            units[it.name()] = (*it)["label"].asString();
        }
    }
}

/**
 * Check inventory, fetch it if not inited yet.
 */
bool AgoDataLogger::checkInventory() {
    if (inventory.isNull()) {
        updateInventory();
    }

    if (inventory.isMember("devices"))
        return true;

    //inventory is empty
    return false;
}

bool AgoDataLogger::createTableIfNotExist(std::string tablename, std::list<std::string> createqueries) {
    try {
        cppdb::result r;
        if (sql.driver() == "sqlite3") {
            AGO_TRACE() << "checking existance of table in sqlite: " << tablename;
            r = sql<< "SELECT name FROM sqlite_master WHERE type='table' AND name = ?" << tablename << cppdb::row;
        } else {
            AGO_TRACE() << "checking existance of table in non-sqlite: " << tablename;
            r = sql << "SELECT * FROM information_schema.tables WHERE table_schema = ? AND table_name = ? LIMIT 1" << dbname << tablename << cppdb::row;
        }
        if (r.empty()) {
            AGO_INFO() << "Creating missing table '" << tablename << "'";
            for( std::list<std::string>::iterator it=createqueries.begin(); it!=createqueries.end(); it++ ) {
                sql << (*it) << cppdb::exec;
            }
            createqueries.clear();
        }
    } catch(std::exception const &e) {
        AGO_ERROR() << "Sql exception: " << e.what();
    }
    return true;
}

/**
 * Save device map file
 */
void AgoDataLogger::saveDeviceMapFile()
{
    fs::path dmf = getConfigPath(DEVICEMAPFILE);
    writeJsonFile(devicemap, dmf);
}

/**
 * Format string to specified format
 */
std::string AgoDataLogger::string_format(const std::string fmt, ...) {
    int size = ((int)fmt.size()) * 2 + 50;   // use a rubric appropriate for your code
    std::string str;
    va_list ap;
    while (1) {     // maximum 2 passes on a POSIX system...
        str.resize(size);
        va_start(ap, fmt);
        int n = vsnprintf((char *)str.data(), size, fmt.c_str(), ap);
        va_end(ap);
        if (n > -1 && n < size) {  // everything worked
            str.resize(n);
            return str;
        }
        if (n > -1)  // needed size returned
            size = n + 1;   // for null char
        else
            size *= 2;      // guess at a larger size (o/s specific)
    }
    return str;
}

/**
 * Return real string length
 */
int AgoDataLogger::string_real_length(const std::string str)
{
    int len = 0;
    const char* s = str.c_str();
    while(*s)
    {
        len += (*s++ & 0xc0) != 0x80;
    }
    return len;
}

/**
 * Prepend to source string until source size reached
 */
std::string AgoDataLogger::string_prepend_spaces(std::string source, size_t newSize)
{
    std::string output = source;
    int max = (int)newSize - string_real_length(output);
    for( int i=0; i<max; i++ )
    {
        output.insert(0, 1, ' ');
    }
    return output;
}

/**
 * Prepare graph for specified uuid
 * @param uuid: device uuid
 * @param multiId: if multigraph specify its index (-1 if not multigraph)
 * @param data: output map
 */
bool AgoDataLogger::prepareGraph(std::string uuid, int multiId, Json::Value& data)
{
    //check params
    if( multiId>=15 )
    {
        //no more color available
        AGO_ERROR() << "agodatalogger-RRDtool: no more color available";
        return false;
    }

    //filename
    std::stringstream filename;
    filename << uuid << ".rrd";

    //get device infos from inventory
    if( !checkInventory() )
    {
        //unable to continue
        AGO_ERROR() << "agodatalogger-RRDtool: no inventory available";
        return false;
    }

    const Json::Value& devices(inventory["devices"]);
    if( !devices.isMember(uuid) || !devices[uuid].isMember("devicetype") )
    {
        //device not found
        AGO_ERROR() << "agodatalogger-RRDtool: device not found";
        return false;
    }

    const Json::Value& device(devices[uuid]);

    //prepare kind
    std::string kind = device["devicetype"].asString();
    replaceString(kind, "sensor", "");
    replaceString(kind, "meter", "");
    std::string pretty_kind = kind;
    if( multiId>=0 )
    {
        //keep first 4 chars
        pretty_kind.resize(4);
        //and append device name (or uuid if name not set)
        if( device.isMember("name") && !device["name"].asString().empty() )
        {
            pretty_kind = device["name"].asString();
        }
        else
        {
            pretty_kind = uuid + " (" + pretty_kind + ")";
        }
    }
    pretty_kind.resize(20, ' ');

    //prepare unit
    std::string unit = "U";
    std::string vertical_unit = "U";
    if( device.isMember("values") )
    {
        const Json::Value& values(device["values"]);
        for(auto it=values.begin(); it!=values.end(); it++ )
        {
            if( it.name() == kind || it.name() == "batterylevel" )
            {
                if( it->isMember("unit") )
                {
                    vertical_unit = (*it)["unit"].asString();
                }
                break;
            }
        }
    }
    if( units.isMember(vertical_unit) )
    {
        vertical_unit = units[vertical_unit].asString();
    }

    if( vertical_unit=="%" )
    {
        unit = "%%";
    }
    else
    {
        unit = vertical_unit;
    }

    //prepare colors
    std::string colorL = "#000000";
    std::string colorA = "#A0A0A0";
    std::string colorMax = "#FF0000";
    std::string colorMin = "#00FF00";
    std::string colorAvg = "#0000FF";
    if( multiId<0 )
    {
        if( device["devicetype"].asString()=="humiditysensor" )
        {
            //blue
            colorL = "#0000FF";
            colorA = "#7777FF";
        }
        else if( device["devicetype"].asString()=="temperaturesensor" )
        {
            //red
            colorL = "#FF0000";
            colorA = "#FF8787";
        }
        else if( device["devicetype"].asString()=="energysensor" || device["devicetype"].asString()=="powersensor" ||
                device["devicetype"].asString()=="powermeter" || device["devicetype"].asString()=="batterysensor" )
        {
            //green
            colorL = "#007A00";
            colorA = "#00BB00";
        }
        else if( device["devicetype"].asString()=="brightnesssensor" )
        {
            //orange
            colorL = "#CCAA00";
            colorA = "#FFD400";
        }
    }
    else
    {
        //multi graph (only line color necessary)
        colorL = colors[multiId];
    }

    //fill output map
    data["filename"] = filename.str();
    data["kind" ] = pretty_kind;
    data["unit"] = unit;
    data["vertical_unit"] = vertical_unit;
    data["colorL"] = colorL;
    data["colorA"] = colorA;
    data["colorMax"] = colorMax;
    data["colorMin"] = colorMin;
    data["colorAvg"] = colorAvg;

    return true;
}

/**
 * Display params (debug purpose)
 */
void AgoDataLogger::dumpGraphParams(const char** params, const int num_params)
{
    AGO_TRACE() << "Dump graph parameters (" << num_params << " params) :";
    for( int i=0; i<num_params; i++ )
    {
        AGO_TRACE() << " - " << std::string(params[i]);
    }
}

/**
 * Add graph param
 */
bool AgoDataLogger::addGraphParam(const std::string& param, char** params, int* index)
{
    params[(*index)] = (char*)malloc(sizeof(char)*param.length()+1);
    if( params[(*index)]!=NULL )
    {
        strcpy(params[(*index)], param.c_str());
        (*index)++;
        return true;
    }
    return false;
}

/**
 * Add default and mandatory graph params
 */
void AgoDataLogger::addDefaultParameters(int start, int end, std::string vertical_unit, int width, int height, char** params, int* index)
{
    //first params
    addGraphParam("dummy", params, index);
    addGraphParam("-", params, index);

    addGraphParam("--slope-mode", params, index);

    //start
    addGraphParam("--start", params, index);
    addGraphParam(string_format("%d", start), params, index);

    //end
    addGraphParam("--end", params, index);
    addGraphParam(string_format("%d", end), params, index);

    //vertical label
    if( vertical_unit.length()>0 )
    {
        addGraphParam("--vertical-label", params, index);
        addGraphParam(vertical_unit, params, index);
    }

    //size
    addGraphParam("--width", params, index);
    addGraphParam(string_format("%d", width), params, index);
    addGraphParam("--height", params, index);
    addGraphParam(string_format("%d", height), params, index);
}

/**
 * Add default thumb graph parameters
 */
void AgoDataLogger::addDefaultThumbParameters(int duration, int width, int height, char** params, int* index)
{
    //first params
    addGraphParam("dummy", params, index);
    addGraphParam("-", params, index);

    //start
    addGraphParam("--start", params, index);
    addGraphParam(string_format("end-%dh", duration), params, index);

    //end
    addGraphParam("--end", params, index);
    addGraphParam("now", params, index);

    //size
    addGraphParam("--width", params, index);
    addGraphParam(string_format("%d", width), params, index);
    addGraphParam("--height", params, index);
    addGraphParam(string_format("%d", height), params, index);

    //legend
    addGraphParam("--no-legend", params, index);
    //addGraphParam("--y-grid", params, index);
    //addGraphParam("none", params, index);
    addGraphParam("--x-grid", params, index);
    addGraphParam("none", params, index);
}

/**
 * Add single graph parameters
 */
void AgoDataLogger::addSingleGraphParameters(Json::Value& data, char** params, int* index)
{
    std::string param = "";

    //DEF
    fs::path rrdfile = getLocalStatePath(data["filename"].asString());
    addGraphParam(string_format("DEF:level=%s:level:AVERAGE", rrdfile.c_str()), params, index);

    //VDEF last
    addGraphParam("VDEF:levellast=level,LAST", params, index);

    //VDEF average
    addGraphParam("VDEF:levelavg=level,AVERAGE", params, index);

    //VDEF max
    addGraphParam("VDEF:levelmax=level,MAXIMUM", params, index);

    //VDEF min
    addGraphParam("VDEF:levelmin=level,MINIMUM", params, index);

    //GFX AREA
    addGraphParam(string_format("AREA:level%s", data["colorA"].asString().c_str()), params, index);

    //GFX LINE
    addGraphParam(string_format("LINE1:level%s:%s", data["colorL"].asString().c_str(), data["kind"].asString().c_str()), params, index);

    //MIN LINE
    addGraphParam(string_format("LINE1:levelmin%s::dashes", data["colorMin"].asString().c_str()), params, index);

    //MIN GPRINT
    addGraphParam(string_format("GPRINT:levelmin:   Min %%6.2lf%s", data["unit"].asString().c_str()), params, index);

    //MAX LINE
    addGraphParam(string_format("LINE1:levelmax%s::dashes", data["colorMax"].asString().c_str()), params, index);

    //MAX GPRINT
    addGraphParam(string_format("GPRINT:levelmax:   Max %%6.2lf%s", data["unit"].asString().c_str()), params, index);

    //AVG LINE
    addGraphParam(string_format("LINE1:levelavg%s::dashes", data["colorAvg"].asString().c_str()), params, index);

    //AVG GPRINT
    addGraphParam(string_format("GPRINT:levelavg:   Avg %%6.2lf%s", data["unit"].asString().c_str()), params, index);

    //LAST GPRINT
    addGraphParam(string_format("GPRINT:levellast:   Last %%6.2lf%s", data["unit"].asString().c_str()), params, index);
}

/**
 * Add multi graph parameters
 */
void AgoDataLogger::addMultiGraphParameters(Json::Value& data, char** params, int* index)
{
    //keep index value
    int id = (*index);

    //DEF
    fs::path rrdfile = getLocalStatePath(data["filename"].asString());
    addGraphParam(string_format("DEF:level%d=%s:level:AVERAGE", id, rrdfile.c_str()), params, index);

    //VDEF last
    addGraphParam(string_format("VDEF:levellast%d=level%d,LAST", id, id), params, index);

    //VDEF average
    addGraphParam(string_format("VDEF:levelavg%d=level%d,AVERAGE", id, id), params, index);

    //VDEF max
    addGraphParam(string_format("VDEF:levelmax%d=level%d,MAXIMUM", id, id), params, index);

    //VDEF min
    addGraphParam(string_format("VDEF:levelmin%d=level%d,MINIMUM", id, id), params, index);

    //GFX LINE
    addGraphParam(string_format("LINE1:level%d%s:%s", id, data["colorL"].asString().c_str(), data["kind"].asString().c_str()), params, index);

    //MIN GPRINT
    addGraphParam(string_format("GPRINT:levelmin%d:     Min %%6.2lf%s", id, data["unit"].asString().c_str()), params, index);

    //MAX GPRINT
    addGraphParam(string_format("GPRINT:levelmax%d:     Max %%6.2lf%s", id, data["unit"].asString().c_str()), params, index);

    //AVG GPRINT
    addGraphParam(string_format("GPRINT:levelavg%d:     Avg %%6.2lf%s", id, data["unit"].asString().c_str()), params, index);

    //LAST GPRINT
    addGraphParam(string_format("GPRINT:levellast%d:     Last %%6.2lf%s", id, data["unit"].asString().c_str()), params, index);

    //new line
    addGraphParam("COMMENT:\\n", params, index);
}

/**
 * Add thumb graph parameters
 */
void AgoDataLogger::addThumbGraphParameters(Json::Value& data, char** params, int* index)
{
    //keep index value
    int id = (*index);

    //DEF
    fs::path rrdfile = getLocalStatePath(data["filename"].asString());
    addGraphParam(string_format("DEF:level%d=%s:level:AVERAGE", id, rrdfile.c_str()), params, index);

    //GFX LINE
    addGraphParam(string_format("LINE1:level%d%s:%s", id, data["colorL"].asString().c_str(), data["kind"].asString().c_str()), params, index);
}

/**
 * Generate RRDtool graph
 */
bool AgoDataLogger::generateGraph(const Json::Value& uuids, int start, int end, int thumbDuration, unsigned char** img, unsigned long* size)
{
    char** params;
    int num_params = 0;
    int index = 0;
    int multiId = -1;
    Json::Value datas(Json::arrayValue);

    //get graph datas
    for( auto it = uuids.begin(); it!=uuids.end(); it++ )
    {
        //multigraph?
        if( uuids.size()>1 )
            multiId++;

        //get data
        Json::Value data(Json::objectValue);
        if( prepareGraph((*it).asString(), multiId, data) )
        {
            datas.append(data);
        }
    }

    //prepare graph parameters
    if( datas.size()>1 )
    {
        //multigraph

        //adjust some stuff
        int defaultNumParam = 13;
        std::string vertical_unit = "";
        std::string lastUnit = "";
        size_t maxUnitLength = 0;
        for( auto it = datas.begin(); it!=datas.end(); it++ )
        {
            vertical_unit = (*it)["vertical_unit"].asString();

            if( (*it)["unit"].asString().length()>maxUnitLength )
            {
                maxUnitLength = (*it)["unit"].asString().length();
            }

            if( (*it)["unit"].asString()!=lastUnit )
            {
                if( lastUnit.length()>0 )
                {
                    //not the same unit
                    defaultNumParam = 11;
                    vertical_unit = "";
                }
                else
                {
                    lastUnit = (*it)["unit"].asString();
                }
            }
        }

        //format unit (add spaces for better display)
        for( auto it = datas.begin(); it!=datas.end(); it++ )
        {
            std::string unit = (*it)["unit"].asString();
            //special case for %%
            if( unit=="%%" )
            {
                (*it)["unit"] = string_prepend_spaces(unit, maxUnitLength+1);
            }
            else
            {
                (*it)["unit"] = string_prepend_spaces(unit, maxUnitLength);
            }
        }

        if( thumbDuration<=0 )
        {
            //alloc memory
            num_params = 11 * datas.size() + defaultNumParam; //11 parameters per datas + 10 default parameters (wo vertical_unit) or 12 (with vertical_unit)
            params = (char**)malloc(sizeof(char*) * num_params);

            //add graph parameters
            addDefaultParameters(start, end, vertical_unit, 850, 300, params, &index);
            for( auto it = datas.begin(); it!=datas.end(); it++ )
            {
                addMultiGraphParameters(*it, params, &index);
            }
        }
        else
        {
            //alloc memory
            num_params = 2 * datas.size() + 13;
            params = (char**)malloc(sizeof(char*) * num_params);

            //add graph parameters
            addDefaultThumbParameters(thumbDuration, 250, 40, params, &index);
            for( auto it = datas.begin(); it!=datas.end(); it++ )
            {
                addThumbGraphParameters(*it, params, &index);
            }
        }
    }
    else if( datas.size()==1 )
    {
        //single graph
        Json::Value& data(datas[0]);
        if( thumbDuration<=0 )
        {
            //alloc memory
            num_params = 14 + 13; //14 specific graph parameters + 13 default parameters
            params = (char**)malloc(sizeof(char*) * num_params);

            //add graph parameters
            addDefaultParameters(start, end, data["vertical_unit"].asString(), 850, 300, params, &index);
            addSingleGraphParameters(data, params, &index);
        }
        else
        {
            //alloc memory
            num_params = 2 * datas.size() + 13;
            params = (char**)malloc(sizeof(char*) * num_params);

            //add graph parameters
            addDefaultThumbParameters(thumbDuration, 250, 40, params, &index);
            addThumbGraphParameters(data, params, &index);
        }
    }
    else
    {
        //no data
        AGO_ERROR() << "agodatalogger-RRDtool: no data";
        return false;
    }
    num_params = index;
    dumpGraphParams((const char**)params, num_params);

    //build graph
    bool found = false;
    rrd_clear_error();
    rrd_info_t* grinfo = rrd_graph_v(num_params, (char**)params);
    rrd_info_t* walker;
    if( grinfo!=NULL )
    {
        walker = grinfo;
        while (walker)
        {
            AGO_TRACE() << "RRD walker key = " << walker->key;
            if (strcmp(walker->key, "image") == 0)
            {
                *img = walker->value.u_blo.ptr;
                *size = walker->value.u_blo.size;
                found = true;
                break;
            }
            walker = walker->next;
        }
    }
    else
    {
        AGO_ERROR() << "agodatalogger-RRDtool: unable to generate graph [" << rrd_get_error() << "]";
        return false;
    }

    if( !found ) 
    {
        AGO_ERROR() << "agodatalogger-RRDtool: no image generated by rrd_graph_v command";
        return false;
    }

    //free memory
    for( int i=0; i<num_params; i++ )
    {
        free(params[i]);
        params[i] = NULL;
    }
    free(params);

    return true;
}

/**
 * Store event data into RRDtool database
 */
void AgoDataLogger::eventHandlerRRDtool(std::string subject, std::string uuid, Json::Value content)
{
    if( (subject=="event.device.batterylevelchanged" || boost::algorithm::starts_with(subject, "event.environment.")) && content.isMember("level") && content.isMember("uuid") )
    {
        //generate rrd filename and path
        std::stringstream filename;
        filename << content["uuid"].asString() << ".rrd";
        fs::path rrdfile = getLocalStatePath(filename.str());

        //create rrd file if necessary
        if( !fs::exists(rrdfile) )
        {
            AGO_INFO() << "New device detected, create rrdfile " << rrdfile.string();
            const char *params[] = {"DS:level:GAUGE:21600:U:U", "RRA:AVERAGE:0.5:1:1440", "RRA:AVERAGE:0.5:5:2016", "RRA:AVERAGE:0.5:30:1488", "RRA:AVERAGE:0.5:60:8760", "RRA:AVERAGE:0.5:360:2920", "RRA:MIN:0.5:1:1440", "RRA:MIN:0.5:5:2016", "RRA:MIN:0.5:30:1488", "RRA:MIN:0.5:60:8760", "RRA:MIN:0.5:360:2920", "RRA:MAX:0.5:1:1440", "RRA:MAX:0.5:5:2016", "RRA:MAX:0.5:30:1488", "RRA:MAX:0.5:60:8760", "RRA:MAX:0.5:360:2920"};

            rrd_clear_error();
            int res = rrd_create_r(rrdfile.string().c_str(), 60, 0, 16, params);
            if( res<0 )
            {
                AGO_ERROR() << "agodatalogger-RRDtool: unable to create rrdfile [" << rrd_get_error() << "]";
            }
        }  

        //update rrd
        if( fs::exists(rrdfile) )
        {
            char param[50];
            snprintf(param, 50, "N:%s", content["level"].asString().c_str());
            const char* params[] = {param};

            rrd_clear_error();
            int res = rrd_update_r(rrdfile.string().c_str(), "level", 1, params);
            if( res<0 )
            {
                AGO_ERROR() << "agodatalogger-RRDtool: unable to update data [" << rrd_get_error() << "] with param [" << param << "]";
            }
        }
    }
}

/**
 * Store event data into SQL database
 */
void AgoDataLogger::eventHandlerSQL(std::string subject, std::string uuid, Json::Value content)
{
    std::string result;

    if( gpsLogging && subject=="event.environment.positionchanged" && content["latitude"].asString()!="" && content["longitude"].asString()!="" )
    {
        AGO_DEBUG() << "specific environment case: position";
        std::string lat = content["latitude"].asString();
        std::string lon = content["longitude"].asString();
        try {
            sql <<  "INSERT INTO position VALUES(null, ?, ?, ?, ?)" << uuid << lat << lon << (int)time(NULL) << cppdb::exec;
        } catch(std::exception const &e) {
            AGO_ERROR() << "Sql error: "  << e.what();
            return;
        }
    }
    else if( dataLogging && content["level"].asString() != "")
    {
        replaceString(subject, "event.environment.", "");
        replaceString(subject, "event.device.", "");
        replaceString(subject, "changed", "");
        replaceString(subject, "event.", "");
        replaceString(subject, "security.sensortriggered", "state"); // convert security sensortriggered event to state

        try {
            cppdb::statement stat = sql << "INSERT INTO data VALUES(null, ?, ?, ?, ?)";

            std::string level = content["level"].asString();
            stat.bind(uuid);
            stat.bind(subject);

            double value;
            switch(content["level"].type()) {
                case Json::realValue:
                    value = content["level"].asDouble();
                    stat.bind(value);
                    break;
                default:
                    stat.bind(level);
            }

            stat.bind(time(NULL));

            stat.exec();
        } catch(std::exception const &e) {
            AGO_ERROR() << "Sql error: "  << e.what();
            return;
        }
    }

}

/**
 * Store journal message into SQLite database
 */
bool AgoDataLogger::eventHandlerJournal(std::string message, std::string type)
{
    try {
        sql <<  "INSERT INTO journal VALUES(null, ?, ?, ?)" << time(NULL) << message << type << cppdb::exec;
    } catch(std::exception const &e) {
        AGO_ERROR() << "Sql error: "  << e.what();
        return false;
    }
    return true;
}

/**
 * Main event handler
 */
void AgoDataLogger::eventHandler(const std::string& subject, const Json::Value& content)
{
    if( !subject.empty() && content.isMember("uuid") )
    {
        //data logging
        eventHandlerSQL(subject, content["uuid"].asString(), content);

        //rrd logging
        if( rrdLogging )
        {
            eventHandlerRRDtool(subject, content["uuid"].asString(), content);
        }
    }
    else if( subject=="event.environment.timechanged" )
    {
        updateInventory();

        if( content.isMember("hour") && content.isMember("minute") && content["hour"].asInt()==0 && content["minute"].asInt()==0 )
        {
            //midnight launch daily purge
            dailyPurge();
        }
    }
}

void AgoDataLogger::debugSqlite(void* foo, const char* msg)
{
    AGO_TRACE() << "SQLITE: " << msg;
}

/**
 * Return graph data from rrd file
 */
//bool AgoDataLogger::GetGraphDataFromRrd(Json::Value content, Json::Value &result)
bool AgoDataLogger::getGraphDataFromRrd(const Json::Value& uuids, int start, int end, Json::Value& result)
{
    AGO_TRACE() << "getGraphDataFromRrd";

    Json::Value values(Json::arrayValue);
    bool error = false;
    std::string uuid = uuids[0].asString();
    std::stringstream filename;
    filename << uuid << ".rrd";
    fs::path rrdfile = getLocalStatePath(filename.str());
    std::string filenamestr = rrdfile.string();
    time_t startTimet = (time_t)start;
    time_t endTimet = (time_t)end;
    AGO_TRACE() << "file=" << filenamestr << " start=" << start << " end=" << end;

    unsigned long step = 0;
    unsigned long ds_cnt;
    char** ds_namv;
    rrd_value_t *data = NULL;
    rrd_clear_error();
    int count = 0;
    unsigned long ds = 0;
    //rrd_fetch_r example found here https://github.com/pldimitrov/Rrd/blob/master/src/Rrd.c
    int res = rrd_fetch_r(filenamestr.c_str(), "AVERAGE", &startTimet, &endTimet, &step, &ds_cnt, &ds_namv, &data);
    if( res==0 )
    {
        int size = (endTimet - startTimet) / step - 1;
        double level = 0;
        for( ds=0; ds<ds_cnt; ds++ )
        {
            for( int i=0; i<size; i++ )
            {
                level = (double)data[ds+i*ds_cnt];
                if( !std::isnan(level) )
                {
                    count++;
                    Json::Value value;
                    value["time"] = (uint64_t)startTimet;
                    value["level"] = (double)data[ds+i*ds_cnt];
                    values.append(value);
                }
                startTimet += step;
            }   
        }
        AGO_TRACE() << "rrd_fetch returns: step=" << step << " datasource_count=" << ds_cnt << " data_count=" << count;

        if( data )
            free(data);

        for( unsigned int i=0; (unsigned long) i < ds_cnt; i++ )
            free(ds_namv[i]);

        free(ds_namv);
    }
    else
    {
        AGO_WARNING() << "rrd_fetch failed: " << rrd_get_error();
        error = true;
    }

    result["values"] = values;
    return !error;
}

/**
 * Return graph data from sqlite
 */
bool AgoDataLogger::getGraphDataFromSqlite(const Json::Value& uuids, int start, int end, std::string environment, Json::Value& result)
{
    AGO_TRACE() << "getGraphDataFromSqlite: " << environment;
    Json::Value values(Json::arrayValue);
    std::string uuid = uuids[0].asString();
    try {
        if( environment=="position" )
        {
            AGO_TRACE() << "Execute query on postition table";
            cppdb::result r = sql << "SELECT timestamp, latitude, longitude FROM position WHERE timestamp BETWEEN ? AND ? AND uuid = ? ORDER BY timestamp" << start << end << uuid;
            while(r.next()) {
                Json::Value value;
                value["time"] = r.get<int>("timestamp");
                value["latitude"] = r.get<double>("latitude");
                value["longitude"] = r.get<double>("longitude");
                values.append(value);
            }
        }
        else
        {
            AGO_TRACE() << "Execute query on data table";
            cppdb::result r = sql << "SELECT timestamp, level FROM data WHERE timestamp BETWEEN ? AND ? AND environment = ? AND uuid = ? ORDER BY timestamp" << start << end << environment << uuid;
            while(r.next()) {
                Json::Value value;
                value["time"] = r.get<int>("timestamp");
                value["level"] = r.get<double>("level");
                values.append(value);
            }
        }
    } catch (std::exception const &e) {
        AGO_ERROR() << "SQL Error: " << e.what();
        return false;
    }
    AGO_TRACE() << "SQL query returns " << values.size() << " values";

    result["values"] = values;
    return true;
}

/**
 * Return data for graph generation
 */
void AgoDataLogger::getGraphData(const Json::Value& uuids, int start, int end, std::string environment, Json::Value& result)
{
    if( dataLogging )
    {
        getGraphDataFromSqlite(uuids, start, end, environment, result);
    }
    else if( environment=="position" )
    {
        //force values retrieving from database
        getGraphDataFromSqlite(uuids, start, end, environment, result);
    }
    else if( rrdLogging )
    {
        getGraphDataFromRrd(uuids, start, end, result);
    }
    else
    {
        Json::Value empty(Json::arrayValue);
        result["values"] = empty;
    }
}

/**
 * Return messages from journal
 * datetime format: 2015-07-12T22:00:00.000Z
 */
bool AgoDataLogger::getMessagesFromJournal(const Json::Value& content, Json::Value& result)
{
    Json::Value messages(Json::arrayValue);
    std::string filter = "";
    std::string type = "";

    //handle filter
    if( content.isMember("filter") )
    {
        filter = content["filter"].asString();
        //append jokers
        filter = "%" + filter + "%";
    }

    //handle type
    if( content.isMember("type") )
    {
        if( content["type"]==JOURNAL_ALL )
        {
            type = "%%";
        }
        else
        {
            type = content["type"].asString();
        }
    }


    //parse the timestrings
    std::string startDate = content["start"].asString();
    std::string endDate = content["end"].asString();
    replaceString(startDate, "-", "");
    replaceString(startDate, ":", "");
    replaceString(startDate, "Z", "");
    replaceString(endDate, "-", "");
    replaceString(endDate, ":", "");
    replaceString(endDate, "Z", "");
    boost::posix_time::ptime base(boost::gregorian::date(1970, 1, 1));
    boost::posix_time::time_duration start = boost::posix_time::from_iso_string(startDate) - base;
    boost::posix_time::time_duration end = boost::posix_time::from_iso_string(endDate) - base;

    AGO_TRACE() << "getMessagesFromJournal: start=" << start.total_seconds() << " end=" << end.total_seconds() << " filter=" << filter << " type=" << type;
    try {
        cppdb::result r = sql <<  "SELECT timestamp, message, type FROM journal WHERE timestamp BETWEEN ? AND ? AND message LIKE ? AND type LIKE ? ORDER BY timestamp DESC" << start.total_seconds() << end.total_seconds() << filter << type;
        while (r.next()) {
            Json::Value value;
            value["time"] = r.get<int>("timestamp");
            value["message"] = r.get<std::string>("message");
            value["type"] = r.get<std::string>("type");
            messages.append(value);
        }
    } catch (std::exception const &e) {
        AGO_ERROR() << "SQL Error: " << e.what();
        return false;
    }

    AGO_TRACE() << "Query returns " << messages.size() << " messages";

    //prepare result
    result["messages"] = messages;

    return true;
}

/**
 * Return some information about database (size, date of first entry...)
 */
Json::Value AgoDataLogger::getDatabaseInfos()
{
    Json::Value returnval;
    returnval["data_start"] = 0;
    returnval["data_end"] = 0;
    returnval["data_count"] = 0;
    returnval["position_start"] = 0;
    returnval["position_end"] = 0;
    returnval["position_count"] = 0;
    returnval["journal_start"] = 0;
    returnval["journal_end"] = 0;
    returnval["journal_count"] = 0;

    //get data time range
    try {
        cppdb::result r = sql <<  "SELECT MIN(timestamp) AS min, MAX(timestamp) AS max FROM data" << cppdb::row;
        if (!r.empty()) {
            returnval["data_start"] = r.get<int>("min");
            returnval["data_end"] = r.get<int>("max");
        }
    } catch (std::exception const &e) {
        AGO_ERROR() << "SQL Error: " << e.what();
    }

    //get data count
    try {
        cppdb::result r = sql <<  "SELECT COUNT(id) AS count FROM data" << cppdb::row;
        if (!r.empty()) {
            returnval["data_count"] = r.get<int64_t>("count");
        }
    } catch (std::exception const &e) {
        AGO_ERROR() << "SQL Error: " << e.what();
    }

    //get position time range
    try {
        cppdb::result r = sql <<  "SELECT MIN(timestamp) AS min, MAX(timestamp) AS max FROM position" << cppdb::row;
        if (!r.empty()) {
            returnval["position_start"] = r.get<int>("min");
            returnval["position_end"] = r.get<int>("max");
        }
    } catch (std::exception const &e) {
        AGO_ERROR() << "SQL Error: " << e.what();
    }

    //get position count
    try {
        cppdb::result r = sql <<  "SELECT COUNT(id) AS count FROM position" << cppdb::row;
        if (!r.empty()) {
            returnval["position_count"] = r.get<int64_t>("count");
        }
    } catch (std::exception const &e) {
        AGO_ERROR() << "SQL Error: " << e.what();
    }

    //get journal time range
    try {
        cppdb::result r = sql <<  "SELECT MIN(timestamp) AS min, MAX(timestamp) AS max FROM journal" << cppdb::row;
        if (!r.empty()) {
            returnval["journal_start"] = r.get<int>("min");
            returnval["journal_end"] = r.get<int>("max");
        }
    } catch (std::exception const &e) {
        AGO_ERROR() << "SQL Error: " << e.what();
    }

    //get journal count
    try {
        cppdb::result r = sql <<  "SELECT COUNT(id) AS count FROM journal" << cppdb::row;
        if (!r.empty()) {
            returnval["journal_count"] = r.get<int64_t>("count");
        }
    } catch (std::exception const &e) {
        AGO_ERROR() << "SQL Error: " << e.what();
    }

    return returnval;
}

/**
 * Purge specified table before specified timestamp.
 * If timestamp=0 all table content is purged
 */
bool AgoDataLogger::purgeTable(std::string table, int timestamp=0)
{
    std::stringstream query;
    query << "DELETE FROM " << table;

    if( timestamp!=0 )
    {
        query << " WHERE timestamp<" << timestamp;
    }

    AGO_TRACE() << "purgeTable query: " << query.str();
    try {
        sql << query.str() << cppdb::exec;
        //vacuum database
        if (sql.driver() == "sqlite3") sql << "VACUUM" << cppdb::exec;
    } catch(std::exception const &e) {
        AGO_ERROR() << "SQL Error: " << e.what();
        return false;
    }
    return true;
}

/**
 * Return specified datetime as database format
 * Datetime format: 2015-07-12T22:00:00.000Z
 */
std::string dateToDatabaseFormat(boost::posix_time::ptime pt)
{
  std::ostringstream datetime_ss;
  time_facet * p_time_output = new time_facet;
  std::locale special_locale (std::locale(""), p_time_output);
  datetime_ss.imbue (special_locale);
  (*p_time_output).format("%Y-%m-%dT%H:%M:%SZ");
  datetime_ss << pt;
  return datetime_ss.str();

}

/**
 * Table can be purged?
 */
bool AgoDataLogger::isTablePurgeAllowed(std::string table)
{
    return std::find(allowedPurgeTables.begin(), allowedPurgeTables.end(), table) != allowedPurgeTables.end();
}

/**
 * Execute daily purge on tables
 */
void AgoDataLogger::dailyPurge()
{
    if( purgeDelay>0 )
    {
        //get current timestamp
        int timestamp = time(NULL);

        //decrease current timestamp with number of configured months
        timestamp -= purgeDelay * 2628000;

        //get infos before purge
        Json::Value before = getDatabaseInfos();

        //purge tables
        for( auto it = allowedPurgeTables.begin(); it!=allowedPurgeTables.end(); it++ )
        {
            purgeTable(*it, timestamp);
        }

        //get infos after purge
        Json::Value after = getDatabaseInfos();

        //log infos
        int dataCount = before["data_count"].asInt() - after["data_count"].asInt();
        int journalCount = before["journal_count"].asInt() - after["journal_count"].asInt();
        int positionCount = before["position_count"].asInt() - after["position_count"].asInt();
        AGO_INFO() << "Daily purge removed " << dataCount << " from data table, " << positionCount << " from position table, " << journalCount << " from journal table";
    }
    else
    {
        //purge disabled
        AGO_DEBUG() << "Daily database purge disabled";
    }
}

/**
 * Compute real rendering value according to user preferences
 */
void AgoDataLogger::computeRendering()
{
    if( !dataLogging && !rrdLogging )
    {
        //no choice: no rendering
        rendering = "none";
    }
    else if( dataLogging && !rrdLogging )
    {
        //only plots rendering available
        rendering = "plots";
    }
    else if( !dataLogging && rrdLogging )
    {
        //rrd enabled, both plots and image available, use user preference
        rendering = desiredRendering;
    }
    else
    {
        //both rrd and data logging are enabled, use user preference
        rendering = desiredRendering;
    }
    AGO_DEBUG() << "Computed rendering: " << rendering;
}

/**
 * "getdata" and "getrawdata" commands handler
 */
void AgoDataLogger::commandGetData(const Json::Value& content, Json::Value& returnData)
{
    AGO_TRACE() << "commandGetData";

    //check parameters
    checkMsgParameter(content, "start", Json::intValue);
    checkMsgParameter(content, "end", Json::intValue);
    checkMsgParameter(content, "devices", Json::arrayValue);

    //variables
    const Json::Value& uuids(content["devices"]);
    std::string environment = "";
    if( content.isMember("env") )
    {
        environment = content["env"].asString();
    }

    //get data
    getGraphData(uuids, content["start"].asInt(), content["end"].asInt(), environment, returnData);
}

/**
 * "getgraph" command handler
 */
bool AgoDataLogger::commandGetGraph(const Json::Value& content, Json::Value& returnData)
{
    AGO_TRACE() << "commandGetGraph";

    //check parameters
    checkMsgParameter(content, "start", Json::intValue);
    checkMsgParameter(content, "end", Json::intValue);
    checkMsgParameter(content, "devices", Json::arrayValue);

    //variables
    unsigned char* img = NULL;
    unsigned long size = 0;
    Json::Value uuids(content["devices"]);

    //is a multigraph?
    if( uuids.size()==1 )
    {
        std::string internalid = agoConnection->uuidToInternalId((*uuids.begin()).asString());
        if( internalid.length()>0 && devicemap["multigraphs"].isMember(internalid) )
        {
            uuids = devicemap["multigraphs"][internalid]["uuids"];
        }
    }

    //get image
    if( generateGraph(uuids, content["start"].asInt(), content["end"].asInt(), 0, &img, &size) )
    {
        returnData["graph"] = base64_encode(img, size);
        return true;
    }
    else
    {
        //error generating graph
        return false;
    }
}

/**
 * Command handler
 */
Json::Value AgoDataLogger::commandHandler(const Json::Value& content)
{
    Json::Value returnData(Json::objectValue);
    std::string internalid = content["internalid"].asString();
    if (internalid == "dataloggercontroller")
    {
        if( (content["command"]=="getdata" || content["command"]=="getgraph") && rendering=="none" )
        {
            //no data storage, nothing to return
            returnData["rendering"] = rendering;
            return responseSuccess(returnData);
        }
        else if( content["command"]=="getdata" )
        {
            //get data according to user preferences
            
            //is multigraph?
            bool isMultigraph = false;
            checkMsgParameter(content, "devices", Json::arrayValue);
            const Json::Value& uuids(content["devices"]);
            if( uuids.size()==1 )
            {
                std::string internalid = agoConnection->uuidToInternalId((*uuids.begin()).asString());
                if( internalid.length()>0 && devicemap["multigraphs"].isMember(internalid) )
                {
                    isMultigraph = true;
                }
            }
            
            if( isMultigraph || rendering=="image" )
            {
                //render image
                //multigraph only implemented on rrdtool for now
                if( commandGetGraph(content, returnData) )
                {
                    returnData["rendering"] = "image";
                    return responseSuccess(returnData);
                }
                else
                {
                    return responseFailed("Failed to generate graph");
                }
                
            }
            else
            {
                //render plots
                commandGetData(content, returnData);
                returnData["rendering"] = "plots";
                return responseSuccess(returnData);
            }
        }
        else if( content["command"]=="getrawdata" )
        {
            //explicitely request raw data (device values)
            commandGetData(content, returnData);
            returnData["rendering"] = "raw";
            return responseSuccess(returnData);
        }
        else if( content["command"]=="getgraph" )
        {
            //explicitely request an image
            if( commandGetGraph(content, returnData) )
            {
                returnData["rendering"] = "image";
                return responseSuccess(returnData);
            }
            else
            {
                return responseFailed("Failed to generate graph");
            }
        }
        else if (content["command"] == "getdeviceenvironments")
        {
            try {
                cppdb::result r = sql << "SELECT distinct uuid, environment FROM data";
                while (r.next()) {
                    returnData[r.get<std::string>("uuid")]=r.get<std::string>("environment");
                }
            } catch(std::exception const &e) {
                AGO_ERROR() << "SQL Error: " << e.what();
                return responseFailed("SQL Error");
            }
            return responseSuccess(returnData);
        }
        else if( content["command"]=="getconfig" )
        {
            //add multigrahs
            Json::Value multis(Json::arrayValue);
            if( devicemap.isMember("multigraphs") )
            {
                for( auto it = devicemap["multigraphs"].begin(); it!=devicemap["multigraphs"].end(); it++ )
                {
                    if( it->isMember("uuids") )
                    {
                        Json::Value multi;
                        multi["name"] = it.name();
                        multi["uuids"] = (*it)["uuids"];
                        multis.append(multi);
                    }
                }
            }

            //database infos
            Json::Value db;
            fs::path dbpath = getLocalStatePath(DBFILE);
            struct stat stat_buf;
            int rc = stat(dbpath.c_str(), &stat_buf);
            db["size"] = (rc==0 ? (int)stat_buf.st_size : 0);
            db["infos"] = getDatabaseInfos();

            returnData["error"] = 0;
            returnData["msg"] = "";
            returnData["multigraphs"] = multis;
            returnData["dataLogging"] = dataLogging ? 1 : 0;
            returnData["gpsLogging"] = gpsLogging ? 1 : 0;
            returnData["rrdLogging"] = rrdLogging ? 1 : 0;
            returnData["purgeDelay"] = purgeDelay;
            returnData["rendering"] = desiredRendering;
            returnData["database"] = db;
            return responseSuccess(returnData);
        }
        else if( content["command"]=="addmultigraph" )
        {
            checkMsgParameter(content, "uuids", Json::arrayValue);
            checkMsgParameter(content, "period", Json::intValue);

            std::string internalid = "multigraph" + devicemap["nextid"].asString();
            if( agoConnection->addDevice(internalid, "multigraph") )
            {
                devicemap["nextid"] = devicemap["nextid"].asInt() + 1;
                Json::Value device;
                device["uuids"] = content["uuids"];
                device["period"] = content["period"].asInt();
                devicemap["multigraphs"][internalid] = device;
                saveDeviceMapFile();
                return responseSuccess("Multigraph " + internalid + " created successfully");
            }
            else
            {
                AGO_ERROR() << "Unable to add new multigraph";
                return responseFailed("Failed to add device");
            }
        }
        else if( content["command"]=="deletemultigraph" )
        {
            checkMsgParameter(content, "multigraph", Json::stringValue);

            std::string internalid = content["multigraph"].asString();
            if( agoConnection->removeDevice(internalid) )
            {
                devicemap["multigraphs"].removeMember(internalid);
                saveDeviceMapFile();
                return responseSuccess("Multigraph " + internalid + " deleted successfully");
            }
            else
            {
                AGO_ERROR() << "Unable to delete multigraph " << internalid;
                return responseFailed("Failed to delete graph");
            }
        }
        else if( content["command"]=="getthumb" )
        {
            checkMsgParameter(content, "multigraph", Json::stringValue);

            //check if inventory available
            if( !checkInventory() )
            {
                //force inventory update during getthumb command because thumb requests are performed during first ui loading
                updateInventory();
            }

            std::string internalid = content["multigraph"].asString();
            if( devicemap.isMember("multigraphs") && devicemap["multigraphs"].isMember(internalid) && devicemap["multigraphs"][internalid].isMember("uuids") )
            {
                unsigned char* img = NULL;
                unsigned long size = 0;
                int period = 12;

                //get thumb period
                if( devicemap["multigraphs"][internalid].isMember("period") )
                {
                    period = devicemap["multigraphs"][internalid]["period"].asInt();
                }

                if( generateGraph(devicemap["multigraphs"][internalid]["uuids"], 0, 0, period, &img, &size) )
                {
                    returnData["graph"] = base64_encode(img, size);
                    return responseSuccess(returnData);
                }
                else
                {
                    //error generating graph
                    return responseFailed("Internal error");
                }
            }
            else
            {
                AGO_ERROR() << "Unable to get thumb: it seems multigraph '" << internalid << "' doesn't exist";
                return responseFailed("Unknown multigraph");
            }
        }
        else if( content["command"]=="setconfig" )
        {
            checkMsgParameter(content, "dataLogging", Json::booleanValue);
            checkMsgParameter(content, "rrdLogging", Json::booleanValue);
            checkMsgParameter(content, "gpsLogging", Json::booleanValue);
            checkMsgParameter(content, "purgeDelay", Json::intValue);
            checkMsgParameter(content, "rendering", Json::stringValue);

            bool error = false;
            if( content["dataLogging"].asBool() )
            {
                dataLogging = true;
                AGO_INFO() << "Data logging enabled";
            }
            else
            {
                dataLogging = false;
                AGO_INFO() << "Data logging disabled";
            }
            if( !setConfigOption("dataLogging", dataLogging) )
            {
                AGO_ERROR() << "Unable to save dataLogging status to config file";
                error = true;
            }

            if( !error )
            {
                if( content["gpsLogging"].asBool() )
                {
                    gpsLogging = true;
                    AGO_INFO() << "GPS logging enabled";
                }
                else
                {
                    gpsLogging = false;
                    AGO_INFO() << "GPS logging disabled";
                }
                if( !setConfigOption("gpsLogging", gpsLogging) )
                {
                    AGO_ERROR() << "Unable to save gpsLogging status to config file";
                    error = true;
                }
            }
    
            if( !error )
            {
                if( content["rrdLogging"].asBool() )
                {
                    rrdLogging = true;
                    AGO_INFO() << "RRD logging enabled";
                }
                else
                {
                    rrdLogging = false;
                    AGO_INFO() << "RRD logging disabled";
                }
                if( !setConfigOption("rrdLogging", rrdLogging) )
                {
                    AGO_ERROR() << "Unable to save rrdLogging status to config file";
                    error = true;
                }
            }

            if( !error )
            {
                purgeDelay = content["purgeDelay"].asInt();
                if( !setConfigOption("purgeDelay", purgeDelay) )
                {
                    AGO_ERROR() << "Unable to save purge delay to config file";
                    error = true;
                }
            }

            if( !error )
            {
                desiredRendering = content["rendering"].asString();
                if( !setConfigOption("rendering", desiredRendering) )
                {
                    AGO_ERROR() << "Unable to save rendering value to config file";
                    error = true;
                }
                else
                {
                    //get real rendering according user preferences
                    computeRendering();
                }   
            }
    
            if( error )
            {
                return responseFailed("Failed to save one or more options");
            }

            if( dataLogging )
                AGO_INFO() << "Data logging enabled";
            else
                AGO_INFO() << "Data logging disabled";
            if( gpsLogging )
                AGO_INFO() << "GPS logging enabled";
            else
                AGO_INFO() << "GPS logging disabled";
            if( rrdLogging )
                AGO_INFO() << "RRD logging enabled";
            else
                AGO_INFO() << "RRD logging disabled";

            return responseSuccess();
        }
        else if( content["command"]=="purgetable" )
        {
            checkMsgParameter(content, "table", Json::stringValue);

            //security check
            std::string table = content["table"].asString();
            if( isTablePurgeAllowed(table) )
            {
                if( purgeTable(table) )
                {
                    return responseSuccess();
                }
                else
                {
                    AGO_ERROR() << "Unable to purge table '" << table << "'";
                    return responseFailed("Internal error");
                }
            }
            else
            {
                AGO_ERROR() << "Purge table '" << table << "' not allowed";
                return responseFailed("Table purge not allowed");
            }
        }
        else
        {
            return responseUnknownCommand();
        }
    }
    else if( internalid=="journal" )
    {
        if( content["command"]=="addmessage" )
        {
            //store journal message
            checkMsgParameter(content, "message", Json::stringValue);
            std::string type = content["type"].asString();
            if( type.empty() )
                type = JOURNAL_INFO;

            if( eventHandlerJournal(content["message"].asString(), type) )
            {
                return responseSuccess();
            }
            else
            {
                return responseFailed("Internal error");
            }
        }
        else if( content["command"]=="getmessages" )
        {
            //return messages in specified time range
            checkMsgParameter(content, "filter", Json::stringValue, true);
            checkMsgParameter(content, "type", Json::stringValue, false);

            Json::Value content_(content);
            if( !content_.isMember("start") && !content_.isMember("end") )
            {
                //no timerange specified, return message of today
                ptime s(date(day_clock::local_day()), hours(0));
                ptime e(date(day_clock::local_day()), hours(23)+minutes(59)+seconds(59));
                content_["start"] = dateToDatabaseFormat(s);
                content_["end"] = dateToDatabaseFormat(e);
            }
            else
            {
                checkMsgParameter(content_, "start", Json::stringValue, false);
                checkMsgParameter(content_, "end", Json::stringValue, false);
            }

            if( getMessagesFromJournal(content_, returnData) )
            {
                return responseSuccess(returnData);
            }
            else
            {
                return responseFailed("Internal error");
            }
        }
        else
        {
            return responseUnknownCommand();
        }
    }

    //We do not support sending commands to our 'devices'
    return responseNoDeviceCommands();
}

void AgoDataLogger::setupApp()
{
    //init
    allowedPurgeTables.push_back("data");
    allowedPurgeTables.push_back("position");
    allowedPurgeTables.push_back("journal");

    //init database
    fs::path dbpath = getLocalStatePath(DBFILE);
    try {
        std::string dbconnection = getConfigOption("dbconnection", std::string("sqlite3:db=") + dbpath.string());
        AGO_TRACE() << "CppDB connection string: " << dbconnection;
        sql = cppdb::session(dbconnection);
        AGO_INFO() << "Using " << sql.driver() << " database via CppDB";
        if (sql.driver() == "mysql") {
       	    size_t start = dbconnection.find("database=");
            if (start != std::string::npos) {
                std::string remainder = dbconnection.substr(start+9);
                size_t end = remainder.find(";");
                if (end != std::string::npos) {
                    dbname = remainder.substr(0,end);
		} else {
                    dbname = remainder;
		}
		AGO_INFO() << "Database name: " << dbname;
            }

        }
    } catch (std::exception const &e) {
        AGO_ERROR() << "Can't open database: " << e.what();
        throw StartupError();
    }

    //create missing tables
    std::list<std::string> queries;
    //db
    if (sql.driver() == "sqlite3") {
        queries.push_back("CREATE TABLE data(id INTEGER PRIMARY KEY AUTOINCREMENT, uuid TEXT, environment TEXT, level REAL, timestamp LONG);");
    } else {
        queries.push_back("CREATE TABLE data (id INTEGER PRIMARY KEY AUTO_INCREMENT, uuid VARCHAR(36), environment VARCHAR(64), level REAL, timestamp INT(11));");
    }
    queries.push_back("CREATE INDEX timestamp_idx ON data (timestamp);");
    queries.push_back("CREATE INDEX environment_idx ON data (environment);");
    queries.push_back("CREATE INDEX uuid_idx ON data (uuid);");
    createTableIfNotExist("data", queries);
    queries.clear();
    //position
    if (sql.driver() == "sqlite3") {
        queries.push_back("CREATE TABLE position(id INTEGER PRIMARY KEY AUTOINCREMENT, uuid TEXT, latitude REAL, longitude REAL, timestamp LONG)");
    } else {
        queries.push_back("CREATE TABLE position (id INTEGER PRIMARY KEY AUTO_INCREMENT, uuid VARCHAR(36), latitude REAL, longitude REAL, timestamp INT(11))");
    }
    queries.push_back("CREATE INDEX timestamp_position_idx ON position (timestamp)");
    queries.push_back("CREATE INDEX uuid_position_idx ON position (uuid)");
    createTableIfNotExist("position", queries);
    queries.clear();
    //journal table
    if (sql.driver() == "sqlite3") {
        queries.push_back("CREATE TABLE journal(id INTEGER PRIMARY KEY AUTOINCREMENT, timestamp LONG, message TEXT, type TEXT)");
    } else {
        queries.push_back("CREATE TABLE journal (id INTEGER PRIMARY KEY AUTO_INCREMENT, timestamp INT(11), message TEXT, type VARCHAR(64))");
    }
    queries.push_back("CREATE INDEX timestamp_journal_idx ON journal (timestamp)");
    queries.push_back("CREATE INDEX type_journal_idx ON journal (type)");
    createTableIfNotExist("journal", queries);
    queries.clear();

    //add controller
    agoConnection->addDevice("dataloggercontroller", "dataloggercontroller");

    //add journal
    agoConnection->addDevice("journal", "journal");

    //read config
    std::string optString = getConfigOption("dataLogging", "1");
    int r;
    if(sscanf(optString.c_str(), "%d", &r) == 1) {
        dataLogging = (r == 1);
    }
    optString = getConfigOption("gpsLogging", "1");
    if(sscanf(optString.c_str(), "%d", &r) == 1) {
        gpsLogging = (r == 1);
    }
    optString = getConfigOption("rrdLogging", "1");
    if(sscanf(optString.c_str(), "%d", &r) == 1) {
        rrdLogging = (r == 1);
    }
    if( dataLogging )
        AGO_INFO() << "Data logging enabled";
    else
        AGO_INFO() << "Data logging disabled";
    if( gpsLogging )
        AGO_INFO() << "GPS logging enabled";
    else
        AGO_INFO() << "GPS logging disabled";
    if( rrdLogging )
        AGO_INFO() << "RRD logging enabled";
    else
        AGO_INFO() << "RRD logging disabled";
    optString = getConfigOption("purgeDelay", "0");
    if(sscanf(optString.c_str(), "%d", &r) == 1)
    {
        purgeDelay = r;
    }
    AGO_INFO() << "Purge delay = " << purgeDelay << " months";
    desiredRendering = getConfigOption("rendering", "plots");
    AGO_INFO() << "Rendering = " << desiredRendering;
    //get real rendering according user preferences
    computeRendering();

    // load map, create sections if empty
    fs::path dmf = getConfigPath(DEVICEMAPFILE);
    readJsonFile(devicemap, dmf);
    if (!devicemap.isMember("nextid"))
    {
        devicemap["nextid"] = 1;
        writeJsonFile(devicemap, dmf);
    }
    if (!devicemap.isMember("multigraphs"))
    {
        Json::Value devices(Json::arrayValue);
        devicemap["multigraphs"] = devices;
        writeJsonFile(devicemap, dmf);
    }

    //register existing devices
    AGO_INFO() << "Register existing multigraphs:";
    Json::Value multigraphs = devicemap["multigraphs"];
    for( Json::Value::const_iterator it=multigraphs.begin(); it!=multigraphs.end(); it++ )
    {
        std::string internalid = it.name();
        AGO_INFO() << " - " << internalid;
        agoConnection->addDevice(internalid, "multigraph");
    }

    addEventHandler();
    addCommandHandler();
}

AGOAPP_ENTRY_POINT(AgoDataLogger);
