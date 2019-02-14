#include "agoapp.h"

using namespace agocontrol;
namespace po = boost::program_options;

class AgoExample: public AgoApp {
private:
    void setupApp();
    Json::Value commandHandler(const Json::Value& content);
    void appCmdLineOptions(boost::program_options::options_description &options);
public:
    AGOAPP_CONSTRUCTOR(AgoExample);
};

Json::Value AgoExample::commandHandler(const Json::Value& content) {
    checkMsgParameter(content, "command", Json::stringValue);
    checkMsgParameter(content, "internalid", Json::stringValue);

    std::string command = content["command"].asString();
    std::string internalid = content["internalid"].asString();

    if (command == "on") {
        AGO_DEBUG() << "Switch " << internalid << " ON";
        agoConnection->emitEvent(internalid, "event.device.statechanged", "255", "");
        return responseSuccess();

    } else if (command == "off") {
        AGO_DEBUG() << "Switch " << internalid << " OFF";
        agoConnection->emitEvent(internalid, "event.device.statechanged", "0", "");
        return responseSuccess();
    }

    return responseUnknownCommand();
}

void AgoExample::appCmdLineOptions(boost::program_options::options_description &options) {
    options.add_options()
        ("test,T", po::bool_switch(),
         "A custom option which can be set or not set")
        ("test-string", po::value<std::string>(),
         "This can be a string")
        ;
}

void AgoExample::setupApp() {
    AGO_INFO() << "example device starting up";

    // do some device specific setup
    std::string test_param;

    if (cli_vars.count("test-string"))
        test_param = cli_vars["test-string"].as<std::string>();

    AGO_INFO() << "Param Test string is " << test_param;

    /* Read a configuration value "test_option" from our example.conf file
     * This will look in the configuration file conf.d/<app name>.conf
     * under the [<app name>] section.
     * In this example, this means example.conf and [example] section
     */
    std::string cfgvalue = getConfigOption("test_option", "not set");
    AGO_INFO() << "Config test_option is '" << cfgvalue << "'";

    if(cfgvalue == "invalid") {
        throw ConfigurationError("test-string must be set to 'valid'");
    }

    if(false) {
        // Other startup failure
        AGO_FATAL() << "setup failed";
        throw StartupError();
    }

    // add some devices
    agoConnection->addDevice("123", "dimmer");
    agoConnection->addDevice("124", "switch");

    // add our command handler
    addCommandHandler();
}

/* Finally create an application main entry point, this will create
 * the int main(...) {} part */
AGOAPP_ENTRY_POINT(AgoExample);

