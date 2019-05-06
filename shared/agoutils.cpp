#include "agoutils.h"

#include <string>
#include <memory>

#include <string.h> // for strerror_r

#include <uuid/uuid.h>

std::string agocontrol::utils::generateUuid() {
    std::unique_ptr<char[]> name(new char[38]);
    uuid_t tmpuuid;
    uuid_generate(tmpuuid);
    uuid_unparse(tmpuuid, name.get());
    return std::string(name.get());
}

/*
 * Thread-safe and portable version of strerror_r
 * On some platforms, strerror returns a char*, and on some others a int code.
 *
 * Using this templatized solution, the compiler picks the right one..
 * except we get unused warnings.. use the macros suggested in Linux manpage.
 *
 * https://stackoverflow.com/questions/41953104/strerror-r-is-incorrectly-declared-on-alpine-linux
 */
char* check_error(int result, char* buffer, int err) {
    if(result)
        sprintf(buffer, "unknown error: %d", err);
    return buffer;
}

char* check_error(char* result, char*, int) {
    return result;
}

std::string agocontrol::utils::strerror(int errno_) {
    char buffer[1024];
    return std::string(check_error(strerror_r(errno_, buffer, 1024), buffer, errno_));
}
