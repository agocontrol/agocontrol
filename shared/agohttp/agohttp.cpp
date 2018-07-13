#include <boost/foreach.hpp>

#include "agolog.h"
#include "agohttp.h"

#define MONGOOSE_POLLING_INTERVAL 1000 //in ms, minimum mongoose poll interval

#define AGO_MG_F_NO_KEEPALIVE MG_F_USER_1
#define AGO_MG_F_ACTIVE MG_F_USER_2

namespace agocontrol {
namespace agohttp {

// Mg_EVENT names for debuggin.. base ones at least.. 100 = http request
const char *mg_event_name(int ev){
    switch(ev) {
        // Created with:
        // 	grep '#define MG_EV_' mongoose.h|grep -v MG_EV_MQTT_CONNACK|awk '{print "case "$2": return \""$2"\";"}'
        case MG_EV_POLL: return "MG_EV_POLL";
        case MG_EV_ACCEPT: return "MG_EV_ACCEPT";
        case MG_EV_CONNECT: return "MG_EV_CONNECT";
        case MG_EV_RECV: return "MG_EV_RECV";
        case MG_EV_SEND: return "MG_EV_SEND";
        case MG_EV_CLOSE: return "MG_EV_CLOSE";
        case MG_EV_TIMER: return "MG_EV_TIMER";
        case MG_EV_HTTP_REQUEST: return "MG_EV_HTTP_REQUEST";
        case MG_EV_HTTP_REPLY: return "MG_EV_HTTP_REPLY";
        case MG_EV_HTTP_CHUNK: return "MG_EV_HTTP_CHUNK";
        case MG_EV_SSI_CALL: return "MG_EV_SSI_CALL";
        case MG_EV_SSI_CALL_CTX: return "MG_EV_SSI_CALL_CTX";
        case MG_EV_WEBSOCKET_HANDSHAKE_REQUEST: return "MG_EV_WEBSOCKET_HANDSHAKE_REQUEST";
        case MG_EV_WEBSOCKET_HANDSHAKE_DONE: return "MG_EV_WEBSOCKET_HANDSHAKE_DONE";
        case MG_EV_WEBSOCKET_FRAME: return "MG_EV_WEBSOCKET_FRAME";
        case MG_EV_WEBSOCKET_CONTROL_FRAME: return "MG_EV_WEBSOCKET_CONTROL_FRAME";
    #ifdef MG_EV_HTTP_MULTIPART_REQUEST
        case MG_EV_HTTP_MULTIPART_REQUEST: return "MG_EV_HTTP_MULTIPART_REQUEST";
        case MG_EV_HTTP_PART_BEGIN: return "MG_EV_HTTP_PART_BEGIN";
        case MG_EV_HTTP_PART_DATA: return "MG_EV_HTTP_PART_DATA";
        case MG_EV_HTTP_PART_END: return "MG_EV_HTTP_PART_END";
        case MG_EV_HTTP_MULTIPART_REQUEST_END: return "MG_EV_HTTP_MULTIPART_REQUEST_END";
    #endif
        case MG_EV_MQTT_CONNECT: return "MG_EV_MQTT_CONNECT";
        case MG_EV_MQTT_CONNACK: return "MG_EV_MQTT_CONNACK";
        case MG_EV_MQTT_PUBLISH: return "MG_EV_MQTT_PUBLISH";
        case MG_EV_MQTT_PUBACK: return "MG_EV_MQTT_PUBACK";
        case MG_EV_MQTT_PUBREC: return "MG_EV_MQTT_PUBREC";
        case MG_EV_MQTT_PUBREL: return "MG_EV_MQTT_PUBREL";
        case MG_EV_MQTT_PUBCOMP: return "MG_EV_MQTT_PUBCOMP";
        case MG_EV_MQTT_SUBSCRIBE: return "MG_EV_MQTT_SUBSCRIBE";
        case MG_EV_MQTT_SUBACK: return "MG_EV_MQTT_SUBACK";
        case MG_EV_MQTT_UNSUBSCRIBE: return "MG_EV_MQTT_UNSUBSCRIBE";
        case MG_EV_MQTT_UNSUBACK: return "MG_EV_MQTT_UNSUBACK";
        case MG_EV_MQTT_PINGREQ: return "MG_EV_MQTT_PINGREQ";
        case MG_EV_MQTT_PINGRESP: return "MG_EV_MQTT_PINGRESP";
        case MG_EV_MQTT_DISCONNECT: return "MG_EV_MQTT_DISCONNECT";
    #ifdef MG_EV_COAP_CON
        case MG_EV_COAP_CON: return "MG_EV_COAP_CON";
        case MG_EV_COAP_NOC: return "MG_EV_COAP_NOC";
        case MG_EV_COAP_ACK: return "MG_EV_COAP_ACK";
        case MG_EV_COAP_RST: return "MG_EV_COAP_RST";
    #endif
        default:
            return "unknown event";
    }
}


/* On each mg_connection we have user_data which we use
 * to find the AgoHttp instance and any connection specific data.
 *
 * On connection init, user_data will point to a shared defaultUserData
 * object which only holds pointer to AgoHttp object.
 * If we later need to keep user-specific data between invocations
 * of mongooseEventHandler, we create a connection-specific instance
 * if MgUserData which points to both http and the HttpReqRep currently
 * active.
 */
class MgUserData {
friend class AgoHttp;
public:
    AgoHttp *http;
    // for non-shared UserData, this points to data
    boost::shared_ptr<HttpReqRep> requestData;

    MgUserData(AgoHttp *_http)
            : http(_http) {
        AGO_TRACE() << "userData created "<< this;
    }

    ~MgUserData() {
        AGO_TRACE() << "userData destroying "<< this << "with cd " << requestData.get();
    }

    MgUserData* specificFor(struct mg_connection *conn, boost::shared_ptr<HttpReqRep> requestData) {
        if(this->requestData != NULL) {
            AGO_TRACE() << "userData re-use specific "<< this;
            // Already inited / connection-specific
            this->requestData = requestData;
            return this;
        }

        MgUserData* mcw = new MgUserData(http);
        mcw->requestData = requestData;
        conn->user_data = mcw;

        AGO_TRACE() << "userData creating specific "<< mcw << " for " << requestData;
        return mcw;
    }

    // Called from static C-style mongooseEventHandler_wrapper, must be public.
    void mongooseEventHandler(struct mg_connection *conn, int ev, void *ev_data) {
        this->http->mongooseEventHandler(conn, ev, ev_data);
    }
};

static void mongooseEventHandlerWrapper(struct mg_connection *conn, int ev, void *ev_data) {
    MgUserData* ud = (MgUserData*)conn->user_data;
    ud->mongooseEventHandler(conn, ev, ev_data);
}

static void handleKeepaliveFlagAfterRequestFinish(struct mg_connection *conn) {
    // connection keepalive not wanted?
    if(conn->flags & AGO_MG_F_NO_KEEPALIVE) {
        conn->flags |= MG_F_SEND_AND_CLOSE;
    }else
        conn->flags &= ~AGO_MG_F_ACTIVE;
}



HttpReqRep::HttpReqRep() {
    AGO_TRACE() << "Connection data inited: " << this;
}

HttpReqRep::~HttpReqRep() {
    AGO_TRACE() << "Connection data destroyed: " << this;
}

void HttpReqRep::writeResponse(struct mg_connection *conn) {
    // Implementation-specific data writer
    writeResponseData(conn);
    handleKeepaliveFlagAfterRequestFinish(conn);
}



void HttpReqJsonRep::onTimeout() {
    // No "standard" json way to respond. Override onTimeout if non-empty response is desired
    setResponseCode(503);
    jsonResponse.clear();
}

void HttpReqJsonRep::writeResponseData(struct mg_connection *conn) {
    Json::StreamWriterBuilder builder;
    builder.settings_["indentation"] = "";
    std::unique_ptr<Json::StreamWriter> writer (builder.newStreamWriter());
    std::stringstream response;
    writer->write(jsonResponse, &response);
    const std::string& data(response.str());

    int status = getResponseCode();
    AGO_TRACE() << "Writing " << this << " " << status << " response: " << data; //(data.length() > 10000 ? data.substr(0, 1000) : data);
    mg_send_head(conn, status, data.size(), "Content-Type: application/json");
    mg_send(conn, data.c_str(), data.size());
}

boost::shared_ptr<HttpReqRep> AgoHttp::handleRequest(mg_connection *conn, struct http_message *req) {
    boost::shared_ptr<HttpReqRep> reqRep;

    mg_str path_str;
    mg_parse_uri( req->uri, /*scheme*/NULL, /*user_info*/NULL,
            /*host*/NULL, /*port*/NULL,
            &path_str, /*query*/NULL,
            /*fragment*/NULL);
    const std::string reqPath(path_str.p, path_str.len);

    AGO_TRACE() << "Incoming HTTP request on " << reqPath;
    std::map<std::string, agohttp_url_handler_fn>::const_iterator it = urlHandlers.find(reqPath);
    if(it != urlHandlers.end()) {
        AGO_TRACE() << "Matched handler on " << it->first << ", calling";
        reqRep = it->second(conn, req, reqPath);
        AGO_TRACE() << "Handler for " << it->first << " returned";
        return reqRep;
    }

    // Prefix handler?
    for(it = urlPrefixHandlers.begin(); it != urlPrefixHandlers.end(); it++) {
        if(reqPath.size() >= it->first.size() &&
                reqPath.compare(0, it->first.size(), it->first) == 0) {
            AGO_TRACE() << "Matched prefix handler on " << it->first << ", calling";
            reqRep = it->second(conn, req, reqPath);
            AGO_TRACE() << "Handler for " << it->first << " returned";
            return reqRep;
        }
    }

    AGO_TRACE() << "No URL match for " << std::string(req->uri.p, req->uri.len) << ", sending to mg_serve_http";
    if(httpServeOpts.document_root) {
        // No registered handler, use regular file serve
        mg_serve_http(conn, req, httpServeOpts);
    }else{
        // Docroot not enabled, 404
        mg_send_head(conn, 404, 0, "");
        handleKeepaliveFlagAfterRequestFinish(conn);
    }
    return reqRep; // empty
}

/**
 * Mongoose event handler
 */
void AgoHttp::mongooseEventHandler(struct mg_connection *conn, int event, void *ev_data)
{
    MgUserData *connWrapper = (MgUserData *) conn->user_data;
    {
        char remote_addr[50];
        mg_conn_addr_to_str(conn, &remote_addr[0], sizeof(remote_addr), MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT | MG_SOCK_STRINGIFY_REMOTE);
        AGO_TRACE() << mg_event_name(event) << "[" << conn->sock << "]: " << remote_addr << " (userData=" << connWrapper << " ->requestData " << connWrapper->requestData.get() << ")";
    }

    switch(event) {
        case MG_EV_RECV:
            // Mark connection active until we've written the response
            conn->flags |= AGO_MG_F_ACTIVE;
            break;
        case MG_EV_HTTP_REQUEST: {
            struct http_message *req = (http_message *) ev_data;

            struct mg_str *conn_hdr = mg_get_http_header(req, "Connection");
            int keepalive;
            if (conn_hdr != NULL) {
                keepalive = (mg_vcasecmp(conn_hdr, "keep-alive") == 0);
            } else {
                keepalive = (mg_vcmp(&req->proto, "HTTP/1.1") == 0);
            }

            if(!keepalive) {
                AGO_TRACE() << "Disabling keepalive";
                conn->flags |= AGO_MG_F_NO_KEEPALIVE;
            }

            // Autentication enabled?
            if(authFile && httpServeOpts.auth_domain) {
                rewind(authFile);
                if(!mg_http_check_digest_auth(req, httpServeOpts.auth_domain, authFile)) {
                    // Borrowed from static mg_http_send_digest_auth_request
                    mg_printf(conn,
                            "HTTP/1.1 401 Unauthorized\r\n"
                            "WWW-Authenticate: Digest qop=\"auth\", "
                            "realm=\"%s\", nonce=\"%lu\"\r\n"
                            "Content-Length: 0\r\n\r\n",
                            httpServeOpts.auth_domain, (unsigned long) mg_time());
                    handleKeepaliveFlagAfterRequestFinish(conn);
                    return;
                }
            }

            boost::shared_ptr<HttpReqRep> reqRep = handleRequest(conn, req);
            if(!reqRep)
                return;

            {
                boost::lock_guard<boost::mutex> lock(reqRep->mutex);
                if(reqRep->isResponseReady()) {
                    // Response is ready to be written in full
                    reqRep->writeResponse(conn);
                    AGO_TRACE() << "handler wrote response, dropping shortlived reqRep " << reqRep.get();
                    return;
                }
            }

            mg_set_timer(conn, mg_time() + reqRep->getTimeout()/1000.0);

            // XXXX: why do we keep it??
            // One or more requests requires background processing.
            // Replace the connection wrapper with our own
            AGO_TRACE() << "handler did not write response, keeping reqRep " << reqRep.get();
            connWrapper = connWrapper->specificFor(conn, reqRep);
            break;
        }
        case MG_EV_TIMER:
        case MG_EV_POLL: {
            HttpReqRep *reqRep = connWrapper->requestData.get();
            if (reqRep) {
                bool writeResponse;
                boost::unique_lock<boost::mutex> lock(reqRep->mutex);
                if(event == MG_EV_TIMER) {
                    AGO_TRACE() << "timer trigged on reqRep " << reqRep;
                    reqRep->onTimeout();
                    writeResponse = true;
                } else {
                    AGO_TRACE() << "polling reqRep " << reqRep;
                    writeResponse = reqRep->isResponseReady();
                }

                if(writeResponse) {
                    // Response is ready to be written
                    reqRep->writeResponse(conn);

                    // Must unlock before mutex is (potentially) destroyed
                    // on shared_ptr dtor
                    lock.unlock();

                    // This async request is now done, but connectionWrapper may be reused in
                    // subsequent request (on same connection)
                    AGO_TRACE() << "connection " << reqRep << " is done, resetting";

                    connWrapper->requestData.reset();

                    mg_set_timer(conn, 0);
                }
            }

            if(state != Running && !(conn->flags & AGO_MG_F_ACTIVE)) {
                AGO_TRACE() << "Connection idle during shutdown, closing";
                conn->flags |= MG_F_CLOSE_IMMEDIATELY;
            }
            break;
        }
        case MG_EV_SEND:
            if(conn->send_mbuf.len == 0)
            {
                // Send done
                AGO_TRACE() << "Send done";
                if(state != Running) {
                    AGO_TRACE() << "Connection idle during shutdown, closing it";
                    conn->flags |= MG_F_CLOSE_IMMEDIATELY;
                }
                else
                    conn->flags &= ~AGO_MG_F_ACTIVE;
            }
            break;
        case MG_EV_CLOSE:
            // If connection-specific user data was assigned, clear it.
            if(conn->user_data != defaultUserData) {
                delete (MgUserData*)conn->user_data;
                // Should never be used any more but..
                conn->user_data = defaultUserData;
            }
            break;
    }
}

static void mg_event_handle_wakeup_broadcast(struct mg_connection *conn, int ev, void *ev_data) {
    // No-op; just want to wake up all other sockets.
}

void AgoHttp::wakeup() {
    if(state != Running) {
        // Calling when not polling anymore will hang us
        return;
    }
    // Wake servers to let pending getEvent trigger. Handler does nothing, but breaks loop and triggers POLL
    mg_broadcast(&mongooseMgr, mg_event_handle_wakeup_broadcast, (void*)"", 0);
}

/**
 * Webserver process (threaded)
 */
void AgoHttp::mongoosePollLoop()
{
    AGO_TRACE() << "Webserver thread started";
    while(true)
    {
        mg_mgr_poll(&mongooseMgr, MONGOOSE_POLLING_INTERVAL);

        if(state == ShuttingDown || state == Stopped) {
            if (mongooseMgr.active_connections == NULL) {
                // All connections are gone, break out of loop allowing
                // app to exit.
                break;
            }
        }
    }
    AGO_TRACE() << "Webserver thread terminated";
}

static void mg_shutdown_broadcast(struct mg_connection *nc, int ev, void *ev_data) {
    if(nc->flags & MG_F_LISTENING) {
        char remote_addr[50];
        mg_conn_addr_to_str(nc, &remote_addr[0], sizeof(remote_addr), MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT | MG_SOCK_STRINGIFY_REMOTE);
        AGO_TRACE() << "MG SHUTDOWN BROADCAST[" << nc->sock << "]: " << remote_addr << " Shutting down listener socket";
        nc->flags |= MG_F_CLOSE_IMMEDIATELY;
    }
    // A regular poll will be completed right after this
}

AgoHttp::AgoHttp()
    : authFile(NULL)
    , state(Stopped)
    , defaultUserData(new MgUserData(this))
{
    memset(&httpServeOpts, 0, sizeof(httpServeOpts));  // Reset all options to defaults
}

AgoHttp::~AgoHttp() {
    delete defaultUserData;
}

void AgoHttp::setDocumentRoot(const std::string& value) {
    if(state != Stopped)
        throw std::runtime_error("Cannot set document root when running");
    if(httpServeOpts.document_root)
        free((void *) httpServeOpts.document_root);

    if(value.empty()) {
        httpServeOpts.document_root = NULL;
        return;
    }
    httpServeOpts.document_root = strdup(value.c_str());
}

void AgoHttp::setAuthDomain(const std::string& value) {
    if(state != Stopped)
        throw std::runtime_error("Cannot change settings when running");

    if(httpServeOpts.auth_domain)
        free((void *) httpServeOpts.auth_domain);

    if(value.empty()) {
        httpServeOpts.auth_domain = NULL;
        return;
    }
    httpServeOpts.auth_domain = strdup(value.c_str());
}

void AgoHttp::setAuthFile(const boost::filesystem::path& path) {
    if(state != Stopped)
        throw std::runtime_error("Cannot change settings when running");

    if(authFile)
        fclose(authFile);

    // activate auth
    authFile = fopen(path.c_str(), "r");
    if(authFile==NULL)
    {
        // unable to parse auth file
        AGO_ERROR() << "Auth support: error parsing \"" << path.string() << "\" file. Authentication deactivated";
    }
    else
    {
        AGO_INFO() << "Enabling authentication";
    }
}

void AgoHttp::addHandler(const std::string& path, agohttp_url_handler_fn handler) {
    if(urlHandlers.count(path) != 0) {
        throw std::runtime_error("Path already bound");
    }

    urlHandlers[path] = handler;
}

void AgoHttp::addPrefixHandler(const std::string& path, agohttp_url_handler_fn handler) {
    if(urlPrefixHandlers.count(path) != 0) {
        throw std::runtime_error("Path already bound");
    }

    urlPrefixHandlers[path] = handler;
}

void AgoHttp::addBinding(const std::string& address
#if MG_ENABLE_SSL
        , const boost::filesystem::path& certFile,
        const boost::filesystem::path& keyFile,
        const boost::filesystem::path& caCertFile
#endif
        ) {
    if(state != Stopped)
        throw std::runtime_error("Cannot change settings when running");

    HttpBindOptions opts;
    opts.address = address;
#if MG_ENABLE_SSL
    opts.sslCertFile = certFile.native();
    opts.sslKeyFile = keyFile.native();
    opts.sslCaCertFile = caCertFile.native();
#endif
    bindings.push_back(opts);
}

void AgoHttp::start() {
    if(state != Stopped) {
        if(state == Running)
            return;

        throw std::runtime_error("Cannot start when not stopped");
    }

    mg_mgr_init(&mongooseMgr, NULL);

    BOOST_FOREACH(const HttpBindOptions& opts, bindings) {
        struct mg_bind_opts bindopts = {};
        const char *err;
        bindopts.error_string = &err;

#if MG_ENABLE_SSL
        // If empty, they are ignored.
        if(!opts.sslCertFile.empty())
            bindopts.ssl_cert = opts.sslCertFile.c_str();
        if(!opts.sslKeyFile.empty())
            bindopts.ssl_key = opts.sslKeyFile.c_str();
        if(!opts.sslCaCertFile.empty())
            bindopts.ssl_ca_cert = opts.sslCaCertFile.c_str();
#endif

        AGO_INFO() << "Binding HTTP on " << opts.address;
        struct mg_connection *lc = mg_bind_opt(&mongooseMgr, opts.address.c_str(),
                mongooseEventHandlerWrapper,
                bindopts);

        if(!lc) {
            AGO_ERROR() << "Failed to bind to port " << opts.address << ": " << err;
            throw std::runtime_error("Bad HTTP configuration");
        }

        // All child connections will inherit user_data
        lc->user_data = defaultUserData;
        mg_set_protocol_http_websocket(lc);
    }

    // Run one thread for polling. Command execution is handled from this
    // thread, or is dispatched to handler thread as required.
    state = Running;
    mongooseThread = boost::thread(boost::bind(&AgoHttp::mongoosePollLoop, this));
}

void AgoHttp::shutdown() {
    // Close all listeners
    AGO_DEBUG() << "Closing all listener sockets";
    if(state == Running) {
        mg_broadcast(&mongooseMgr, mg_shutdown_broadcast, (void*)"", 0);
    }
    state = ShuttingDown;
}

void AgoHttp::close() {
    AGO_TRACE() << "Waiting for webserver threads";
    mongooseThread.join();

    AGO_TRACE() << "Cleaning up web server";
    mg_mgr_free(&mongooseMgr);

    if(authFile) {
        fclose(authFile);
        authFile = NULL;
    }

    // Strdup'ed
    if(httpServeOpts.document_root)
        free((void *) httpServeOpts.document_root);
    if(httpServeOpts.auth_domain)
        free((void *) httpServeOpts.auth_domain);

    state = Stopped;
}

}; // namespace agohttp
}; // namespace agocontrol

