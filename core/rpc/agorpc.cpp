/*
   Copyright (C) 2012 Harald Klein <hari@vt100.at>

   This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

   See the GNU General Public License for more details.

   this is a lightweight RPC/HTTP interface for ago control for platforms where the regular cherrypy based admin interface is too slow
   */

#include <errno.h>

#include <deque>
#include <map>
#include <string>

#include <qpid/messaging/Message.h>

#include <json/reader.h>
#include <json/writer.h>

#include <boost/foreach.hpp>
#include <boost/filesystem.hpp>
#include <boost/preprocessor/stringize.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/locks.hpp>
#include <boost/tokenizer.hpp>

#include "agoapp.h"
#include "agohttp/agohttp.h"

#define GETEVENT_DEFAULT_TIMEOUT_SECONDS 28 // long-polling getevent

//upload file path
#define UPLOAD_PATH "/tmp/"

//default auth file
#define HTPASSWD ".htpasswd"

/* JSON-RPC 2.0 standard error codes
 * http://www.jsonrpc.org/specification#error_object
 */
#define JSONRPC_PARSE_ERROR -32700
#define JSONRPC_INVALID_REQUEST -32600
#define JSONRPC_METHOD_NOT_FOUND -32601
#define JSONRPC_INVALID_PARAMS -32602
#define JSONRPC_INTERNAL_ERROR -32603

// -32000 to -32099 impl-defined server errors
#define AGO_JSONRPC_NO_EVENT            -32000
#define AGO_JSONRPC_MESSAGE_ERROR       -32001
#define AGO_JSONRPC_COMMAND_ERROR       -31999

namespace fs = ::boost::filesystem;
using namespace agocontrol;
using namespace agocontrol::agohttp;

// struct and map for json-rpc event subscriptions
struct Subscriber
{
    std::deque<qpid::types::Variant::Map> queue;
    time_t lastAccess;
};

class AgoRpc;

class JsonRpcReqRep : public HttpReqJsonRep {
friend class AgoRpc;
protected:
    AgoRpc *appInstance;

    // Raw JsonRCP request & response
    Json::Value jsonrpcRequest;

    // For GetEvent call, subscriptionId.
    // No batching supported for getEvent
    std::string subscriptionId;

    uint64_t timeout;

public:
    JsonRpcReqRep(AgoRpc* _appInstance)
        : appInstance(_appInstance)
        , timeout(GETEVENT_DEFAULT_TIMEOUT_SECONDS*1000)
    {
        // JsonRPC does not know of anything else
        setResponseCode(200);
    }

    bool isResponseReady();
    bool isResponseReady(bool isTimeout);
    void onTimeout();
    uint16_t getTimeout();
};

class FileDownloadReqRep : public HttpReqRep {
public:
    FileDownloadReqRep(AgoRpc* _appInstance, struct http_message* _hm_req)
        : appInstance(_appInstance)
        , hm_req(_hm_req)
    {
    }

    // Reading appInstance/requests should be safe without holding mutex.
    // Writing to response is NOT safe.

    AgoRpc *appInstance;

    struct http_message* hm_req;
    qpid::types::Variant::Map request;
    std::string error;

    fs::path filepath;

    void writeResponseData(struct mg_connection *conn);
    bool isResponseReady() { return (getResponseCode() != 0); }
    void onTimeout();
};

class FileUploadReqRep : public HttpReqJsonRep {
public:
    FileUploadReqRep(AgoRpc* _appInstance, struct http_message* _hm_req)
        : appInstance(_appInstance)
    {
    }

    // Reading appInstance/requests should be safe without holding mutex.
    // Writing to response is NOT safe.

    AgoRpc *appInstance;

    qpid::types::Variant::List requests;
    void onTimeout();
};

// helper to determine last element
#ifndef _LIBCPP_ITERATOR
template <typename Iter>
Iter next(Iter iter)
{
    return ++iter;
}
#endif


class AgoRpc: public AgoApp {
private:
    std::map<std::string, Subscriber> subscriptions;
    boost::mutex mutexSubscriptions;

    AgoHttp agoHttp;


    bool getEventsFor(JsonRpcReqRep* reqRep, bool isTimeout=false);
    bool handleJsonRpcRequest(JsonRpcReqRep *reqRep, const Json::Value &request, Json::Value &responseRoot);
    bool handleJsonRpcRequests(boost::shared_ptr<JsonRpcReqRep> reqRep);

    boost::shared_ptr<JsonRpcReqRep> jsonrpc(struct mg_connection *conn, struct http_message *hm);

    boost::shared_ptr<HttpReqRep> downloadFile(struct mg_connection *conn, struct http_message *hm);
    void downloadFile_thread(boost::shared_ptr<FileDownloadReqRep> reqRep);

    boost::shared_ptr<HttpReqRep> uploadFiles(struct mg_connection *conn, struct http_message *hm);
    void uploadFile_thread(boost::shared_ptr<FileUploadReqRep> reqRep);

    void eventHandler(std::string subject, qpid::types::Variant::Map content) ;

    void jsonrpc_message(JsonRpcReqRep* reqRep, boost::unique_lock<boost::mutex> &lock, const Json::Value& params, Json::Value& responseRoot);
    void jsonrpc_thread(boost::shared_ptr<JsonRpcReqRep> conn);

    void setupApp();

    void doShutdown();
    void cleanupApp();
public:
    AGOAPP_CONSTRUCTOR_HEAD(AgoRpc) {}

friend class JsonRpcReqRep;
};


static bool jsonrpcErrorResponse(Json::Value& responseRoot, int code, const std::string& message) {
    assert(responseRoot.isObject());
    if(!responseRoot.isMember("jsonrpc"))
        responseRoot["jsonrpc"] = "2.0";

    if(!responseRoot.isMember("id"))
        responseRoot["id"] = Json::Value();

    Json::Value error(Json::objectValue);
    error["code"] = code;
    error["message"] = message;
    responseRoot["error"] = error;
    return true;
}


// Lock shall be held on entry and exit, and should be released when not interacting with reqRep
// or responseRoot
void AgoRpc::jsonrpc_message(JsonRpcReqRep* reqRep, boost::unique_lock<boost::mutex> &lock, const Json::Value& params, Json::Value& responseRoot) {
    // prepare message
    const Json::Value& content = params["content"];
    const Json::Value& subject = params["subject"];
    qpid::types::Variant::Map command = jsonToVariantMap(content);

    const Json::Value& replytimeout = params["replytimeout"];
    qpid::messaging::Duration timeout = qpid::messaging::Duration::SECOND * 3;
    if (replytimeout.isNumeric()) {
        timeout = qpid::messaging::Duration(replytimeout.asDouble() * 1000);
    }
    //send message and handle response
    AGO_TRACE() << "Request on " << reqRep << ": " << command << "(timeout=" << timeout.getMilliseconds() << " : << "<<replytimeout<< ")";

    agocontrol::AgoResponse response;
    try {
        lock.unlock();
        response = agoConnection->sendRequest(subject.asString(), command, timeout);
    } catch(...) {
        lock.lock();
        throw;
    }

    AGO_TRACE() << "Response: " << response.getResponse();
    variantMapToJson(response.getResponse(), responseRoot);
}


/**
 * If handleJsonRpcRequests decided we could not be executed synchronously, it will
 * dispatch the rest of the work in a pooled thread:
 */
void AgoRpc::jsonrpc_thread(boost::shared_ptr<JsonRpcReqRep> reqRep) {
    AGO_TRACE() << "Entering jsonrcp thread for " << reqRep.get();
    boost::unique_lock<boost::mutex> lock(reqRep->mutex);
    // Execute any remote calls
    if (reqRep->jsonrpcRequest.isArray())
    {
        // Batch mode: array of events, find out which are "message"
        // Abort if response is ready (timeout)
        for (unsigned int i = 0; i< reqRep->jsonrpcRequest.size() && !reqRep->responseReady; i++) {
            if(reqRep->jsonrpcRequest[i]["method"] != "message")
                continue;

            const Json::Value& params = reqRep->jsonrpcRequest[i]["params"];

            Json::Value& responseRoot(reqRep->jsonResponse[i]);
            jsonrpc_message(reqRep.get(), lock, params, responseRoot);
        }
    }
    else
    {
        assert(reqRep->jsonrpcRequest["method"] == "message");
        const Json::Value& params = reqRep->jsonrpcRequest["params"];
        jsonrpc_message(reqRep.get(), lock, params, reqRep->jsonResponse);
    }

    // Should be done now
    reqRep->responseReady = true;

    // Wakeup mongoose main poll loop, it will POLL our client
    // and write the response.
    AGO_TRACE() << "Response stored, Exiting jsonrpc thread for " << reqRep.get();
    agoHttp.wakeup();
}


bool AgoRpc::getEventsFor(JsonRpcReqRep* reqRep, bool isTimeout) {
    // Do we have any event pending?
    boost::unique_lock<boost::mutex> lock(mutexSubscriptions);
    std::map<std::string, Subscriber>::iterator it = subscriptions.find(reqRep->subscriptionId);
    if (it == subscriptions.end()) {
        AGO_TRACE() << "getEventsFor " << reqRep <<": unknown subscription ID " <<reqRep->subscriptionId;
        return jsonrpcErrorResponse(reqRep->jsonResponse, JSONRPC_INVALID_PARAMS, "Invalid request, no subscription for uuid");
    }

    if(!(it->second.queue.empty())) {
        qpid::types::Variant::Map event = it->second.queue.front();
        it->second.queue.pop_front();
        lock.unlock();

        AGO_TRACE() << "getEventsFor " << reqRep <<": found event";

        // Write event
        reqRep->jsonResponse["result"] = Json::Value(Json::objectValue);
        // TODO: use Json swap method when event is JSON to avoid cloning
        variantMapToJson(event, reqRep->jsonResponse["result"]);
        return true;
    }

    // No event.
    // Is it time to tell client to quit anyway?
    if(isExitSignaled() || isTimeout) {
        AGO_TRACE() << "getEventsFor " << reqRep <<": timeout";
        return jsonrpcErrorResponse(reqRep->jsonResponse, AGO_JSONRPC_NO_EVENT, "No messages available");
    }

    AGO_TRACE() << "getEventsFor " << reqRep <<": no events (timeout=" << reqRep->timeout << ")";

    // Nothing to report yet.
    return false;
}

bool JsonRpcReqRep::isResponseReady() {
    return isResponseReady(false);
}

void JsonRpcReqRep::onTimeout() {
    // Triggers immediate write, unless already written.
    isResponseReady(true);
}

bool JsonRpcReqRep::isResponseReady(bool isTimeout) {
    if(subscriptionId.empty() || responseReady)
    {
        if(isTimeout)
            // Ensures an array of jsonrpc messages are not continued on
            // in jsonrpc_thread
            responseReady = true;
        return responseReady;
    }

    // If there is a pending getEvent call for this connection, this may set responseReady
    // with either an event, or timeout error.
    return appInstance->getEventsFor(this, isTimeout);
}

uint16_t JsonRpcReqRep::getTimeout() {
    return timeout;
}

/**
 * Try to execute this request, if it can be done locally.
 * If it requries a remote call, do nothing, it will be executed in background thread instead.
 *
 * request/response is passed explicitly here since we might be part of a batch.
 *
 * @param reqRep
 * @param request The particular request object we're parsing
 * @param responseRoot Json::Value object where result should be placed
 * @return True if process is finished and resonse is ready to send, else more processing is required.
 */
bool AgoRpc::handleJsonRpcRequest(JsonRpcReqRep *reqRep, const Json::Value &request, Json::Value &responseRoot) {
    assert(responseRoot.isObject());
    AGO_TRACE() << "JSONRPC request: " << request;

    if (!request.isObject())
        return jsonrpcErrorResponse(responseRoot, JSONRPC_INVALID_REQUEST, "Invalid request, not an object");

    // If ID is missing, this is a notification and no response should be sent (unless error).
    if(!request.isMember("id")) {
        //... but we do not have any methods which are notifications..
        jsonrpcErrorResponse(responseRoot, JSONRPC_INVALID_REQUEST, "Invalid request, 'id' missing");
    }
    responseRoot["id"] = request["id"];

    // Version is required
    if(!request.isMember("jsonrpc") || request["jsonrpc"].asString() != "2.0")
        return jsonrpcErrorResponse(responseRoot, JSONRPC_INVALID_REQUEST, "Invalid request, 'jsonrpc' unknown/missing");
    responseRoot["jsonrpc"] = request["jsonrpc"];

    // Method is required
    const Json::Value &methodV = request["method"];
    if(!methodV.isString())
        return jsonrpcErrorResponse(responseRoot, JSONRPC_INVALID_REQUEST, "Invalid request, 'method' invalid/missing");

    const std::string method = methodV.asString();

    // Params may or may not be required depending on method
    if (method == "message" )
    {
        const Json::Value& params = request["params"];
        if (!params.isObject() || params.empty())
            return jsonrpcErrorResponse(responseRoot, JSONRPC_INVALID_PARAMS, "Invalid request, 'params' invalid/missing");

        // This cannot be processed remote remote call
        return false;
    }
    else if (method == "subscribe")
    {
        // Local call possible
        const std::string subscriptionId = generateUuid();
        if (subscriptionId == "")
            return jsonrpcErrorResponse(responseRoot, JSONRPC_INTERNAL_ERROR, "Failed to generate UUID");

        std::deque<qpid::types::Variant::Map> empty;
        Subscriber subscriber;
        subscriber.lastAccess=time(0);
        subscriber.queue = empty;
        {
            boost::lock_guard<boost::mutex> lock(mutexSubscriptions);
            subscriptions[subscriptionId] = subscriber;
        }
        responseRoot["result"] = subscriptionId;
        return true;

    }
    else if (method == "unsubscribe" || method == "getevent")
    {
        const Json::Value& params = request["params"];
        if (!params.isObject() || params.empty())
            return jsonrpcErrorResponse(responseRoot, JSONRPC_INVALID_PARAMS, "Invalid request, 'params' invalid/missing");

        const Json::Value& subscriptionId = params["uuid"];
        if (!subscriptionId.isString())
            return jsonrpcErrorResponse(responseRoot, JSONRPC_INVALID_PARAMS, "Invalid request, param 'uuid' missing");

        if (method == "unsubscribe") {
            AGO_DEBUG() << "removing subscription: " << subscriptionId.asString();
            {
                boost::lock_guard <boost::mutex> lock(mutexSubscriptions);
                std::map<std::string, Subscriber>::iterator it = subscriptions.find(subscriptionId.asString());
                if (it != subscriptions.end()) {
                    subscriptions.erase(subscriptionId.asString());
                }
            }

            responseRoot["result"] = "success";
            return true;
        }else {
            // getevent
            if(!reqRep->jsonrpcRequest.isObject()) {
                return jsonrpcErrorResponse(responseRoot, JSONRPC_INVALID_REQUEST, "Invalid request, getevent is not batchable");
            }

            if(params.isMember("timeout"))
                // Custom timeout in seconds
                reqRep->timeout = params["timeout"].asDouble()*1000;

            reqRep->subscriptionId = subscriptionId.asString();

            // If we have pending events for this subscription, it returns immediately
            return getEventsFor(reqRep);
        }
    }

    return jsonrpcErrorResponse(responseRoot, JSONRPC_METHOD_NOT_FOUND, "Invalid request, method not found");
}

/**
 * Verify all JSONRPC requests in jsonrpcRequest, and determine if any requires a remote
 * call/background thread.
 *
 * @param reqRep
 * @return
 */
bool AgoRpc::handleJsonRpcRequests(boost::shared_ptr<JsonRpcReqRep> reqRep) {
    bool finished;
    if (reqRep->jsonrpcRequest.isArray())
    {
        // Batch, array of events
        reqRep->jsonResponse = Json::Value(Json::arrayValue);
        finished = true;
        for (unsigned int i = 0; i< reqRep->jsonrpcRequest.size(); i++) {
            reqRep->jsonResponse[i] = Json::Value(Json::objectValue);
            if(!handleJsonRpcRequest(reqRep.get(), reqRep->jsonrpcRequest[i], reqRep->jsonResponse[i])) {
                // Not finished; background work required
                finished = false;
            }
        }
    }
    else
    {
        reqRep->jsonResponse = Json::Value(Json::objectValue);
        finished = handleJsonRpcRequest(reqRep.get(), reqRep->jsonrpcRequest, reqRep->jsonResponse);
    }

    // Special polled case, when we have subscriptionId we do not need background thread.
    if(!finished && reqRep->subscriptionId.empty()) {
        threadPool().post(boost::bind(&AgoRpc::jsonrpc_thread, this, reqRep));
    }

    return finished;
}

/**
 * Prepare processing of one or more JSONRPC requests
 */
boost::shared_ptr<JsonRpcReqRep> AgoRpc::jsonrpc(struct mg_connection *conn, struct http_message *hm)
{
    boost::shared_ptr<JsonRpcReqRep> reqRep(new JsonRpcReqRep(this));
    Json::Reader reader;
    if ( !reader.parse(hm->body.p, hm->body.p + hm->body.len, reqRep->jsonrpcRequest, false) ) {
        reqRep->jsonResponse = Json::Value(Json::objectValue);
        reqRep->responseReady = jsonrpcErrorResponse(reqRep->jsonResponse, JSONRPC_PARSE_ERROR, "Failed to parse JSON request");
    } else {
        reqRep->responseReady = handleJsonRpcRequests(reqRep);
    }

    return reqRep;
}

/**
 * Upload files
 * @info source from https://github.com/cesanta/mongoose/blob/master/examples/upload.c
 */
boost::shared_ptr<HttpReqRep> AgoRpc::uploadFiles(struct mg_connection *conn, struct http_message *hm)
{
    boost::shared_ptr<FileUploadReqRep> reqRep(
            new FileUploadReqRep(this, hm)
        );

    const char *data;
    size_t data_len, ofs = 0;
    char var_name[100], file_name[100];
    std::string uuid = "";

#define FILE_UPLOAD_ERROR(error_message)  \
    response["error"] = Json::Value(Json::objectValue); \
    response["error"]["message"] = (error_message);

    // Index matches "requests"
    reqRep->jsonResponse["files"] = Json::Value(Json::arrayValue);

    // upload files
    while ((ofs = mg_parse_multipart(hm->body.p + ofs, hm->body.len - ofs, var_name, sizeof(var_name),
                    file_name, sizeof(file_name), &data, &data_len)) > 0)
    {
        if( strlen(file_name)>0 )
        {
            //check if uuid found
            if(uuid.size() == 0)
            {
                //no uuid found yet, drop file
                continue;
            }

            // One response per valid object, we put either result or error in this (as received from remote)
            Json::Value response(Json::objectValue);

            // at same index as in jsonrcpResponse, we have our request
            // this is the actual message sent to device
            qpid::types::Variant::Map request;
            request["uuid"] = std::string(uuid);
            request["command"] = "uploadfile";

            // Sanitize filename, it should only be a filename.
            fs::path orig_fn(file_name);
            fs::path safe_fn = orig_fn.filename();
            if(std::string(file_name) != safe_fn.string()){
                AGO_ERROR() << "Rejecting file upload, unsafe path \"" << file_name << "\" ";
                FILE_UPLOAD_ERROR("Invalid filename");
            }else{
                response["name"] = safe_fn.string();

                // Save file to a temporary path
                fs::path tempfile = fs::path(UPLOAD_PATH) / fs::unique_path().replace_extension(safe_fn.extension());
                FILE* fp = fopen(tempfile.c_str(), "wb");
                if( !fp )
                {
                    std::string err(strerror(errno));
                    AGO_ERROR() << "Failed to open file " << tempfile.string() << " for writing: " << err;
                    FILE_UPLOAD_ERROR(std::string("Failed to open file: ") + err);
                }else {
                    request["filepath"] = tempfile.string();
                    request["filename"] = safe_fn.string();

                    AGO_DEBUG() << "Uploading file \"" << safe_fn.string() << "\" file to " << uuid << " via " << tempfile;
                    size_t written = fwrite(data, sizeof(char), data_len, fp);
                    fclose(fp);
                    if( written!=data_len )
                    {
                        //error writting file, drop it
                        fs::remove(tempfile);
                        AGO_ERROR() << "Uploaded file \"" << tempfile.string() << "\" not fully written (no space left?)";
                        FILE_UPLOAD_ERROR("Failed to write file, no space left?");
                    }else{
                        request["filesize"] = data_len;
                        response["size"] = (Json::UInt64)data_len;
                    }
                }
            }

            reqRep->requests.push_back(request);
            reqRep->jsonResponse["files"].append(response);
        }
        else
        {
            //it's a posted value
            if( strcmp(var_name, "uuid")==0 )
            {
                uuid = std::string(data, data_len);
            }
        }
    }

#undef FILE_UPLOAD_ERROR

    // Dispatch remote request in background thread.
    threadPool().post(boost::bind(&AgoRpc::uploadFile_thread, this, reqRep));

    return reqRep;
}

void AgoRpc::uploadFile_thread(boost::shared_ptr<FileUploadReqRep> reqRep) {
    int i=0;
    BOOST_FOREACH(qpid::types::Variant &request_, reqRep->requests) {
        qpid::types::Variant::Map &request(request_.asMap());
        std::string tempfile(request["filepath"]);
        AgoResponse r = agoConnection->sendRequest(request);
        if(r.isError())
            AGO_ERROR() << "Uploading file \"" << tempfile << "\" failed: " << r.getMessage();
        else if( r.isOk() )
            AGO_INFO() << "Uploading file " << request["filename"] << " was successful";

        boost::unique_lock<boost::mutex> lock(reqRep->mutex);
        // Copy full remote result 1:1, adds result or error.
        variantMapToJson(r.getResponse(), reqRep->jsonResponse["files"][i++]);

        // delete file (it should be processed by sendcommand)
        //XXX: maybe a purge process could be interesting to implement
        fs::remove(tempfile);
    }

    boost::unique_lock<boost::mutex> lock(reqRep->mutex);
    reqRep->jsonResponse["count"] = i;
    reqRep->responseReady = true;

    AGO_TRACE() << "Leaving upload thread " << reqRep.get();
    agoHttp.wakeup();
}

void FileUploadReqRep::onTimeout() {
    setResponseCode(503);
    jsonResponse["error"] = "Backend timeout";
}

/**
 * downloadFile is implemented by sending a downloadfile message to the target device,
 * which returns a "filepath" response pointing to a file on the local filesystem.
 *
 * This is then served to the client.
 * TODO XXX: Without any filtering on what filepath is... can theoretically read anything.
 */
boost::shared_ptr<HttpReqRep> AgoRpc::downloadFile(struct mg_connection *conn, struct http_message *hm)
{
    boost::shared_ptr<FileDownloadReqRep> reqRep(
            new FileDownloadReqRep(this, hm)
        );

    // Verify parameters first, then dispatch to background thread.
    char param[1024];
    Json::Value content;

    // get params
    if( mg_get_http_var(&hm->query_string, "filename", param, sizeof(param)) > 0 )
        reqRep->request["filename"] = std::string(param);

    if( mg_get_http_var(&hm->query_string, "uuid", param, sizeof(param)) > 0 )
        reqRep->request["uuid"] = std::string(param);

    if( reqRep->request["filename"].isVoid() || reqRep->request["uuid"].isVoid() )
    {
        //missing parameters!
        AGO_ERROR() << "Download file, missing parameters. Nothing done";
        reqRep->setResponseCode(400);
        reqRep->error = "Invalid request parameters";
        return reqRep;
    }

    reqRep->request["command"] = "downloadfile";

    // Dispatch remote request in background thread.
    threadPool().post(boost::bind(&AgoRpc::downloadFile_thread, this, reqRep));

    return reqRep;
}

void AgoRpc::downloadFile_thread(boost::shared_ptr<FileDownloadReqRep> reqRep) {
    AGO_TRACE() << "Entering downloadFile_thread for " << reqRep.get();
    // send command
    AgoResponse r = agoConnection->sendRequest(reqRep->request);

    boost::unique_lock<boost::mutex> lock(reqRep->mutex);
    if( r.isOk() )
    {
        qpid::types::Variant::Map responseMap = r.getData();

        //command sent successfully
        if( !responseMap["filepath"].isVoid() && responseMap["filepath"].asString().length()>0 )
        {
            // 404??
            // all seems valid
            reqRep->filepath = fs::path(responseMap["filepath"].asString());
            AGO_DEBUG() << "Downloading file " << reqRep->filepath;
            reqRep->setResponseCode(200);
        }
        else
        {
            //invalid command response
            AGO_ERROR() << "Download file, sendCommand returned invalid response (need filepath)";
            reqRep->error = "Internal error";
            reqRep->setResponseCode(500);
        }
    }
    else
    {
        //command failed
        AGO_ERROR() << "Download file, sendCommand failed, unable to send file: " << r.getMessage();
        reqRep->error = r.getMessage();
        reqRep->setResponseCode(500);
    }

    AGO_TRACE() << "Leaving downloadFile_thread for " << reqRep.get();
    // will trigger writeResponseData from mongoose thread.
    agoHttp.wakeup();
}

void FileDownloadReqRep::writeResponseData(struct mg_connection *conn) {
    if(getResponseCode() != 200) {
        AGO_TRACE() << "Writing " << this << " error response: " << error;
        mg_send_head(conn, getResponseCode(), error.size(), "Content-Type: text/plain");
        mg_send(conn, error.c_str(), error.size());
        return;
    }

    std::stringstream headers;
    headers << "Content-Disposition: attachment; filename="
        // .filename() returns filename within ""
        << filepath.filename();

    AGO_TRACE() << "Responding with file " << filepath;
    // XXX: We do not have any mime info.
    // XXX: Danger danger, filepath can be anything given by remote agoapp
    mg_http_serve_file(conn, hm_req, filepath.c_str(),
            mg_mk_str("application/octet-stream"),
            mg_mk_str(headers.str().c_str()));
}

void FileDownloadReqRep::onTimeout() {
    setResponseCode(503);
    error = "Backend timeout";
}

/**
 * Agoclient event handler
 */
void AgoRpc::eventHandler(std::string subject, qpid::types::Variant::Map content) {
    // don't flood clients with unneeded events
    if (subject == "event.environment.timechanged" || subject == "event.device.discover") {
        return;
    }

    //remove empty command from content
    qpid::types::Variant::Map::iterator it = content.find("command");
    if (it != content.end() && content["command"].isVoid()) {
        content.erase("command");
    }

    //prepare event content
    content["event"] = subject;
    if (subject.find("event.environment.") != std::string::npos && subject.find("changed") != std::string::npos) {
        std::string quantity = subject;
        replaceString(quantity, "event.environment.", "");
        replaceString(quantity, "changed", "");
        content["quantity"] = quantity;
    } else if (subject == "event.device.batterylevelchanged") {
        std::string quantity = subject;
        replaceString(quantity, "event.device.", "");
        replaceString(quantity, "changed", "");
        content["quantity"] = quantity;
    }

    {
        boost::lock_guard <boost::mutex> lock(mutexSubscriptions);
        //AGO_TRACE() << "Incoming notify: " << content;
        for (std::map<std::string, Subscriber>::iterator it = subscriptions.begin(); it != subscriptions.end();) {
            if (it->second.queue.size() > 100) {
                // this subscription seems to be abandoned, let's remove it to save resources
                AGO_INFO() << "removing subscription as the queue size exceeds limits: " << it->first.c_str();
                subscriptions.erase(it++);
            } else {
                it->second.queue.push_back(content);
                ++it;
            }
        }
    }

    // Wakeup sleeping sockets, any event subscribers will see their updated queue.
    agoHttp.wakeup();
}

void AgoRpc::setupApp() {
    std::string ports_cfg;
    fs::path htdocs;
    fs::path certificate;
    std::string domainname;

    //get parameters
    ports_cfg = getConfigOption("ports", "8008,8009s");
    htdocs = getConfigOption("htdocs", fs::path(BOOST_PP_STRINGIZE(DEFAULT_HTMLDIR)));
    certificate = getConfigOption("certificate", getConfigPath("/rpc/rpc_cert.pem"));
    domainname = getConfigOption("domainname", "agocontrol");

    agoHttp.setDocumentRoot(htdocs.string());
    agoHttp.setAuthDomain(domainname);

    // Expose any custom python extra include paths via MONGOOSE_CGI env vars
    setenv("MONGOOSE_CGI", getConfigOption("python_extra_paths", "").c_str(), 1);

    // Parse bindings/ports
    typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
    boost::char_separator<char> sep(", ");
    tokenizer tok(ports_cfg, sep);
    for(tokenizer::iterator gen=tok.begin(); gen != tok.end(); ++gen) {
        std::string addr(*gen);
        if(addr[addr.length() -1] == 's') {
            addr.assign(addr, 0, addr.length()-1);
            agoHttp.addBinding(addr, certificate);
        }else
            agoHttp.addBinding(addr);
    }

    fs::path authPath = htdocs / HTPASSWD;
    if( fs::exists(authPath) )
        agoHttp.setAuthFile(authPath);
    else
        AGO_INFO() << "Disabling authentication: file does not exist";

    agoHttp.addHandler("/jsonrpc", boost::bind(&AgoRpc::jsonrpc, this, _1, _2));
    agoHttp.addHandler("/upload", boost::bind(&AgoRpc::uploadFiles, this, _1, _2));
    agoHttp.addHandler("/download", boost::bind(&AgoRpc::downloadFile, this, _1, _2));

    try {
        agoHttp.start();
    }catch(const std::runtime_error &err) {
        throw ConfigurationError(err.what());
    }

    addEventHandler();
}

void AgoRpc::doShutdown() {
    agoHttp.shutdown();
    AgoApp::doShutdown();
}

void AgoRpc::cleanupApp() {
    // Wait for Http to close and cleanup
    agoHttp.close();
}

AGOAPP_ENTRY_POINT(AgoRpc);

