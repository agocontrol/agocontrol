#include <iostream>
#include <fstream>
#include <string>
#include "schema.h"
#include "agojson.h"


#if defined(__FreeBSD__) && YAML_CPP_VERSION <= 3
#include "yaml-cpp03/yaml.h"
#else
#include "yaml-cpp/yaml.h"
#endif

static Json::Value yamlSequenceToJsonValue(const YAML::Node& node);
static Json::Value yamlMapToJsonValue(const YAML::Node& node);

/// Merge two arrayValues
Json::Value mergeList(const Json::Value& a, const Json::Value& b) {
    Json::Value result = b;
    for(auto it = a.begin(); it != a.end(); it++) {
        result.append(*it);
    }
    return result;
} 

Json::Value mergeMap(const Json::Value& a, const Json::Value& b) {
    Json::Value result = a;

    for (auto it = b.begin(); it != b.end(); it++) {

        if (result.isMember(it.name())) {
            Json::Value &aValue(result[it.name()]);

            if ((aValue.type()==Json::objectValue) && (it->type()==Json::objectValue)) {
                result[it.name()] = mergeMap(aValue, *it);
            } else if ((aValue.type()==Json::arrayValue) && (it->type()==Json::arrayValue)) {
                result[it.name()] = mergeList(aValue, *it);
            } else {
                Json::Value list;
                list.append(*it);
                list.append(aValue);
                result[it.name()].swap(list);
            }
        } else {
            result[it.name()] = *it;
        }
    }

    return result;

}

static Json::Value yamlMapToJsonValue(const YAML::Node& node) {
    Json::Value output(Json::objectValue);

#if YAML_CPP_VERSION >= 5
    for(YAML::const_iterator it=node.begin();it!=node.end();++it) {
        std::string key = it->first.as<std::string>();
        //std::cout << "Key: " << key << " Type: " << it->second.Type() <<  std::endl;
        switch(it->second.Type()) {
            case YAML::NodeType::Map:
                output[key] = yamlMapToJsonValue(it->second);
                break;
            case YAML::NodeType::Sequence:
                output[key] = yamlSequenceToJsonValue(it->second);
                break;
            case YAML::NodeType::Scalar:
                try {
                    output[key] = Json::Value(it->second.as<std::string>());
                } catch(YAML::TypedBadConversion<std::string>& e) {
                    std::cout << "Failed YAML to-string conversion for " << key << ": " << e.what() << std::endl;
                }
                break;
            case YAML::NodeType::Null:
                output[key] = Json::Value(Json::nullValue);
            case YAML::NodeType::Undefined:
               std::cout << "Error: YAML value for " << key << " is undefined" << std::endl;
               break;

        }
    }
#else
    if (node.Type() == YAML::NodeType::Map) {
        for(YAML::Iterator it=node.begin(); it!=node.end(); ++it) {
            if (it.first().Type() == YAML::NodeType::Scalar) {
                std::string key;
                it.first() >> key;
                if (it.second().Type() == YAML::NodeType::Map) {
                    output[key] = yamlMapToJsonValue(it.second());
                } else if (it.second().Type() == YAML::NodeType::Sequence) {
                    output[key] = yamlSequenceToJsonValue(it.second());
                } else if (it.second().Type() == YAML::NodeType::Scalar) {
                    std::string value;
                    it.second() >> value;
                    output[key] = value;
                }
            } else {
                std::cout << "Error: YAML key is not scalar" << std::endl;
            }
        }
    }
#endif
    return output;
}

static Json::Value yamlSequenceToJsonValue(const YAML::Node& node) {
    Json::Value output(Json::arrayValue);

#if YAML_CPP_VERSION >= 5
    for (YAML::const_iterator it=node.begin();it!=node.end();++it) {
    switch (it->Type()) {
        case YAML::NodeType::Map:
            output.append(yamlMapToJsonValue(it->second));
            break;
        case YAML::NodeType::Sequence:
            output.append(yamlSequenceToJsonValue(it->second));
            break;
        case YAML::NodeType::Scalar:
            try {
                output.append(Json::Value(it->as<std::string>()));
            } catch(YAML::TypedBadConversion<std::string>& e) {
                std::cout << "Failed YAML to-string conversion for array value: " << e.what() << std::endl;
            }
            break;

        case YAML::NodeType::Null:
            output.append(Json::Value(Json::nullValue));
            break;
        case YAML::NodeType::Undefined:
           std::cout << "Error: YAML value in array is undefined" << std::endl;
           break;
    }
}
#else
    if (node.Type() == YAML::NodeType::Sequence) {
        for(unsigned int i=0; i<node.size(); i++) {
            if (node[i].Type() == YAML::NodeType::Sequence) {
                output.append(yamlSequenceToJsonValue(node[i]));
            } else if (node[i].Type() == YAML::NodeType::Map) {
                output.append(yamlMapToJsonValue(node[i]));
            } else if (node[i].Type() == YAML::NodeType::Scalar) {
                std::string value;
                node[i] >> value;
                output.append(Json::Value(value));
            }
        }
    }
#endif
    return output;
}

Json::Value parseSchema(const fs::path &file) {
#if YAML_CPP_VERSION >= 5
    YAML::Node schema = YAML::LoadFile(file.string());
    return yamlMapToJsonValue(schema);
#else
    std::ifstream fin(file.string());
    YAML::Parser parser(fin);
    Json::Value schema;
    YAML::Node doc;
    while(parser.GetNextDocument(doc)) {
        if (doc.Type() == YAML::NodeType::Map) {
            schema = yamlMapToJsonValue(doc);
        }
    }
    return schema;
#endif
}

