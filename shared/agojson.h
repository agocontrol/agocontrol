#ifndef AGOCONTROL_AGOJSON_H
#define AGOCONTROL_AGOJSON_H

#include <boost/filesystem.hpp>
#include <json/value.h>
#include <qpid/messaging/Message.h>

namespace agocontrol {

    /// convert a Variant::Map to JSON representation.
    void variantMapToJson(const qpid::types::Variant::Map &map, Json::Value &root) ;
    void variantListToJson(const qpid::types::Variant::List &list, Json::Value &root) ;

    /// convert a JSON value to a Variant::Map.
    qpid::types::Variant::Map jsonToVariantMap(const Json::Value& value);

    /// convert a JSON string to a Variant::List.
    qpid::types::Variant::List jsonToVariantList(const Json::Value& value);

    /// Read a JSON::Value from a file
    bool readJsonFile(Json::Value& root, const boost::filesystem::path &filename);

    /// write a Json::Value to a file
    bool writeJsonFile(const Json::Value& root, const boost::filesystem::path &filename);

    /// write a Variant::Map to a JSON file.
    bool variantMapToJSONFile(const qpid::types::Variant::Map& map, const boost::filesystem::path &filename);

    /// Read a JSON file into a Variant::Map, returning false if read fails, leaving map untouched.
    bool jsonFileToVariantMap(qpid::types::Variant::Map& map, const boost::filesystem::path &filename);

    /// Read a JSON file and return a Variant::Map, returning an empty map if read fails.
    qpid::types::Variant::Map jsonFileToVariantMap(const boost::filesystem::path &filename);

};

#endif //AGOCONTROL_AGOJSON_H
