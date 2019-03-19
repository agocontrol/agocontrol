#ifndef AGOCONTROL_AGOUTILS_H
#define AGOCONTROL_AGOUTILS_H

#include <string>

namespace agocontrol {
    namespace utils {
        /**
         * Helper to generate a UUID as a string.
         * @return
         */
        std::string generateUuid();

        /**
         * A portable & threadsafe version of strerror, for simpler C++ use.
         */
        std::string strerror(int errno_);
    }
}

#endif //AGOCONTROL_AGOUTILS_H
