/*
 * aGoControl messagesend - CLI for sending commands
 */

#include <stdlib.h> 

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <vector>

#include "agoclient.h"

using namespace agocontrol;

using std::stringstream;
using std::string;

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cout << "Usage example: " << argv[0] << " uuid=ca9424e6-406d-4144-8931-584046eaaa34 command=setlevel level=50" << std::endl;
        return -1;
    }
    AgoConnection agoConnection("messagesend");
    agoConnection.start();

    Json::Value content;
    std::string subject;
    subject = "";
    for (int i=1;i<argc;i++) {
        string name, value;
        if (nameval(string(argv[i]),name, value)) {
            if (name == "subject") {
                subject = value;
            } else {
                content[name]=Json::Value(value);
            }
        }
    }

    std::cout << "Sending message: " << content << std::endl;
    if(subject.empty()) {
        AgoResponse response = agoConnection.sendRequest(content);
        if(response.isOk()) {
            std::cout << "Success: " << response.getResponse() << std::endl;
        }else{
            std::cout << "Error: " << response.getResponse() << std::endl;
        }
    } else {
        agoConnection.sendMessage(subject, content);
    }
}


