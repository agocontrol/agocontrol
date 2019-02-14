#ifndef SCHEMA_H
#define SCHEMA_H

#include <boost/filesystem.hpp>
#include <json/json.h>

namespace fs = ::boost::filesystem;

Json::Value mergeList(const Json::Value& a, const Json::Value& b);
Json::Value mergeMap(const Json::Value& a, const Json::Value& b);
Json::Value parseSchema(const fs::path &filename);

#endif
