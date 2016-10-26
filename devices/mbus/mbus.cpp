/*
   Copyright (C) 2016 Harald Klein <hari@vt100.at>

   This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

   See the GNU General Public License for more details.

*/
#include <iostream>
#include <sstream>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <termios.h>
#include <stdio.h>

#include "agoapp.h"

using namespace qpid::types;
using namespace std;
using namespace agocontrol;

class AgoMbus: public AgoApp {
private:
    void setupApp();
    qpid::types::Variant::Map commandHandler(qpid::types::Variant::Map command);

public:
    AGOAPP_CONSTRUCTOR_HEAD(AgoMbus)
        {
            // Compatability with old configuration section
            appConfigSection = "mbus";
        }
};

// commandhandler
qpid::types::Variant::Map AgoMbus::commandHandler(qpid::types::Variant::Map content) {
    string addr = content["internalid"].asString();
    if (content["command"] == "on") {
    } else if (content["command"] == "off") {
    }
    return responseSuccess();
}

void AgoMbus::setupApp() {

    std::string device = getConfigOption("device", "/dev/ttyUSB0");

    addCommandHandler();

}

AGOAPP_ENTRY_POINT(AgoMbus);
