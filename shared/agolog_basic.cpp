/**
 * Basic logging implementation, when Boost.Log is not available.
 */

#include <boost/thread/locks.hpp>
#include <syslog.h>
#include "agolog.h"

namespace agocontrol {
namespace log {

static bool inited = false;

// TODO: Decide on levels. On FreeBSD at least, the LOG_INFO is normally
// not logged, only notice..
static int sev2syslog[] = {
    LOG_DEBUG,
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERR,
    LOG_CRIT
};

void console_sink::output_record(record &rec) {
    std::stringstream out;
    const std::string &lvl = log_container::getLevel(rec.level);

    out
        << boost::posix_time::to_simple_string(rec.timestamp)
        << " [" << lvl << "] "
        << rec.stream_.str()
        << std::endl;

    boost::lock_guard<boost::mutex> lock(sink_mutex);
    std::cout << out.str();
}


syslog_sink::syslog_sink(std::string const &ident, int facility)
{
    strncpy(this->ident, ident.c_str(), sizeof(this->ident));
    openlog(this->ident, LOG_PID, facility);
}

void syslog_sink::output_record(record &rec) {
    syslog(sev2syslog[rec.level], "%s", rec.stream_.str().c_str());
}


record_pump::~record_pump()
{
    assert(rec.state == record::closed);
    logger.push_record(rec);
}



static AGO_LOGGER_IMPL default_inst;
AGO_LOGGER_IMPL & log_container::get() {
    return default_inst;
}

void log_container::setOutputConsole() {
    get().setSink(boost::shared_ptr<log_sink>( new console_sink() ) );

    std::cout.exceptions ( std::ostream::failbit | std::ostream::badbit );
}

void log_container::setOutputSyslog(const std::string &ident, int facility) {
    get().setSink( boost::shared_ptr<log_sink>(new syslog_sink(ident.c_str(), facility)) );
}

void log_container::initDefault() {
    if(inited)
        return;
    inited = true;
    // Nothing to init here.. the simple_logger instance constructor sets default level & creates
    // console sink
}

void log_container::setCurrentLevel(severity_level lvl, const std::map<std::string, severity_level>& ignored) {
    get().setLevel(lvl);
}

simple_logger::simple_logger()
    : current_level(log_container::getDefaultLevel())
    , sink( boost::shared_ptr<log_sink>(new console_sink()) )
{}

}/* namespace log */
}/* namespace agocontrol */
