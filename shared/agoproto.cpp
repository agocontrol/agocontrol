#include <stdexcept>
#include <sstream>
#include "agoproto.h"
#include "agojson.h"

static const Json::Value EMPTY_DATA = Json::Value();
static const std::string EMPTY_STRING;
Json::Value agocontrol::responseResult(const std::string& identifier)
{
    return responseResult(identifier, EMPTY_STRING, EMPTY_DATA);
}

Json::Value agocontrol::responseResult(const std::string& identifier, const std::string& message)
{
    return responseResult(identifier, message, EMPTY_DATA);
}

Json::Value agocontrol::responseResult(const std::string& identifier, const Json::Value& data)
{
    return responseResult(identifier, EMPTY_STRING, data);
}

Json::Value agocontrol::responseResult(const std::string& identifier, const std::string& message, const Json::Value& data)
{
    Json::Value result;

    if (identifier.empty())
        throw std::invalid_argument("Response without identifier not permitted");

    result["identifier"] = identifier;

    if(!message.empty())
        result["message"] = message;

    if(!data.empty())
        result["data"] = data;

    Json::Value response;
    response["result"] = result;
    response["_newresponse"] = true; // TODO: remove thits after everything is using new response style
    return response;
}

Json::Value agocontrol::responseError(const std::string& identifier, const std::string& message)
{
    return responseError(identifier, message, EMPTY_DATA);
}

Json::Value agocontrol::responseError(const std::string& identifier, const std::string& message, const Json::Value& data)
{
    Json::Value error;

    if (identifier.empty())
        throw std::invalid_argument("Response without identifier not permitted");

    if (message.empty())
        throw std::invalid_argument("Error response without message not permitted");

    error["identifier"] = identifier;
    error["message"] = message;

    if(!data.empty())
        error["data"] = data;

    Json::Value response;
    response["error"] = error;
    response["_newresponse"] = true; // TODO: remove thits after everything is using new response style
    return response;
}


/* Shortcuts to respond with basic FAILED error */

Json::Value agocontrol::responseFailed(const std::string& message)
{
    return responseError(RESPONSE_ERR_FAILED, message, EMPTY_DATA);
}

Json::Value agocontrol::responseFailed(const std::string& message, const Json::Value& data)
{
    return responseError(RESPONSE_ERR_FAILED, message, data);
}

/* Shortcuts to respond with basic SUCCESS result */
Json::Value agocontrol::responseSuccess()
{
    return responseResult(RESPONSE_SUCCESS, EMPTY_STRING, EMPTY_DATA);
}

Json::Value agocontrol::responseSuccess(const char * message)
{
    return responseResult(RESPONSE_SUCCESS, std::string(message), EMPTY_DATA);
}

Json::Value agocontrol::responseSuccess(const std::string& message)
{
    return responseResult(RESPONSE_SUCCESS, message, EMPTY_DATA);
}

Json::Value agocontrol::responseSuccess(const Json::Value& data)
{
    return responseResult(RESPONSE_SUCCESS, EMPTY_STRING, data);
}

Json::Value agocontrol::responseSuccess(const std::string& message, const Json::Value& data)
{
    return responseResult(RESPONSE_SUCCESS, message, data);
}


/* Helper to check incoming messages */
void agocontrol::checkMsgParameter(const Json::Value& content, const std::string& key) {
    if(!content.isMember(key)) {
        std::stringstream err;
        err << "Parameter " << key << " is required";
        throw AgoCommandException(RESPONSE_ERR_PARAMETER_MISSING, err.str());
    }
}
void agocontrol::checkMsgParameter(const Json::Value& content, const std::string& key,
        Json::ValueType type, bool allowEmpty) {
    checkMsgParameter(content, key);

    Json::ValueType msgType = content[key].type();
    if(!content[key].isConvertibleTo(type)) {
        std::stringstream err;
        err << "Parameter " << key << " has invalid type";
        throw AgoCommandException(RESPONSE_ERR_PARAMETER_INVALID, err.str());
    }

    if(msgType == Json::stringValue && !allowEmpty && content[key].asString().empty()) {
        std::stringstream err;
        err << "Parameter " << key << " must not be empty";
        throw AgoCommandException(RESPONSE_ERR_PARAMETER_INVALID, err.str());
    }
}

agocontrol::AgoResponse::AgoResponse(agocontrol::AgoResponse&& rhs) noexcept {
    root = std::move(rhs.root);
    response = std::move(rhs.response);
}

agocontrol::AgoResponse& agocontrol::AgoResponse::operator=(agocontrol::AgoResponse&& rhs) noexcept {
    root = std::move(rhs.root);
    response = std::move(rhs.response);
    return *this;
}

void agocontrol::AgoResponse::init(const Json::Value& response_) {
    response = response_;
    validate();
}

void agocontrol::AgoResponse::validate() {
    if(isError() && isOk())
        throw std::invalid_argument("error and result are mutually exclusive");

    if(isOk()) {
        if(response["result"].type() != Json::objectValue)
            throw std::invalid_argument("result must be map");

        root = response["result"];
    }
    else if(isError()) {
        if(response["error"].type() != Json::objectValue)
            throw std::invalid_argument("error must be map");

        root = response["error"];
    }
    else {
        throw std::invalid_argument("error or result must be set");
    }

    if(!root.isMember("identifier"))
        throw std::invalid_argument("identifier must be set");

    if(isError()) {
        if(!root.isMember("message"))
            throw std::invalid_argument("error.message must be set");
    }

    if(root.isMember("data") && root["data"].type() != Json::objectValue)
        throw std::invalid_argument("data must be a map");
}


bool agocontrol::AgoResponse::isError() const {
    return response.isMember("error") == 1;
}

bool agocontrol::AgoResponse::isOk() const {
    return response.isMember("result") == 1;
}

std::string agocontrol::AgoResponse::getIdentifier() /*const*/ {
    return root["identifier"].asString();
}

std::string agocontrol::AgoResponse::getMessage() /*const*/ {
    if(root.isMember("message"))
        return root["message"].asString();
    else
        return std::string();
}


const Json::Value& agocontrol::AgoResponse::getData() /*const*/ {
    if(root.isMember("data"))
        return root["data"];
    else
        return EMPTY_DATA;
}

