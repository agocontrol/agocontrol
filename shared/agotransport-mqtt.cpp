
#include <mosquittopp.h>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/condition.hpp>
#include <boost/lexical_cast.hpp>


#include "agotransport-mqtt.h"
#include "agolog.h"
#include "agoutils.h"

#define TOPIC_BASE "com.agocontrol/"
#define PUBLISH_TOPIC TOPIC_BASE "legacy"

namespace agotransport = agocontrol::transport;

static AGO_LOGGER(mqtt);
static AGO_LOGGER(transport);

/**
 * Factory function for creating AgoMqttTransport via dlsym.
 */
extern "C" agocontrol::transport::AgoTransport *create_instance(const char *id, const char *broker, const char *user, const char *password) {
    return new agocontrol::transport::AgoMqttTransport(id, broker, user, password);
}

static bool mqtt_inited = false;

class MqttReply {
public:
    MqttReply(agotransport::MqttImpl &impl_);
    ~MqttReply();

    agotransport::MqttImpl &impl;
    std::string replyTopic;
    Json::Value msg;
    boost::condition condition;
};

class agotransport::MqttImpl : mosqpp::mosquittopp {
public:
    MqttImpl(const std::string &id, const std::string &broker_, const std::string &user_, const std::string &password_);
    ~MqttImpl();

    bool start();

    void on_connect(int rc);
    void on_message(const struct mosquitto_message *message);
    void on_subscribe(int mid, int qos_count, const int *granted_qos);
    void on_log(int level, const char *str);

    boost::mutex mutex;
    boost::condition connected_condition;
    bool connected;
    bool shutdownSignaled;
    int pending_subscribes;

    std::string host;
    int port;
    const std::string username;
    const std::string password;

    const std::string connection_uuid;
    const std::string topic_replies_base;
    int reply_seq;

    // Topic->MqttReply ptr
    std::map<std::string, MqttReply *> pending_replies;

    std::deque<Json::Value> queue;
    boost::condition queue_condition;

    void shutdown();

    void sendReply(const Json::Value &content, std::string replyTopic);
    void sendMessage(const std::string &topic, const Json::Value &message);

    friend AgoMqttTransport;
};

agotransport::AgoMqttTransport::AgoMqttTransport(const std::string &id, const std::string &broker, const std::string &user, const std::string &password)
    : impl(new MqttImpl(id, broker, user, password))
{
}

agotransport::AgoMqttTransport::~AgoMqttTransport()
{
    AGOL_DEBUG(transport) << "MQTT transport destroyed";
    impl->disconnect();
    // impl free'd trough unique_ptr
}

bool agotransport::AgoMqttTransport::start() {
    return impl->start();
}

void agotransport::AgoMqttTransport::shutdown() {
    AgoTransport::shutdown();
    impl->shutdown();
}

bool agotransport::AgoMqttTransport::sendMessage(Json::Value& message)
{
    impl->sendMessage(std::string(PUBLISH_TOPIC), message);
    return true;
}

agocontrol::AgoResponse agotransport::AgoMqttTransport::sendRequest(Json::Value& message, std::chrono::milliseconds timeout)
{
    // acquires a reply id from impl & tracks it. destructor will untrack it.
    MqttReply reply(*impl);
    message["reply-to"] = reply.replyTopic;

    sendMessage(message);

    // Now wait for the on_message callback to trigger a wakeup (or timeout)
    boost::unique_lock<boost::mutex> lock(impl->mutex);
    bool got_reply = reply.condition.timed_wait(lock, boost::posix_time::milliseconds(timeout.count()));
    lock.unlock();

    AgoResponse r;
    if(shutdownSignaled) {
        r.init(responseError(RESPONSE_ERR_NO_REPLY, "Shutdown signaled"));
        return r;
    }

    if(!got_reply) {
        AGOL_WARNING(transport) << "Timeout waiting for reply to " << reply.replyTopic;
        r.init(responseError(RESPONSE_ERR_NO_REPLY, "Timeout"));
    }else{
        try {
            r.init(reply.msg);
            AGOL_TRACE(transport) << "Received response: " << r.response;
        } catch(const std::invalid_argument& ex) {
            AGOL_ERROR(transport) << "Invalid message on reply topic, failed to init as reply: " << ex.what();
            AGOL_ERROR(transport) << "Faulty msg: " << reply.msg;
            r.init(responseError(RESPONSE_ERR_INTERNAL, ex.what()));
        }
    }

    return r;
}


agotransport::AgoTransportMessage agotransport::AgoMqttTransport::fetchMessage(std::chrono::milliseconds timeout)
{
    AgoTransportMessage ret;

    boost::unique_lock<boost::mutex> lock(impl->mutex);
    if(impl->queue.empty()) {
        bool got_msg = impl->queue_condition.timed_wait(lock, boost::posix_time::milliseconds(timeout.count()));

        if(shutdownSignaled || !got_msg)
            return ret;
    }

    ret.message.swap(impl->queue.front());
    impl->queue.pop_front();
    lock.unlock();

    if(ret.message.isMember("reply-to")) {
        std::string replyTopic(ret.message["reply-to"].asString());
        // Do not expose reply-to to client, transport specific.
        ret.message.removeMember("repy-to");
        ret.replyFuction = boost::bind(&MqttImpl::sendReply, boost::ref(*impl), _1, replyTopic);
    }

    return ret;
}


/** Internal Mosquitto specific impl below **/


/* The MqttReply class acquires a unique reply topic on construction,
 * and registers itself in the impl as "waiting for reply".
 * In destructor, it  will unregister itself from the impl.
 */
MqttReply::MqttReply(agotransport::MqttImpl& impl_)
        : impl(impl_) {

    boost::lock_guard<boost::mutex> lock(impl.mutex);
    replyTopic = impl.topic_replies_base + std::to_string(++impl.reply_seq);
    //AGOL_TRACE(transport) << "MQTT: now tracking reply on " << replyTopic;
    impl.pending_replies[replyTopic] = this;
}

MqttReply::~MqttReply() {
    boost::lock_guard<boost::mutex> lock(impl.mutex);
    //AGOL_TRACE(transport) << "MQTT: no longer tracking reply on " << replyTopic;
    impl.pending_replies.erase(replyTopic);

}



agotransport::MqttImpl::MqttImpl(const std::string &id, const std::string &broker_, const std::string& user_, const std::string &password_)
        : mosquittopp(id.c_str())
        , connected(false)
        , shutdownSignaled(false)
        , username(user_)
        , password(password_)
        , connection_uuid(utils::generateUuid())
        , topic_replies_base(std::string(TOPIC_BASE) + connection_uuid + "/replies/")
        , reply_seq(0) {
    if (!mqtt_inited) {
        mosqpp::lib_init();
        mqtt_inited = true;
    }

    // Split broker in host:port
    size_t colon = broker_.find_first_of(':');
    if(colon == std::string::npos) {
        host = broker_;
        port = 1883;
    } else  {
        host = broker_.substr(0, colon);
        try {
            port = boost::lexical_cast<int>(broker_.substr(colon + 1));
        }catch(boost::bad_lexical_cast&e) {
            throw std::runtime_error("Invalid broker, failed to parse port number");
        }
    }
}

agotransport::MqttImpl::~MqttImpl() {
    // XXX: Assumes we never have more than one..
    mosqpp::lib_cleanup();
}

bool agotransport::MqttImpl::start() {
    int keepalive = 60;

    if(!username.empty() && !password.empty()) {
        AGOL_INFO(transport) << "Connecting to " << host << ":" << port << " with user " << username;
        username_pw_set(username.c_str(), password.c_str());
    } else {
        AGOL_INFO(transport) << "Connecting to " << host << ":" << port << " anonymously";
    }

    /* If starting the loop before calling connect(_async), then there is some kind of race condition
     * in mosqutto which *somtimes* makes it wait one loop sleep period before actually connecting.
     * In version 1.3.x that sleep period is 60s, in newer ones it is ~1s.
     * So, instead we can connect from here, and block until done. That also mimics how
     * the qpid transport behaves, blocking in start() until connected.
     */
    while(!shutdownSignaled) {
        int rc = connect_async(host.c_str(), port, keepalive);
        if (rc == MOSQ_ERR_INVAL) {
            AGOL_ERROR(transport) << "Invalid connection parameters: " << host << ":" << port;
            return false;
        } else if(rc == MOSQ_ERR_SUCCESS) {
            break;
        } else {
            if(rc == MOSQ_ERR_ERRNO && errno == ENOTCONN) {
                // Silently ignore, seen on FreeBSD a few times during setup,
                // depending on log level (i.e. some other race condition?)
                break;
            }

            AGOL_ERROR(transport) << "Connection failed: " << mosquitto_strerror(rc);
            sleep(1);
        }
    }

    if(shutdownSignaled)
        return false;

    AGOL_DEBUG(transport) << "Launching MQTT loop thread";
    loop_start();

    // Now wait for mqtt to be connected and subscribed
    boost::unique_lock<boost::mutex> lock(mutex);

    AGOL_DEBUG(transport) << "waiting for subscriptions to be setup";
    if(!connected)
        connected_condition.timed_wait(lock, boost::posix_time::milliseconds(1000));

    if(!connected)
        return false;

    AGOL_INFO(transport) << "Connection & subscriptions ready";
    return true;
}

void agotransport::MqttImpl::shutdown() {
    AGOL_DEBUG(transport) << "Shutting down";
    shutdownSignaled = true;
    disconnect(); // breaks out of loop once network requests have finished.
    loop_stop();

    boost::lock_guard<boost::mutex> lock(mutex);
    queue_condition.notify_all();
    for(auto it = pending_replies.begin(); it !=pending_replies.end(); it++) {
        it->second->condition.notify_one();
    }

    // wake up the condition, in case were blocking in start()
    AGOL_DEBUG(transport) << "Shutdown complete";
    connected = false;
    connected_condition.notify_one();
}

void agotransport::MqttImpl::on_connect(int rc)
{
    if (!rc)
    {
        AGOL_INFO(transport) << "Connected, subscribing to "
            << PUBLISH_TOPIC << ", " << topic_replies_base << "/+";

        pending_subscribes = 2;
        subscribe(NULL, PUBLISH_TOPIC, 1);
        subscribe(NULL, (topic_replies_base + "+").c_str(), 1);
    }else {
        AGOL_WARNING(transport) << "Connect failed " << rc;

        boost::unique_lock<boost::mutex> lock(mutex);
        connected = false;
        connected_condition.notify_one();
    }
}

void agotransport::MqttImpl::on_subscribe(int mid, int qos_count, const int *granted_qos)
{
    pending_subscribes--;
    if(pending_subscribes > 0)
        return;

    AGOL_DEBUG(transport) << "Subscriptions succeeded.";

    boost::unique_lock<boost::mutex> lock(mutex);
    connected = true;
    connected_condition.notify_one();
}

void agotransport::MqttImpl::on_log(int level, const char* msg) {
    switch(level) {
        case MOSQ_LOG_INFO:
            AGOL_INFO(mqtt) << "MQTT[info]: " << msg;
            break;
        case MOSQ_LOG_NOTICE:
            AGOL_INFO(mqtt) << "MQTT[notice]: " << msg;
            break;
        case MOSQ_LOG_WARNING:
            AGOL_WARNING(mqtt) << "MQTT[warning]: " << msg;
            break;
        case MOSQ_LOG_ERR:
            AGOL_ERROR(mqtt) << "MQTT[error]: " << msg;
            break;
        case MOSQ_LOG_DEBUG:
            AGOL_DEBUG(mqtt) << "MQTT[debug]: " << msg;
            break;
        default:
            AGOL_ERROR(mqtt) << "MQTT[unknown " << level << "]: " << msg;
            break;
    }
}

void agotransport::MqttImpl::on_message(const struct mosquitto_message *message)
{
    std::string topic(message->topic);;
    if(topic.find(TOPIC_BASE) != 0) {
        AGOL_ERROR(transport) << "Ignoring message on unknown topic, should not have been received at all" << topic;
        return;
    }

    MqttReply* reply = NULL;
    if(topic.find(topic_replies_base) != std::string::npos) {
        // Sent to our reply topic. Find pending reply handler
        boost::lock_guard<boost::mutex> lock(mutex);
        auto it = pending_replies.find(topic);
        if(it == pending_replies.end()) {
            AGOL_WARNING(transport) << "Received too late response for " << topic;
            return;
        }

        // Just hint that we have a valid reply
        reply = it->second;
    }else if(topic != PUBLISH_TOPIC) {
        AGOL_WARNING(transport) << "Ignoring message on unknown topic " << topic;
        return;
    }

    Json::Value msg(Json::objectValue);
    Json::CharReaderBuilder builder;
    std::string errors;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    char *payload = (char*) message->payload;
    if ( !reader->parse(payload, payload + message->payloadlen,
                &msg, &errors) ) {
        AGOL_WARNING(transport) << "Failed to parse JSON: " << errors;
        return;
    }

    AGOL_TRACE(transport) << "Received message on " << topic << ": " << msg;

    if(reply) {
        boost::lock_guard<boost::mutex> lock(mutex);
        // re-check
        if(pending_replies.find(topic) == pending_replies.end()) {
            AGOL_INFO(transport) << "Reply handler disappeared while parsing, dropping";
            return;
        }

        //AGOL_TRACE(transport) << "delivering to reply handler";
        reply->msg.swap(msg);
        reply->condition.notify_one();
        return;
    }

    // Regular message, deliver to main queue and wake up any fetchMessage call
    boost::lock_guard<boost::mutex> lock(mutex);
    queue.push_back(msg);
    queue_condition.notify_one();
}




void agotransport::MqttImpl::sendMessage(const std::string& topic, const Json::Value& message)
{
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";

    std::string payload = Json::writeString(builder, message);

    AGOL_TRACE(transport) << "Sending message to " << topic << ": " << payload;
    publish(NULL, topic.c_str(), strlen(payload.c_str()), payload.c_str(), 1, false);
}

void agotransport::MqttImpl::sendReply(const Json::Value& content, std::string replyTopic) {
    AGOL_TRACE(transport) << "Sending reply to " << replyTopic << ": " << content;
    sendMessage(replyTopic, content);
}

