#ifndef AGOPROTO_H
#define AGOPROTO_H

#include <string>
#include <json/json.h>

#include "response_codes.h"

namespace agocontrol {
    /**
     * These methods shall be used to build valid command handler response maps
     */
    Json::Value responseResult(const std::string& identifier);
    Json::Value responseResult(const std::string& identifier, const std::string& message);
    Json::Value responseResult(const std::string& identifier, const std::string& message, const Json::Value& data);
    Json::Value responseResult(const std::string& identifier, const Json::Value& data);

    Json::Value responseError(const std::string& identifier, const std::string& message, const Json::Value& data);
    Json::Value responseError(const std::string& identifier, const std::string& message);

    // Shortcut to send responseError(RESPONSE_ERR_FAILED, message)
    Json::Value responseFailed(const std::string& message);
    Json::Value responseFailed(const std::string& message, const Json::Value& data);

#define responseUnknownCommand() \
    responseError(RESPONSE_ERR_UNKNOWN_COMMAND, "Command not supported");

#define responseNoDeviceCommands() \
    responseError(RESPONSE_ERR_NO_DEVICE_COMMANDS, "Device does not have any commands")

    // Shortcut to send responseResult(RESPONSE_SUCCESS, ...)
    Json::Value responseSuccess();
    Json::Value responseSuccess(const char *message);
    Json::Value responseSuccess(const std::string& message);
    Json::Value responseSuccess(const Json::Value& data);
    Json::Value responseSuccess(const std::string& message, const Json::Value& data);


    /**
     * An exception which can be thrown from a CommandHandler in order to signal failure.
     * This will be catched by the code calling the CommandHandler, and will then be
     * translated into a proper response.
     */
    class AgoCommandException: public std::exception {
    public:
        AgoCommandException(const std::string& identifier_, const std::string& message_)
            : identifier(identifier_)
            , message(message_)
        {
        }
        ~AgoCommandException() throw() {};

        Json::Value toResponse() const {
            return responseError(identifier, message);
        }

        const char* what() const throw() {
            return identifier.c_str();
        }

    private:
        const std::string identifier;
        const std::string message;
    };

    /**
     * Precondition functions to verify requests, and possibly throw AgoCommandExceptions
     */
    
    // Ensure that the message map has an 'key' entry, with a non-void value
    void checkMsgParameter(const Json::Value& content, const std::string& key);

    // Ensure that the message map has an 'key' entry, with a non-void value
    // of the specified type. If allowEmpty is set to false (default), and type is string,
    // we check for empty string too. For other types, allowEmpty is ignored.
    void checkMsgParameter(const Json::Value& content, const std::string& key,
            Json::ValueType type,
            bool allowEmpty=false);



    /**
     * When sending requests FROM an app, AgoClient will deserialize them into a 
     * AgoResponse object which can be interacted with easilly
     */
    class AgoConnection;
    class AgoResponse {
        friend class AgoConnection;
    protected:
        Json::Value response;
        Json::Value root;
        void init(const Json::Value& response);
        void validate();
    public:
        AgoResponse(){};
        AgoResponse(AgoResponse&& rhs) noexcept;
        AgoResponse& operator=(AgoResponse&& rhs) noexcept;

        // Return true if we have an "error" element
        bool isError() const;

        // Return true if we have a "result" element
        bool isOk() const;

        // Get the "identifier" field from either type of response
        std::string getIdentifier() /*const*/;

        // Get the "message" field from either type of response
        std::string getMessage() /*const*/;

        // Get either "result.data" or "error.data"
        const Json::Value& getData() /*const*/;

        // Get the raw message; only use in agorpc!
        Json::Value& getResponse() { return response; };
    };

}/* namespace agocontrol */

#endif
