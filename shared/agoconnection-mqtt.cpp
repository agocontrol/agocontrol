
#include <mosquittopp.h>
#include <boost/thread/mutex.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/thread.hpp> 

#include "agoconnection-mqtt.h"
#include "agolog.h"

#include "agoclient.h"

#define PUBLISH_TOPIC "com.agocontrol/legacy"
#define MAX_PAYLOAD 65535

namespace agocontrol {
static bool mqtt_inited = false;

class MosquittoAdapter : mosqpp::mosquittopp {
public:
    MosquittoAdapter(const std::string &id, const std::string &host_, int port_)
            : mosquittopp(id.c_str()), host(host_), port(port_) {
        if (!mqtt_inited) {
            mosqpp::lib_init();
            mqtt_inited = true;
        }
    }

    ~MosquittoAdapter() {
        // XXX: Assumes we never have more than one..
        mosqpp::lib_cleanup();
    }

    void on_connect(int rc);

    void on_message(const struct mosquitto_message *message);

    void on_subscribe(int mid, int qos_count, const int *granted_qos);

    void on_log(int level, const char *str);

private:
    std::deque<AgoConnectionMessage *> messageDeque;
    boost::mutex mutexCon;
    boost::condition_variable_any cond;
    boost::mutex mutexReply;
    boost::condition_variable_any replyCond;
    std::string replyExpectedUuid;
    AgoConnectionMessage *replyMessage;
    const std::string host;
    int port;

    friend AgoMQTTImpl;
    };
}

agocontrol::AgoMQTTImpl::AgoMQTTImpl(const std::string& id, const std::string& host, int port)
    : adapter(new MosquittoAdapter(id, host, port))
{
}

agocontrol::AgoMQTTImpl::~AgoMQTTImpl()
{
    adapter->disconnect();
    // adapter free'd trough unique_ptr
}

bool agocontrol::AgoMQTTImpl::start() {

    int keepalive = 60;
    // Synchronous connect
    int rc = adapter->connect_async(adapter->host.c_str(), adapter->port, keepalive);
    if(rc == MOSQ_ERR_INVAL) {
        AGO_ERROR() << "Invalid MQTT connection parameters: " << adapter->host << ":" << adapter->port;
        return false;
    }
    else if(rc == MOSQ_ERR_ERRNO) {
        char msg[1024];
        strerror_r(errno, msg, 1024);
        AGO_ERROR() << "MQTT connection failed: " << msg;
        // TODO: Does caller expect us to retry?
        return false;
    }

    // TODO: username etc?
    //
    // username_pw_set(user, pass);
    
    AGO_INFO() << "Connected to MQTT broker " << adapter->host << ":" << adapter->port;

    AGO_INFO() << "Starting MQTT loop";
    adapter->loop_start();    

    return true;
}

void agocontrol::MosquittoAdapter::on_connect(int rc)
{
    if (!rc)
    {
        AGO_INFO() << "Connected to MQTT broker, subscribing to topic " << PUBLISH_TOPIC;
        subscribe(0, PUBLISH_TOPIC, 1); // mid, topic, qos
    }
}

void agocontrol::MosquittoAdapter::on_subscribe(int mid, int qos_count, const int *granted_qos)
{
    AGO_INFO() << "Subscription succeeded." << std::endl;
}


void agocontrol::MosquittoAdapter::on_message(const struct mosquitto_message *message)
{
    //std::string payload((const char*)message->payload, message->payloadlen);
    //AGO_INFO() << "received mqtt buf: " << payload << std::endl;

    Json::CharReaderBuilder builder;
    Json::CharReader * reader = builder.newCharReader();

    Json::Value root;
    std::string errors;

    bool parsingSuccessful = reader->parse((const char*)message->payload,(const char*)message->payload+message->payloadlen, &root, &errors);
    //bool parsingSuccessful = reader->parse(payload.c_str(),payload.c_str()+payload.length(), &root, &errors);
    delete reader;
    if (parsingSuccessful) {
        AGO_DEBUG() << "parsed JSON from MQTT message";
        if (root.isMember("type") && root["type"] == "reply") { // reply for request
            AGO_DEBUG() << "message is a reply";
            boost::lock_guard<boost::mutex> lock(mutexReply);
            if (root["reply-id"] == replyExpectedUuid) {
                AGO_DEBUG() << "reply message is for us: " << root["reply-id"] << " matches " << replyExpectedUuid;
                AgoConnectionMessage *newMessage = new AgoConnectionMessage();
                newMessage->msg = root;
                {
                    replyMessage = newMessage;
                    replyCond.notify_one();
                }
            } else {
                AGO_DEBUG() << "reply message is not for us: " << root["reply-id"] << " vs " << replyExpectedUuid;
            }


        } else { // regular message or event
            AgoConnectionMessage *newMessage = new AgoConnectionMessage();
            newMessage->msg = root;
            {
                boost::lock_guard<boost::mutex> lock(mutexCon);
                messageDeque.push_back(newMessage);
                cond.notify_one();
            }
        }
    } else {
        AGO_ERROR() << "cannot parse MQTT message to JSON: " << errors;
    }

}

void agocontrol::MosquittoAdapter::on_log(int level, const char* msg) {
    switch(level) {
        case MOSQ_LOG_INFO:
            AGO_INFO() << "MQTT[info]: " << msg;
            break;
        case MOSQ_LOG_NOTICE:
            AGO_INFO() << "MQTT[notice]: " << msg;
            break;
        case MOSQ_LOG_WARNING:
            AGO_WARNING() << "MQTT[warning]: " << msg;
            break;
        case MOSQ_LOG_ERR:
            AGO_ERROR() << "MQTT[error]: " << msg;
            break;
        case MOSQ_LOG_DEBUG:
            AGO_DEBUG() << "MQTT[debug]: " << msg;
            break;
    }
}

bool agocontrol::AgoMQTTImpl::sendMessage(const std::string& topic, const Json::Value& content)
{
    Json::StreamWriterBuilder builder;

    builder["indentation"] = "";
    std::string payload = Json::writeString(builder, content);
    adapter->publish(NULL, PUBLISH_TOPIC, strlen(payload.c_str()), payload.c_str(), 1, false);
    return true;
}

agocontrol::AgoResponse agocontrol::AgoMQTTImpl::sendRequest(const std::string& topic, const Json::Value& content, std::chrono::milliseconds timeout)
{
    AgoResponse r;

    boost::system_time const targettime=boost::get_system_time() + boost::posix_time::milliseconds(timeout.count());

    Json::StreamWriterBuilder builder;
    Json::Value message;
    message = content;
    std::string replyUuid = generateUuid();
    message["reply-id"] = replyUuid;
    message["type"] = "request";

    builder["indentation"] = "";
    std::string payload = Json::writeString(builder, message);
    adapter->publish(NULL, PUBLISH_TOPIC, strlen(payload.c_str()), payload.c_str(), 1, false);

    boost::lock_guard<boost::mutex> lock(adapter->mutexReply);
    adapter->replyMessage = NULL;
    adapter->replyExpectedUuid = replyUuid;

    while (targettime > boost::get_system_time())
    {
        AGO_TRACE() << "[sendRequest] expected reply UUID: " << adapter->replyExpectedUuid;
        if (adapter->replyMessage != NULL) {
            Json::Value response;
            response = adapter->replyMessage->msg;

            r.init(response);
            AGO_TRACE() << "[sendRequest] reply received, emptying replyExpectedUuid";
            adapter->replyExpectedUuid = "";
            AGO_INFO() << "Remote response received: " << r.response;
            return r;
        } else {
            //boost::this_thread::sleep_for(boost::chrono::milliseconds(5));
            adapter->replyCond.timed_wait(adapter->mutexReply, targettime);
        }
    }

    AGO_WARNING() << "No reply for request message";
    r.init(responseError(RESPONSE_ERR_NO_REPLY, "Timeout"));

    return r;
}

agocontrol::AgoConnectionMessage agocontrol::AgoMQTTImpl::fetchMessage(std::chrono::milliseconds timeout)
{
    agocontrol::AgoConnectionMessage ret;

    boost::system_time const targettime=boost::get_system_time() + boost::posix_time::milliseconds(timeout.count());

    while (targettime > boost::get_system_time())
    {
        boost::lock_guard<boost::mutex> lock(adapter->mutexCon);
        if (adapter->messageDeque.size() > 0) {
            AGO_TRACE() << "Popping MQTT Message";
            ret = *adapter->messageDeque.front();
            adapter->messageDeque.pop_front();
            std::string replyaddress= ret.msg.isMember("reply-id") ? ret.msg["reply-id"].asString() : "";
            ret.replyFuction = boost::bind(&AgoMQTTImpl::sendReply, this, _1, replyaddress);
            return ret;
        } else {
            //boost::this_thread::sleep_for(boost::chrono::milliseconds(5));
            adapter->cond.timed_wait(adapter->mutexCon, targettime);
        }

    }
    AGO_TRACE() << "Timed out in fetchMessage";
    return ret;
}

void agocontrol::AgoMQTTImpl::sendReply(const Json::Value& content, std::string replyAddress) 
{
    Json::StreamWriterBuilder builder;
    Json::Value message;
    message = content;
    message["reply-id"] = replyAddress;
    message["type"] = "reply";

    builder["indentation"] = "";
    std::string payload = Json::writeString(builder, message);
    AGO_TRACE() << "[mqtt] sending reply for " << replyAddress;


    adapter->publish(NULL, PUBLISH_TOPIC, strlen(payload.c_str()), payload.c_str(), 1, false);

}

