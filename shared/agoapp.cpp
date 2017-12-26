#include <signal.h>
#include <sys/wait.h>

#include <stdexcept>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/asio/placeholders.hpp>
#include <boost/program_options/parsers.hpp>



#include "agoapp.h"

#include "agoclient.h"
#include "options_validator.hpp"

using namespace qpid::types;
using namespace qpid::messaging;

using namespace agocontrol::log;
using namespace boost_options_validator;
namespace po = boost::program_options;
namespace fs = boost::filesystem;

namespace agocontrol {

AgoApp::AgoApp(const char *appName_)
    : appName(appName_)
      , exit_signaled(false)
      , signals(ioService_)
      , agoConnection(NULL)
{
    // Keep a "short" name too, trim leading 'Ago' from app name
    // This will be used for getConfigOption section names
    if(appName.find("Ago") == 0)
        appShortName = appName.substr(3);
    else
        appShortName = appName;

    boost::to_lower(appShortName);

    // Default to same config section as shortname, but this may be custom
    // if the original app did not use something "matching"
    appConfigSection = appShortName;
}

int AgoApp::parseCommandLine(int argc, const char **argv) {
    po::options_description options;
    options.add_options()
        ("help,h", "produce this help message")
        ("log-level", po::value<std::string>(),
         (std::string("Log level. Valid values are one of\n ") +
          boost::algorithm::join(log_container::getLevels(), ", ")).c_str())
        ("log-method", po::value<std::string>(),
         "Where to log. Valid values are one of\n console, syslog")
        ("log-syslog-facility", po::value<std::string>(),
         (std::string("Which syslog facility to log to. Valid values are on of\n ") +
          boost::algorithm::join(log_container::getSyslogFacilities(), ", ")).c_str())
        ("debug,d", po::bool_switch(),
         "Shortcut to set console logging with level DEBUG")
        ("trace,t", po::bool_switch(),
         "Shortcut to set console logging with level TRACE")
        ("config-dir", po::value<std::string>(),
         "Directory with configuration files")
        ("state-dir", po::value<std::string>(),
         "Directory with local state files")
        ;

    appCmdLineOptions(options);
    try {
        po::variables_map &vm (cli_vars);
        po::store(po::parse_command_line(argc, argv, options), vm);
        po::notify(vm);

        if(vm.count("config-dir")) {
            std::string dir = vm["config-dir"].as<std::string>();
            try {
                AgoClientInternal::setConfigDir(dir);
            } catch(const fs::filesystem_error& error) {
                std::cout << "Could not use " << dir << " as config-dir: "
                    << error.code().message()
                    << std::endl;
                return 1;
            }
        }

        if(vm.count("state-dir")) {
            std::string dir = vm["state-dir"].as<std::string>();
            try {
                AgoClientInternal::setLocalStateDir(dir);
            } catch(const fs::filesystem_error& error) {
                std::cout << "Could not use " << dir << " as state-dir: "
                    << error.code().message()
                    << std::endl;
                return 1;
            }
        }

        // Init dirs before anything else, so we at least show correct in help output
        if (vm.count("help")) {
            initDirectorys();
            std::cout << "usage: " << argv[0] << std::endl
                << options << std::endl
                << std::endl
                << "Paths:" << std::endl
                << "  Default config dir: "
                <<  BOOST_PP_STRINGIZE(DEFAULT_CONFDIR) << std::endl
                << "  Default state dir : "
                <<  BOOST_PP_STRINGIZE(DEFAULT_LOCALSTATEDIR) << std::endl
                << "  Active config dir : "
                << getConfigPath().string() << std::endl
                << "  Active state dir  : "
                << getLocalStatePath().string() << std::endl
                << std::endl
                << "System configuration file      : "
                << getConfigPath("conf.d/system.conf").string() << std::endl
                << "App-specific configuration file: "
                << getConfigPath("conf.d/" + appShortName + ".conf").string() << std::endl
                ;
            return 1;
        }

        options_validator<std::string>(vm, "log-method")
            ("console")("syslog")
            .validate();

        options_validator<std::string>(vm, "log-syslog-facility")
            (log_container::getSyslogFacilities())
            .validate();

        options_validator<std::string>(vm, "log-level")
            (log_container::getLevels())
            .validate();
    }
    catch(std::exception& e)
    {
        std::cout << "Failed to parse command line: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}


void AgoApp::setup() {
    setupSignals();
    setupLogging();
    setupIoThread();
    setupAgoConnection();
    setupApp();

    //Send event app is started
    //This is useful to monitor app (most of the time systemd restarts app before agosystem find it has crashed)
    //And it fix enhancement #143
    qpid::types::Variant::Map content;
    content["process"] = appShortName;
    //no internalid specified, processname is in event content
    agoConnection->emitEvent("", "event.monitoring.processstarted", content);
}

void AgoApp::cleanup() {
    cleanupApp();
    cleanupIoThread();
    cleanupAgoConnection();
}

void AgoApp::setupLogging() {
    log_container::initDefault();

    std::string level_str;
    if(cli_vars["debug"].as<bool>())
        level_str = "DEBUG";
    else if(cli_vars["trace"].as<bool>())
        level_str = "TRACE";
    else if(cli_vars.count("log-level"))
        level_str = cli_vars["log-level"].as<std::string>();
    else
        level_str = getConfigOption("log_level", "info", ExtraConfigNameList("system"));

    try {
        logLevel = log_container::getLevel(level_str);
        log_container::setCurrentLevel(logLevel);
    }catch(std::runtime_error &e) {
        throw ConfigurationError(e.what());
    }

    std::string method;
    if(cli_vars["debug"].as<bool>() || cli_vars["trace"].as<bool>())
        method = "console";
    else if(cli_vars.count("log-method"))
        method = cli_vars["log-method"].as<std::string>();
    else
        method = getConfigOption("log_method", "console", ExtraConfigNameList("system"));

    if(method == "syslog") {
        std::string facility_str;
        if(cli_vars.count("log-syslog-facility"))
            facility_str = cli_vars["log-syslog-facility"].as<std::string>();
        else
            facility_str = getConfigOption("syslog_facility", "local0", ExtraConfigNameList("system"));

        int facility = log_container::getFacility(facility_str);
        if(facility == -1) {
            throw ConfigurationError("Bad syslog facility '" + facility_str + "'");
        }

        log_container::setOutputSyslog(boost::to_lower_copy(appName), facility);
        AGO_DEBUG() << "Using syslog facility " << facility_str << ", level " << level_str;
    }
    else if(method == "console") {
        AGO_DEBUG() << "Using console log, level " << level_str;
    }
    else {
        throw ConfigurationError("Bad log_method '" + method + "'");
    }

    method = getConfigOption("log_method", "console", ExtraConfigNameList("system"));
}


void AgoApp::setupAgoConnection() {
    agoConnection = new AgoConnection(appShortName.c_str());
    // This allows signal handler to reach a pending connection
    agoConnection->start();
}

void AgoApp::cleanupAgoConnection() {
    if(agoConnection == NULL)
        return;

    delete agoConnection;
    agoConnection = NULL;
}

void AgoApp::addCommandHandler() {
    agoConnection->addHandler(boost::bind(&AgoApp::commandHandler, this, _1));
}

void AgoApp::addEventHandler() {
    agoConnection->addEventHandler(boost::bind(&AgoApp::eventHandler, this, _1, _2));
}



void AgoApp::signal_add_with_restart(int signal) {
    AGO_TRACE() << "Handling signal " << signal;
    signals.add(signal);

    // We want to make sure that syscalls are automatically restarted,
    // or our logging may break down (and all other sorts of weird stuff).
    struct sigaction fix;
    sigaction(signal, NULL, &fix);
    fix.sa_flags = SA_RESTART;
    sigaction(signal, &fix, NULL);
}

void AgoApp::setupSignals() {
    // Before anything other threads start, mask all signals in all threads.
    // This will then be inherited. In our ioThread we will unmask signals
    // to make sure they are all handled there, and also very important,
    // so syscalls in other threads are not interupted
    AGO_TRACE() << "Masking signals for all threads";
    sigset_t set;
    sigfillset(&set);
    pthread_sigmask(SIG_BLOCK, &set, NULL);

    // Handle these signals
    signal_add_with_restart(SIGINT);
    signal_add_with_restart(SIGTERM);
    signal_add_with_restart(SIGCHLD);

    signals.async_wait(boost::bind(&AgoApp::signal_handler, this,
                                   boost::asio::placeholders::error,
                                   boost::asio::placeholders::signal_number));
}

void AgoApp::signal_handler(const boost::system::error_code& error, int signal_number) {
    AGO_TRACE() << "signal_handler called, error=" << error << " signal_number=" << signal_number;
    if(error) return; // Cancelled; shutdown
    switch (signal_number) {
        case SIGINT:
        case SIGTERM:
            AGO_DEBUG() << "Exit signal catched, shutting down";
            signalExit();
            break;

        case SIGCHLD: {
            // Reap completed child processes so that we don't end up with zombies.
            int status = 0;
            pid_t pid;
            while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
                AGO_DEBUG() << "Reaped child " << pid;
            }
            break;
        }
        default:
            AGO_WARNING() << "Unmapped signal " << signal_number << " received";
            break;
    }

    // Wait for next signal
    signals.async_wait(boost::bind(&AgoApp::signal_handler, this,
                                   boost::asio::placeholders::error,
                                   boost::asio::placeholders::signal_number));
}

void AgoApp::signalExit() {
    if(exit_signaled) return;

    exit_signaled = true;

    // Dispatch signalExitThr call in separate thread, for application
    // to do more serious shutdown procedures
    // TODO: Revise now when it's running in boost::asio
    boost::thread t(boost::bind(&AgoApp::doShutdown, this));
    t.detach();
}

void AgoApp::doShutdown() {
    try {
        if(agoConnection)
            agoConnection->shutdown();

    } catch(std::exception &e) {
    }
}

void AgoApp::iothread_run() {
    // Re-enable signals in this threads
    AGO_TRACE() << "Unmasking signals";
    sigset_t set;
    sigfillset(&set);
    pthread_sigmask(SIG_UNBLOCK, &set, NULL);

    // Leave thread to running the ioService
    AGO_TRACE() << "Handing over to io_service::run";
    ioService_.run();
}

void AgoApp::setupIoThread() {
    ioWork = std::unique_ptr<boost::asio::io_service::work>(
            new boost::asio::io_service::work(ioService_)
    );

    // This thread will run until we're out of work.
    AGO_TRACE() << "Starting IO thread";
    ioThread = boost::thread(boost::bind(&AgoApp::iothread_run, this));
    // &boost::asio::io_service::run, &ioService_));
}

void AgoApp::cleanupIoThread(){
    // Stop listening for signals (it blocks the ioService)
    signals.cancel();
    if(!ioThread.joinable()) {
        AGO_TRACE() << "No IO thread alive";
        return;
    }

    AGO_TRACE() << "Resetting work & joining IO thread, waiting for it to exit";
    ioWork.reset();
    ioThread.join();
    AGO_TRACE() << "IO thread dead";
}




int AgoApp::appMain() {
    agoConnection->run();
    return 0;
}

int AgoApp::main(int argc, const char **argv) {
    try {
        if(parseCommandLine(argc, argv) != 0)
            return 1;
        setup();
    }catch(StartupError &e) {
        cleanup();
        return 1;
    }catch(ConfigurationError &e) {
        std::cerr << "Failed to start " << appName
            << " due to configuration error: "
            << e.what()
            << std::endl;

        cleanup();
        return 1;
    }

    AGO_INFO() << "Starting " << appName;

    int ret = this->appMain();

    AGO_DEBUG() << "Shutting down " << appName;

    cleanup();

    if(ret == 0)
        AGO_INFO() << "Exiting " << appName << "(code " << ret << ")";
    else
        AGO_WARNING() << "Exiting " << appName << "(code " << ret << ")";

    return ret;
}


}/*namespace agocontrol */
