#ifndef AGOCONTROL_AGOJSON_H
#define AGOCONTROL_AGOJSON_H

#include <boost/filesystem.hpp>
#include <json/json.h>

namespace agocontrol {

    /// Read a JSON::Value from a file
    bool readJsonFile(Json::Value& root, const boost::filesystem::path &filename);

    /// write a Json::Value to a file
    bool writeJsonFile(const Json::Value& root, const boost::filesystem::path &filename);


    /// Mimics Variant::parse
    Json::Value parseToJson(const std::string& s);

    bool stringToUInt(const Json::Value& in, Json::UInt& out);
    bool stringToInt(const Json::Value& in, Json::Int& out);
    bool stringToDouble(const Json::Value& in, double& out);
}

#endif //AGOCONTROL_AGOJSON_H
