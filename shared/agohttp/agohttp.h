#ifndef AGOHTTP_H
#define AGOHTTP_H

#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/thread/mutex.hpp>
#include <json/value.h>

#include "mongoose.h"

namespace agocontrol {
namespace agohttp {

void variantMapToJson(const qpid::types::Variant::Map &map, Json::Value &root) ;
void variantListToJson(const qpid::types::Variant::List &list, Json::Value &root) ;


class HttpReqRep {
private:
    int responseCode = 0;

public:
    /* This lock is acquired before AgoHttp makes any calls to a HttpReqRep.
     * Any other thread touching this struct must ensure the lock is held at all times where
     * it might be accessed. However, it must not grab it and BLOCK on it,
     * since then we have no way to call onTimeout().
     */
    boost::mutex mutex;

    HttpReqRep();
    virtual ~HttpReqRep() ;

    void writeResponse(struct mg_connection *conn);
    virtual void setResponseCode(int responseCode_) { responseCode = responseCode_; }
    virtual int getResponseCode() { return responseCode; }

    // Implementation specific
    virtual void writeResponseData(struct mg_connection *conn) = 0;

    virtual bool isResponseReady() = 0;
    virtual void onTimeout() = 0;
    virtual uint16_t getTimeout() { return 30000; }

};

class HttpReqJsonRep : public HttpReqRep {
public:
    // These must not be touched without holding mutex!
    Json::Value jsonResponse;
    bool responseReady;

    HttpReqJsonRep()
        : responseReady(false)
    {
    }

    void writeResponseData(struct mg_connection *conn);
    virtual bool isResponseReady() { return responseReady; }
    virtual void onTimeout();
};

typedef boost::function<
        boost::shared_ptr<HttpReqRep> (struct mg_connection *conn, struct http_message *req, const std::string& path)
    > agohttp_url_handler_fn;

class HttpBindOptions {
friend class AgoHttp;
    std::string address;
    std::string sslCertFile;
    std::string sslKeyFile;
    std::string sslCaCertFile;
};

class MgUserData;
class AgoHttp {
friend class MgUserData;
public:
    AgoHttp();
    ~AgoHttp();

    // The following configuration calls may only be made before call to start()!

    // Add port/address to bind to
    // Valid syntax for address, see:
    // https://docs.cesanta.com/mongoose/master/#/c-api/net.h/mg_bind_opt.md/
    // If SSL is desired on this particular port, specify certFile, and optionally keyFile & caCertFile.
    // SSL files should be in PEM format, certFile may hold both key & cert.
    void addBinding(const std::string& address,
            const boost::filesystem::path& certFile = boost::filesystem::path(),
            const boost::filesystem::path& keyFile = boost::filesystem::path(),
            const boost::filesystem::path& caCertFile = boost::filesystem::path());

    // Set a document root to serve documents from, if no handler have been registered for path.
    // If never called, we respond 404 to any unknown paths.
    void setDocumentRoot(const std::string& value) ;

    // Authenticatio realm to use?
    void setAuthDomain(const std::string& value) ;
    // Location of a htpasswd file to use for authentication
    void setAuthFile(const boost::filesystem::path& path) ;

    // Register a handler on this path. Note that paths are exactly matched.
    void addHandler(const std::string& path, agohttp_url_handler_fn handler);

    // Register a handler starting with this path.
    void addPrefixHandler(const std::string& path, agohttp_url_handler_fn handler);

    // Launch the server. May throw exception if bind fails etc
    void start();

    // Trigger a wakeup of the poll loo, valid from any thread. Must NOT hold any ReqRep mutex
    // when calling this, or deadlock may occur!
    void wakeup();

    void shutdown();
    void close();

protected:
    std::list<HttpBindOptions> bindings;

    struct mg_serve_http_opts httpServeOpts;
    FILE* authFile;

    enum RunState { Stopped, Running, ShuttingDown } state;

    boost::thread mongooseThread;
    struct mg_mgr mongooseMgr;

    MgUserData* defaultUserData;

    void mongoosePollLoop();

    boost::shared_ptr<HttpReqRep> handleRequest(mg_connection *conn, struct http_message *req) ;
    void mongooseEventHandler(struct mg_connection *conn, int ev, void *ev_data);

    std::map<std::string, agohttp_url_handler_fn> urlHandlers;
    std::map<std::string, agohttp_url_handler_fn> urlPrefixHandlers;
};

}; // namespace agohttp
}; // namespace agocontrol

#endif
