/**
 * Boost.Log based logging code, this mainly contains functions to set logging
 * level and sinks.
 */

#define BOOST_LOG_USE_NATIVE_SYSLOG

#include <boost/shared_ptr.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/log/sinks.hpp>
#include <boost/log/sinks/syslog_backend.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/exception_handler.hpp>
#include <boost/log/support/date_time.hpp>

// Avoid warning on empty_deleter_deprecated
#if BOOST_VERSION >= 105600
#include <boost/core/null_deleter.hpp>
#else
#include <boost/utility/empty_deleter.hpp>
#endif

#include "agolog.h"

namespace agocontrol {
namespace log {

static bool inited = false;
static std::string syslog_ident;

namespace logging = boost::log;
namespace sinks = boost::log::sinks;
namespace keywords = boost::log::keywords;
namespace expr = boost::log::expressions;
namespace attrs = boost::log::attributes;


BOOST_LOG_ATTRIBUTE_KEYWORD(severity, "Severity", severity_level);
BOOST_LOG_ATTRIBUTE_KEYWORD(channel, "Channel", std::string)


// The formatting logic for the severities
template< typename CharT, typename TraitsT >
inline std::basic_ostream< CharT, TraitsT >& operator<< (
        std::basic_ostream< CharT, TraitsT >& strm,
        severity_level lvl)
{
    strm << log_container::getLevel(lvl);
    return strm;
}

// http://www.boost.org/doc/libs/1_63_0/libs/log/doc/html/log/detailed/utilities.html#log.detailed.utilities.exception_handlers
struct ago_log_exception_handler
{
    typedef void result_type;

    void operator() (std::runtime_error const& e) const
    {
        std::cout << "boost log std::runtime_error: " << e.what()  << " errno=" << errno << std::endl;
        throw;
    }
    void operator() (std::logic_error const& e) const
    {
        std::cout << "boost log std::logic_error: " << e.what()  << " errno=" << errno << std::endl;
        throw;
    }
};

static AGO_LOGGER_IMPL default_inst(boost::log::keywords::channel = "app");
AGO_LOGGER_IMPL & log_container::get() {
    return default_inst;
}

void log_container::initDefault() {
    if(inited)
        return;
    inited = true;

    logging::add_common_attributes();
    setOutputConsole();
    setCurrentLevel(getDefaultLevel(), std::map<std::string, severity_level>());

    // Setup exception handler
    logging::core::get()->set_exception_handler(logging::make_exception_handler<
            std::runtime_error,
            std::logic_error
       >(ago_log_exception_handler()));

}

/**
 * Configure logging levels.
 *
 * minOutputLevel controls overall minimum level, i.e. if set to INFO nothing lower than INFO is logged.
 * In addition, it's possible to configure minimum levels for different channels, via the channelLevels map.
 * I.e. if "mqtt" channel is ocnfigured with INFO, but minOutputLevel is TRACE, then we still won't see anything
 * lower than INFO from mqtt.
 * If it's instead set to TRACE, and we have minOutputLevel to DEBUG, we will still only see DEBUG from that channel.
 */
void log_container::setCurrentLevel(severity_level minOutputLevel, const std::map<std::string, severity_level>& channelLevels) {
    typedef expr::channel_severity_filter_actor< std::string, severity_level > severity_filter;
    severity_filter min_severity = expr::channel_severity_filter(channel, severity);

    for(auto i = channelLevels.cbegin(); i != channelLevels.cend(); i++) {
        min_severity[i->first] = i->second;
    }
    min_severity.set_default(true);

    boost::log::core::get()->set_filter
        (
            min_severity && ::agocontrol::log::severity >= minOutputLevel
        );
}


void log_container::setOutputConsole() {
    boost::shared_ptr< logging::core > core = logging::core::get();

    // Create a backend and attach a couple of streams to it
    boost::shared_ptr< sinks::text_ostream_backend > backend =
        boost::make_shared< sinks::text_ostream_backend >();

    backend->add_stream(
            boost::shared_ptr< std::ostream >(&std::clog,
#if BOOST_VERSION >= 105600
                                              boost::null_deleter()
#else
                                              boost::empty_deleter()
#endif
            )
            );

    // If our output stream fails, make sure we throw exception rather than
    // just stop logging
    std::clog.exceptions ( std::ostream::failbit | std::ostream::badbit );

    // Enable auto-flushing after each log record written
    backend->auto_flush(true);

    // Wrap it into the frontend and register in the core.
    // The backend requires synchronization in the frontend.
    typedef sinks::synchronous_sink< sinks::text_ostream_backend > sink_t;
    boost::shared_ptr< sink_t > sink(new sink_t(backend));

    sink->set_formatter
        (
         expr::stream
         << expr::format_date_time< boost::posix_time::ptime >("TimeStamp", "%Y-%m-%d %H:%M:%S.%f")
         << " [" << expr::attr< attrs::current_thread_id::value_type > ("ThreadID") << "]"
         << " [" << std::setw(6) << std::left <<  severity << std::setw(0)
         << " : " << std::setw(5) << std::right << channel << std::setw(0) << "] "
         << expr::smessage
        );

    core->remove_all_sinks();
    core->add_sink(sink);
}

void log_container::setOutputSyslog(const std::string &ident, int facility) {
    boost::shared_ptr< logging::core > core = logging::core::get();

    // The ident string passed into the syslog_backend
    // must be keept allocated. syslog_ident is static
    syslog_ident = ident;

    // Create a backend and attach a couple of streams to it
    boost::shared_ptr< sinks::syslog_backend > backend(
            new sinks::syslog_backend(
                keywords::facility = sinks::syslog::make_facility(facility),
                keywords::ident = syslog_ident,
                keywords::use_impl = sinks::syslog::native
                )
            );

    sinks::syslog::custom_severity_mapping <severity_level> mapping("Severity");
    mapping[trace] = sinks::syslog::debug;
    mapping[debug] = sinks::syslog::debug;
    mapping[info] = sinks::syslog::info;
    mapping[warning] = sinks::syslog::warning;
    mapping[error] = sinks::syslog::error;
    mapping[fatal] = sinks::syslog::critical;

    // Wrap it into the frontend and register in the core.
    // The backend requires synchronization in the frontend.
    typedef sinks::synchronous_sink< sinks::syslog_backend > sink_t;
    boost::shared_ptr< sink_t > sink(new sink_t(backend));

    /*
       sink->set_formatter
       (
       expr::stream
    //			<< expr::format_date_time< boost::posix_time::ptime >("TimeStamp", "%Y-%m-%d %H:%M:%S.%f")
    //			<< " [" << expr::attr< attrs::current_thread_id::value_type > ("ThreadID") << "] "
    << " [" << agocontrol::log::severity << "] "
    << expr::smessage
    );
    */

    core->remove_all_sinks();
    core->add_sink(sink);
}

}/* namespace log */
}/* namespace agocontrol */
