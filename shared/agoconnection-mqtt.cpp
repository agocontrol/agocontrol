
#include <mosquittopp.h>
#include <boost/thread/mutex.hpp>
#include <boost/thread/locks.hpp>

#include "agoconnection-mqtt.h"
#include "agolog.h"

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
    boost::mutex mutexCon;
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
        subscribe(0, PUBLISH_TOPIC, 1);
    }
}

void agocontrol::MosquittoAdapter::on_subscribe(int mid, int qos_count, const int *granted_qos)
{
    std::cout << "Subscription succeeded." << std::endl;
}


void agocontrol::MosquittoAdapter::on_message(const struct mosquitto_message *message)
{
    int payload_size = MAX_PAYLOAD + 1;
    char buf[payload_size];

    //if(!strcmp(message->topic, PUBLISH_TOPIC))
    {
        boost::lock_guard<boost::mutex> lock(mutexCon);

        memset(buf, 0, payload_size * sizeof(char));

        /* Copy N-1 bytes to ensure always 0 terminated. */
        memcpy(buf, message->payload, MAX_PAYLOAD * sizeof(char));

        AGO_INFO() << "received mqtt buf: " << buf << std::endl;

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
    return r;
}

agocontrol::AgoConnectionMessage agocontrol::AgoMQTTImpl::fetchMessage(std::chrono::milliseconds timeout)
{
    agocontrol::AgoConnectionMessage ret;
    return ret;
}
