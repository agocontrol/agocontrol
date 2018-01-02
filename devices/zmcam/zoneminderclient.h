/*
     Copyright (C) 2014 Jimmy Rentz <rentzjam@gmail.com>

     This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License.
     This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
     of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

     See the GNU General Public License for more details.

     curl helpers from User Galik on http://www.cplusplus.com/user/Galik/ - http://www.cplusplus.com/forum/unices/45878/

*/

#if !defined(ZONEMINDER_CLIENT_H_INCLUDED_)
#define ZONEMINDER_CLIENT_H_INCLUDED_

#include <string>
#include <sstream>

enum ZM_AUTH_TYPE
{
    ZM_AUTH_NONE = 0,
    ZM_AUTH_PLAIN = 1,
    ZM_AUTH_HASH = 2,
};

class ZoneminderClient
{
public:
    ZoneminderClient();
    ~ZoneminderClient();
    
    bool create(std::string webProtocol,
               std::string hostName,
               unsigned int webPort,
               ZM_AUTH_TYPE webAuthType,
               bool webAuthUseLocalAddress,
               std::string webAuthUserName,
               std::string webAuthPasword,
               std::string webAuthHashSecret,
               unsigned int triggerPort);
    void destroy(void);
    
    bool getVideoFrame(int monitorId, std::ostringstream& outputStream);
    bool setMonitorAlert(int monitorId,
                         unsigned int durationSeconds,
                         int score,
                         std::string causeText,
                         std::string eventText,
                         std::string showText);
    bool clearMonitorAlert(int monitorId);
    
private:
    bool m_initialized;

    std::string m_hostName;
    
    unsigned int m_webPort;
    std::string m_webUrlBase;
    ZM_AUTH_TYPE m_webAuthType;
    std::string m_webAuthBase;
    std::string m_webAuthLocalAddress;
    bool m_webAuthUseLocalAddress;
    
    unsigned int m_triggerPort;
    
    void buildWebUrlBaseString(std::string webProtocol,
                               std::string hostName,
                               unsigned int webPort);
    std::string generateMySqlPassword(std::string password);
    bool getLocalWebSocketAddress(void);
    std::string buildWebAuthString(void);
    
    bool sendTriggerCommand(std::string command);
};


#endif // ZONEMINDER_CLIENT_H_INCLUDED_

