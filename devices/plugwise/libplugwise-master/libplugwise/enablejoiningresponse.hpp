#ifndef LIBPLUGWISE_ENABLEJOININGRESPONSE_HPP
#define LIBPLUGWISE_ENABLEJOININGRESPONSE_HPP 1

#include <common.hpp>
#include <response.hpp>
#include <string>

namespace plugwise {
  class EnableJoiningResponse : public Response {
    public:
      typedef std::tr1::shared_ptr<EnableJoiningResponse> Ptr;
      EnableJoiningResponse ( const std::string& line1, 
                          const std::string& line2) :
        Response(line1, line2) { parse_line2(); };
      virtual std::string str();
      virtual ~EnableJoiningResponse() {};
      bool is_ok();
      virtual bool req_successful();
      std::string stick_mac_addr() { return _stick_mac_addr; };

    private:
      EnableJoiningResponse (const EnableJoiningResponse& original);
      EnableJoiningResponse& operator= (const EnableJoiningResponse& rhs);
      void parse_line2();
      uint32_t _response_code;
      uint32_t _resp_seq_number;
      std::string _stick_mac_addr;
  };
  
};


#endif /* LIBPLUGWISE_STICKINITRESPONSE_HPP */

