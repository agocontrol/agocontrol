#ifndef LIBPLUGWISE_PINGRESPONSE_HPP
#define LIBPLUGWISE_PINGRESPONSE_HPP 1

#include <common.hpp>
#include <response.hpp>
#include <string>

namespace plugwise {
  class PingResponse : public Response {
    public:
      typedef std::tr1::shared_ptr<PingResponse> Ptr;
      PingResponse ( const std::string& line1, 
                          const std::string& line2) :
        Response(line1, line2) { parse_line2(); };
      virtual std::string str();
      virtual ~PingResponse() {};
      bool is_ok();
      virtual bool req_successful();
      std::string circle_plus_mac_addr() { return _circle_mac_addr; };
      uint32_t ping_time() {return _ping_time;};

    private:
      PingResponse (const PingResponse& original);
      PingResponse& operator= (const PingResponse& rhs);
      void parse_line2();
      uint32_t _response_code;
      uint32_t _resp_seq_number;
      std::string _circle_mac_addr;
      uint32_t _qin;
      uint32_t _qout;
      uint32_t _ping_time;
  };
  
};


#endif /* LIBPLUGWISE_STICKINITRESPONSE_HPP */

