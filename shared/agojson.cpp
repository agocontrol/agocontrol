/* Json Helpers */

#include <boost/filesystem/fstream.hpp>

#include <assert.h>

#include <json/reader.h>
#include <json/writer.h>

#include "agojson.h"
#include "agolog.h"


using namespace qpid::types;

void agocontrol::variantMapToJson(const Variant::Map &map, Json::Value &root) {
    assert(root.isObject());

    for (Variant::Map::const_iterator it = map.begin(); it != map.end(); ++it) {
        switch (it->second.getType()) {
            case VAR_MAP:
                root[it->first] = Json::Value(Json::objectValue);
                variantMapToJson(it->second.asMap(), root[it->first]);
                break;
            case VAR_LIST:
                root[it->first] = Json::Value(Json::arrayValue);
                variantListToJson(it->second.asList(), root[it->first]);
                break;
            case VAR_BOOL:
                root[it->first] = it->second.asBool(); break;
            case VAR_STRING:
            case VAR_UUID:
                root[it->first] = it->second.asString(); break;
            case VAR_FLOAT:
            case VAR_DOUBLE:
                root[it->first] = it->second.asDouble(); break;
            case VAR_VOID:
                // null
                root[it->first] = Json::Value(); break;
            case VAR_UINT8:
                root[it->first] = it->second.asUint8(); break;
            case VAR_UINT16:
                root[it->first] = it->second.asUint16(); break;
            case VAR_UINT32:
                root[it->first] = it->second.asUint32(); break;
            case VAR_UINT64:
                root[it->first] = (Json::UInt64)it->second.asUint64(); break;
            case VAR_INT8:
                root[it->first] = it->second.asInt8(); break;
            case VAR_INT16:
                root[it->first] = it->second.asInt16(); break;
            case VAR_INT32:
                root[it->first] = it->second.asInt32(); break;
            case VAR_INT64:
                root[it->first] = (Json::Int64)it->second.asInt64(); break;
            default:
                AGO_WARNING() << "Unhandled Variant type " << it->second.getType() << ", mapping to String";
                root[it->first] = it->second.asString();
                break;
        }
    }
}

void agocontrol::variantListToJson(const Variant::List &list, Json::Value &root) {
    assert(root.isArray());
    int idx=0;
    for (Variant::List::const_iterator it = list.begin(); it != list.end(); ++it, idx++) {
        switch (it->getType()) {
            case VAR_MAP:
                root[idx] = Json::Value(Json::objectValue);
                variantMapToJson(it->asMap(), root[idx]);
                break;
            case VAR_LIST:
                root[idx] = Json::Value(Json::arrayValue);
                variantListToJson(it->asList(), root[idx]);
                break;
            case VAR_BOOL:
                root[idx] = it->asBool(); break;
            case VAR_STRING:
            case VAR_UUID:
                root[idx] = it->asString(); break;
            case VAR_FLOAT:
            case VAR_DOUBLE:
                root[idx] = it->asDouble(); break;
            case VAR_VOID:
                // null
                root[idx] = Json::Value(); break;
            case VAR_UINT8:
                root[idx] = it->asUint8(); break;
            case VAR_UINT16:
                root[idx] = it->asUint16(); break;
            case VAR_UINT32:
                root[idx] = it->asUint32(); break;
            case VAR_UINT64:
                root[idx] = (Json::UInt64)it->asUint64(); break;
            case VAR_INT8:
                root[idx] = it->asInt8(); break;
            case VAR_INT16:
                root[idx] = it->asInt16(); break;
            case VAR_INT32:
                root[idx] = it->asInt32(); break;
            case VAR_INT64:
                root[idx] = (Json::Int64)it->asInt64(); break;
            default:
                AGO_WARNING() << "Unhandled Variant type " << it->getType() << ", mapping to String";
                root[idx] = it->asString();
                break;
        }
    }
}

Variant::List agocontrol::jsonToVariantList(const Json::Value& value) {
    Variant::List list;
    try {
        for (Json::ValueConstIterator it = value.begin(); it != value.end(); it++) {
            switch((*it).type()) {
                case Json::nullValue:
                    break;
                case Json::intValue:
                    list.push_back( (*it).asInt());
                    break;
                case Json::uintValue:
                    list.push_back( (*it).asUInt());
                    break;
                case Json::realValue:
                    list.push_back( (*it).asDouble());
                    break;
                case Json::stringValue:
                    list.push_back( (*it).asString());
                    break;
                case Json::booleanValue:
                    list.push_back( (*it).asBool());
                    break;
                case Json::arrayValue:
                    list.push_back(jsonToVariantList((*it)));
                    break;
                case Json::objectValue:
                    list.push_back(jsonToVariantMap((*it)));
                    break;
                default:
                    AGO_WARNING() << "Unhandled Json::ValueType in jsonToVariantList()";
            }
        }
    } catch (const std::exception& error) {
        AGO_ERROR() << "Exception during JSON->Variant::List conversion: " << error.what();
    }

    return list;
}

Variant::Map agocontrol::jsonToVariantMap(const Json::Value& value) {
    Variant::Map map;
    try {
        for (Json::ValueConstIterator it = value.begin(); it != value.end(); it++) {
            switch((*it).type()) {
                case Json::nullValue:
                    break;
                case Json::intValue:
                    map[it.key().asString()] = (*it).asInt();
                    break;
                case Json::uintValue:
                    map[it.key().asString()] = (*it).asUInt();
                    break;
                case Json::realValue:
                    map[it.key().asString()] = (*it).asDouble();
                    break;
                case Json::stringValue:
                    map[it.key().asString()] = (*it).asString();
                    break;
                case Json::booleanValue:
                    map[it.key().asString()] = (*it).asBool();
                    break;
                case Json::arrayValue:
                    map[it.key().asString()] = jsonToVariantList((*it));
                    break;
                case Json::objectValue:
                    map[it.key().asString()] = jsonToVariantMap((*it));
                    break;
                default:
                    AGO_WARNING() << "Unhandled Json::ValueType in jsonToVariantMap()";
            }
        }
    } catch (const std::exception& error) {
        AGO_ERROR() << "Exception during JSON->Variant::Map conversion: " << error.what();
    }
    return map;
}


bool agocontrol::readJsonFile(Json::Value& root, const boost::filesystem::path &filename) {
    try {
        boost::filesystem::ifstream f;
        f.exceptions(std::ios_base::failbit | std::ios_base::badbit);
        f.open(filename, std::ifstream::binary);

#if JSONCPP_VERSION_MAJOR >= 1
        Json::CharReaderBuilder builder;
        std::string errors;
        if (!Json::parseFromStream(builder, f, &root, &errors)) {
            AGO_ERROR() << "Failed to parse " << filename << ": " << errors;
            return false;
        }
#else
        // jsoncpp 0.6
        Json::Reader reader;
        if(!reader.parse(f, root)) {
            AGO_ERROR() << "Failed to parse " << filename << ": " << reader.getFormattedErrorMessages();
            return false;
        }
#endif
        return true;
#if JSONCPP_VERSION_MAJOR >= 1
    } catch (const Json::RuntimeError& ex) {
        AGO_ERROR() << "Failed to parse " << filename << ": " << ex.what();
        return false;
#endif
    } catch (const std::ios_base::failure& ex) {
        AGO_ERROR() << "Failed to read " << filename << ": " << std::strerror(errno);
        return false;
    }
}

bool agocontrol::writeJsonFile(const Json::Value& root, const boost::filesystem::path &filename) {
    try {
        boost::filesystem::ofstream f;
        f.exceptions ( std::ios_base::failbit | std::ios_base::badbit );
        f.open(filename, std::ifstream::binary);

#if JSONCPP_VERSION_MAJOR >= 1
        Json::StreamWriterBuilder builder;
        builder.settings_["indentation"] = "";
        std::unique_ptr<Json::StreamWriter> writer (builder.newStreamWriter());
        writer->write(root, &f);
#else
        // jsoncpp 0.6
        Json::FastWriter writer;
        f << writer.write(root);
#endif
        return true;
    } catch (const std::ios_base::failure& ex) {
        AGO_ERROR() << "Failed to write " << filename << ": " << std::strerror(errno);
        return false;
    }
}

bool agocontrol::variantMapToJSONFile(const Variant::Map& map, const boost::filesystem::path &filename) {
    Json::Value root(Json::objectValue);
    variantMapToJson(map, root);
    return writeJsonFile(root, filename);
}

bool agocontrol::jsonFileToVariantMap(Variant::Map& map, const boost::filesystem::path &filename) {
    Json::Value root(Json::objectValue);
    if (readJsonFile(root, filename)) {
        map = jsonToVariantMap(root);
        return true;
    }
    return false;
}

Variant::Map agocontrol::jsonFileToVariantMap(const boost::filesystem::path &filename) {
    Variant::Map map;
    jsonFileToVariantMap(map, filename);
    return map;
}

