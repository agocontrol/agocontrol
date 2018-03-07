/* Json Helpers */

#include <boost/filesystem/fstream.hpp>
#include <boost/lexical_cast.hpp>

#include "agojson.h"
#include "agolog.h"


bool agocontrol::readJsonFile(Json::Value& root, const boost::filesystem::path &filename) {
    try {
        boost::filesystem::ifstream f;
        f.exceptions(std::ios_base::failbit | std::ios_base::badbit);
        f.open(filename, std::ifstream::binary);

        Json::CharReaderBuilder builder;
        std::string errors;
        if (!Json::parseFromStream(builder, f, &root, &errors)) {
            AGO_ERROR() << "Failed to parse " << filename << ": " << errors;
            return false;
        }
        return true;
    } catch (const Json::RuntimeError& ex) {
        AGO_ERROR() << "Failed to parse " << filename << ": " << ex.what();
        return false;
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

        Json::StreamWriterBuilder builder;
        builder.settings_["indentation"] = "";
        std::unique_ptr<Json::StreamWriter> writer (builder.newStreamWriter());
        writer->write(root, &f);
        return true;
    } catch (const std::ios_base::failure& ex) {
        AGO_ERROR() << "Failed to write " << filename << ": " << std::strerror(errno);
        return false;
    }
}

namespace {
// Mimics Variant::parse
    const std::string TRUE_STRING("True");
    const std::string FALSE_STRING("False");

    bool same_char(char a, char b) {
        return toupper(a) == toupper(b);
    }

    bool caseInsensitiveMatch(const std::string &a, const std::string &b) {
        return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), &same_char);
    }
};

Json::Value agocontrol::parseToJson(const std::string& s)
{
    Json::Int intValue;
    if(boost::conversion::try_lexical_convert(s, intValue)) {
        if(intValue >= 0) {
            Json::UInt uintValue;
            if(boost::conversion::try_lexical_convert(s, uintValue))
                return Json::Value(uintValue);
        }
        return Json::Value(intValue);
    }

    double realValue;
    if(boost::conversion::try_lexical_convert(s, realValue))
        return Json::Value(realValue);

    if (caseInsensitiveMatch(s, TRUE_STRING))
        return Json::Value(true);
    if (caseInsensitiveMatch(s, FALSE_STRING))
        return Json::Value(false);

    return Json::Value(s);
}

bool agocontrol::stringToUInt(const Json::Value& in, Json::UInt& out) {
    if (in.isIntegral() && in.isConvertibleTo(Json::uintValue)) {
        out = in.asUInt();
        return true;
    }

    Json::Int intValue;
    if(boost::conversion::try_lexical_convert(in.asString(), intValue)) {
        if(intValue >= 0) {
            if(boost::conversion::try_lexical_convert(in.asString(), out))
                return true;
        }
    }

    return false;
}

bool agocontrol::stringToInt(const Json::Value& in, Json::Int& out) {
    if (in.isIntegral()) {
        out = in.asInt();
        return true;
    }

    return boost::conversion::try_lexical_convert(in.asString(), out);
}

bool agocontrol::stringToDouble(const Json::Value& in, double& out) {
    if (in.isDouble()) {
        out = in.asDouble();
        return true;
    }

    return boost::conversion::try_lexical_convert(in.asString(), out);
}