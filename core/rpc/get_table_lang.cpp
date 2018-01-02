#include <iostream>
#include <fstream>
#include <string>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

static bool handleLastModified(const char *filename) {
    char buffer[100];
    struct stat res;
    struct tm *timeinfo;
    struct tm if_modified_tm;

    bool needs_output;

    stat(filename, &res);
    timeinfo = gmtime (&res.st_mtime);
    strftime(buffer, 100, "%a, %d %b %Y %T GMT", timeinfo);

    needs_output = true;

    char *cacheQuery = getenv("HTTP_IF_MODIFIED_SINCE");
    if (cacheQuery != NULL && strptime(cacheQuery, "%a, %d %b %Y %T GMT", &if_modified_tm) != NULL) {
        if (mktime(&if_modified_tm) >= mktime(timeinfo) - timeinfo->tm_isdst * 3600) {
            std::cout << "Status: 304 Not Modified\r\n";
            needs_output = false;
        }
    }

    if (!needs_output) {
        std::cout << "Last-Modified: " << buffer << "\r\n\r\n";
    }
    else {
        std::cout << "\r\n";
    }

    return needs_output;
}

int main(int argc, char **argv){
    std::cout << "Content-Type: text/json; charset=UTF-8\r\n";
    std::cout << "Cache-Control:public\r\n";
    std::string querystring = getenv("QUERY_STRING");
    if (querystring.find("/")!=std::string::npos) return 0;
    if (querystring.find("&")!=std::string::npos) return 0;
    size_t pos = querystring.find("lang=");
    if (pos==std::string::npos) return 0;
    std::string lang= querystring.substr(pos+5,querystring.length());
    if (lang.size() != 2) return 0;
    std::string filename = "../datatables_lang/" + lang + ".txt";

    if (!handleLastModified(filename.c_str())) return 0;

    std::ifstream fin(filename.c_str());
    std::string line;
    if (fin) {
        while (getline(fin,line)) std::cout << line << std::endl;
        fin.close();
    } else {
        if (!handleLastModified("../datatables_lang/en.txt")) return 0;
        std::ifstream fdef("../datatables_lang/en.txt");
        while (getline(fdef,line)) std::cout << line << std::endl;
        fdef.close();
    }

    return 0;
}

