import logging
from six import PY2

# We want to keep C++ and Python ago-coding as similar as possible.
# In C++ we have levels TRACE and CRITICAL.
if PY2:
    _logginglevelNames = logging._levelNames
else:
    _logginglevelNames = logging._levelToName

# Add extra Trace level
TRACE = logging.TRACE = 5
_logginglevelNames[TRACE] = TRACE
_logginglevelNames['TRACE'] = TRACE

# C++ impl has FATAL, in python we have CRITICAL
# Add this alias so we can use the same logging consts
FATAL = logging.FATAL = logging.CRITICAL
_logginglevelNames[FATAL] = FATAL
_logginglevelNames['FATAL'] = FATAL

LOGGING_LOGGER_CLASS = logging.getLoggerClass()

class AgoLoggingClass(LOGGING_LOGGER_CLASS):
    def trace(self, msg, *args, **kwargs):
        if self.isEnabledFor(TRACE):
            self._log(TRACE, msg, args, **kwargs)

    def fatal(self, msg, *args, **kwargs):
        if self.isEnabledFor(FATAL):
            self._log(FATAL, msg, args, **kwargs)


if logging.getLoggerClass() is not AgoLoggingClass:
    logging.setLoggerClass(AgoLoggingClass)
    logging.addLevelName(TRACE, 'TRACE')
    logging.addLevelName(FATAL, 'FATAL')


# TODO: Syslog priority mapping
