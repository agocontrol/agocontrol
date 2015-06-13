#ifndef LIBPLUGWISE_JOINCIRCLERESPONSE_HPP
#define LIBPLUGWISE_JOINCIRCLERESPONSE_HPP 1

#include <common.hpp>
#include <response.hpp>
#include <string>

namespace plugwise {
  class JoinCircleResponse : public Response {
    public:
      typedef std::tr1::shared_ptr<JoinCircleResponse> Ptr;
      JoinCircleResponse ( const std::string& line1, 
                          const std::string& line2) :
        Response(line1, line2) { parse_line2(); };
      virtual std::string str();
      virtual ~JoinCircleResponse() {};
      bool is_ok();
      virtual bool req_successful();
      std::string stick_mac_addr() { return _stick_mac_addr; };

    private:
      JoinCircleResponse (const JoinCircleResponse& original);
      JoinCircleResponse& operator= (const JoinCircleResponse& rhs);
      void parse_line2();
      uint32_t _response_code;
      uint32_t _resp_seq_number;
      std::string _stick_mac_addr;
  };
  
};


#endif /* LIBPLUGWISE_STICKINITRESPONSE_HPP */

