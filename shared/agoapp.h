#ifndef AGOAPP_H
#define AGOAPP_H

#include <memory>
#include <boost/asio/io_service.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/thread.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>

#include "agoclient.h"
#include "agojson.h"  // Not directly used, but used by all apps.

/**
 * Provides boilerplate code for writing an AGO application.
 *
 * Each application needs to implement a class
 * which extends the AgoApp class. A helpful macro for the constructor
 * can be used:
 *
 * 	class MyApp: public AgoApp {
 * 		AGOAPP_CONSTRUCTOR(MyApp);
 *
 * 		// App main code goes here
 * 		int appMain();
 * 	}
 *
 * It must then add a simple main entry point:
 * 	APP_ENTRY_POINT(MyAgoApp);
 *
 */


/* This helps with a class constructor */
#define AGOAPP_CONSTRUCTOR_HEAD(class_name) class_name() : AgoApp(#class_name)
#define AGOAPP_CONSTRUCTOR(class_name) AGOAPP_CONSTRUCTOR_HEAD(class_name) {};

/* This defines the applications main() function */
#define AGOAPP_ENTRY_POINT(app_class_name) \
    int main(int argc, const char**argv) {		\
        app_class_name instance;					\
        return instance.main(argc, argv);		\
    }

namespace agocontrol {

    class TestAgoConfig;
    class AgoApp {
    friend class TestAgoConfig;

    private :
        const std::string appName;
        std::string appShortName;

        boost::mutex appMutex;
        int exitSignaled;

        // IO thread is for proper async work which does not block
        // This is always used, at a minimum it handles IO signals.
        boost::thread ioThread;
        boost::asio::io_service ioService_;
        std::unique_ptr<boost::asio::io_service::work> ioWork;

        // Threadpool IOService allows long-running tasks.
        // This is only launched if needed
        boost::asio::io_service threadpoolIoService_;
        std::unique_ptr<boost::asio::io_service::work> threadpoolIoWork;
        boost::thread_group threadpool;

        boost::asio::signal_set signals;

        int parseCommandLine(int argc, const char **argv);

        void signal_add_with_restart(int signal);
        void signal_handler(const boost::system::error_code& error, int signal_number);
        void iothread_run();

    protected:
        /* This can be overriden from the applications constructor.
         * Preferably, don't!*/
        std::string appConfigSection;

        /* Application log level */
        log::severity_level logLevel;

        /**
         * Any parsed command line parameters are placed in this map-like object
         */
        boost::program_options::variables_map cli_vars;

        void signalExit();

        std::unique_ptr<AgoConnection> agoConnection;

        /* Obtain a reference to the ioService for async operations.
         *
         * This will be executed by AgoApp's IO thread running for the duration of the program.
         * Make sure to not block for any longer durations in this!
         * Use threadPool for longer-running jobs.
         */
        boost::asio::io_service& ioService() { return ioService_; }

        /* Obtain a reference to the ioService for threadpool operations.
         *
         * This will be executed by AgoApp's IO thread running for the duration of the program.
         */
        boost::asio::io_service& threadPool();

        /* Application should implement this function if it like to present
         * any extra command line options to the user.
         */
        virtual void appCmdLineOptions(boost::program_options::options_description &options) {}

        /* Setup will be called prior to appMain being called. This
         * normally sets up logging, an AgoConnection, signal handling,
         * and finally any app specific setup (setupApp).
         *
         * Please keep setupXX() functions in the order called!
         */
        bool setup();
        void setupLogging();
        virtual bool setupAgoConnection(boost::unique_lock<boost::mutex> &lock);
        void setupSignals();
        /* App specific init can be done in this */
        virtual void setupApp() { };
        void setupIoThread();
        void setupThreadPool();

        /* After appMain has returned, we will call cleanup to release
         * any resources */
        void cleanup();
        void cleanupAgoConnection();
        virtual void cleanupApp() { };
        void cleanupIoThread();

        void cleanupThreadPool();

        bool isExitSignaled() { return exitSignaled > 0; }

        /* Shortcut to register the commandHandler with agoConnection.
         * Should be called from setupApp */
        void addCommandHandler();

        /* Shortcut to register the eventHandler with agoConnection.
         * Should be called from setupApp */
        void addEventHandler();

        /* Command handler registered with the agoConncetion; override! */
        virtual Json::Value commandHandler(const Json::Value& content) {
            return Json::Value();
        }

        /* Event handler registered with the agoConncetion; override! */
        virtual void eventHandler(const std::string& subject , const Json::Value& content) {}

        /**
         * This is called by the application on shutdown, triggered via SIGINT and SIGQUIT.
         * At this point, no further incoming messages will be processed.
         */
        virtual void doShutdown() {};

        /**
         * Read a config option from the configuration subsystem.
         *
         * The system is based on per-app configuration files, which has sections
         * and options.
         *
         * The simplest, and most common, usage only specifies the option and
         * defaultValue. This looks in the applications own configuration file, in a
         * section named after the app.
         * Some apps might want to use a custom section too.
         * Edge-case apps might want to look in other files, by specifying value for app.
         *
         * It is posible to look at multiple sections, for example for fallback if it's
         * not present in the main section. To do this, pass an explicit
         * ExtraConfigNameList object as section/app. This will look first at the apps
         * primary section/app, then at any sections/apps specified in the ExtraConfigNameList.
         * To completely override the probed sections/apps, use a plain ConfigNameList.
         *
         * Note that a regular char* or std::string can be passed instead of
         * ConfigNameList objects, it will create an implicit ConfigNameList.
         *
         * Arguments:
         *  option -- The name of the option to retreive
         *
         *  defaultValue -- If the option can not be found in any of the specified
         *      sections, fall back to this value.
         *
         *  section -- A ConfigNameList section to look for the option in.
         *      Defaults to the applications shorted name.
         *
         *  app -- A ConfigNameList identifying the configuration storage unit to look in.
         *      If omited, it defaults to the section(s).
         *
         * Lookup order:
         *  For each specified app, we look at each specified section/option. First non-empty
         *  value wins.
         *
         * Returns:
         *  The value found.
         *  If not found, defaultValue is passed through unmodified.
         */
        std::string getConfigOption(const std::string& option, const char* defaultValue, const ConfigNameList &section=BLANK_CONFIG_NAME_LIST, const ConfigNameList &app = BLANK_CONFIG_NAME_LIST) {
            ConfigNameList section_(section, appConfigSection);
            ConfigNameList app_(app, section_);
            return getConfigSectionOption(section_, option, std::string(defaultValue != nullptr ? defaultValue : ""), app_);
        }
        std::string getConfigOption(const std::string& option, const std::string& defaultValue, const ConfigNameList &section=BLANK_CONFIG_NAME_LIST, const ConfigNameList &app = BLANK_CONFIG_NAME_LIST) {
            ConfigNameList section_(section, appConfigSection);
            ConfigNameList app_(app, section_);
            return getConfigSectionOption(section_, option, defaultValue, app_);
        }
        boost::filesystem::path getConfigOption(const std::string& option, const boost::filesystem::path &defaultValue, const ConfigNameList &section=BLANK_CONFIG_NAME_LIST, const ConfigNameList &app = BLANK_CONFIG_NAME_LIST) {
            ConfigNameList section_(section, appConfigSection);
            ConfigNameList app_(app, section_);
            return getConfigSectionOption(section_, option, defaultValue, app_);
        }

        /**
         * Write a config option to the configuration subsystem.
         *
         * The system is based on per-app configuration files, which has sections
         * and options.
         *
         * This is a low-level implementation, please try to use AgoApp's instance version
         * of setConfigOption instead.
         *
         * Arguments:
         *  section -- A string section in which to store the option in.
         *
         *  option -- The name of the option to set
         *
         *  value -- The value to write
         *
         *  app -- A string identifying the configuration storage unit to store to.
         *      If omited, it defaults to the section.
         *
         * Returns:
         *  true if sucesfully stored, false otherwise.
         *  Please refer to the error log for failure indication.
         */
        bool setConfigOption(const std::string& option, const std::string& value, const std::string& section={}, const std::string& app={}) {
            return setConfigSectionOption(section.empty() ? appConfigSection : section, option, value, app.empty() ? section : app);
        }
        bool setConfigOption(const std::string& option, const char* value, const std::string& section={}, const std::string& app={}) {
            // Only reason for having a const char* over std::string is to avoid implicit conversion of value to bool
            if(value == nullptr)
                throw std::runtime_error("null value sent to setConfigOption("+option+", nullptr, "+section+", "+app+")");
            return setConfigSectionOption(section.empty() ? appConfigSection : section, option, value, app.empty() ? section : app);
        }
        bool setConfigOption(const std::string& option, float value, const std::string& section={}, const std::string& app={}) {
            return setConfigSectionOption(section.empty() ? appConfigSection : section, option, value, app.empty() ? section : app);
        }
        bool setConfigOption(const std::string& option, int value, const std::string& section={}, const std::string& app={}) {
            return setConfigSectionOption(section.empty() ? appConfigSection : section, option, value, app.empty() ? section : app);
        }
        bool setConfigOption(const std::string& option, bool value, const std::string& section={}, const std::string& app={}) {
            return setConfigSectionOption(section.empty() ? appConfigSection : section, option, value, app.empty() ? section : app);
        }
    public:
        AgoApp(const char *appName);
        virtual ~AgoApp() {} ;

        /**
         * Main method, call this from application main entry point (generally this
         * is done using the AGOAPP_ENTRY_POINT macro).
         */
        int main(int argc, const char **argv);

        /**
         * By default this calls agoConnection->run(), and blocks until shutdown signal
         * arrives.
         * This can be overridden if non-standard behavior is required.
         */
        virtual int appMain();
    };

    class ConfigurationError: public std::runtime_error {
    public:
        ConfigurationError(const std::string &what)
            : runtime_error(what) {}
        ConfigurationError(const char *what)
            : runtime_error(std::string(what)) {}
    };

    /* This can be raised from any setup method, to indicate setup failure.
     * Application should log actual error itself!
     */
    class StartupError: public std::runtime_error {
    public:
        StartupError(): runtime_error("") {};
    };


}/* namespace agocontrol */

#endif
