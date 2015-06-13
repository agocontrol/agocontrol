#ifndef LIBPLUGWISE_CLOCKINFORESPONSE_HPP
#define LIBPLUGWISE_CLOCKINFORESPONSE_HPP 1

#include <common.hpp>
#include <response.hpp>
#include <string>

namespace plugwise {
  class ClockInfoResponse : public Response {
    public:
      typedef std::tr1::shared_ptr<ClockInfoResponse> Ptr;
      ClockInfoResponse ( const std::string& line1, 
                          const std::string& line2) :
        Response(line1, line2) { parse_line2(); };
      virtual std::string str();
      virtual ~ClockInfoResponse() {};
      bool is_ok();
      virtual bool req_successful();
      std::string circle_plus_mac_addr() { return _circle_plus_mac_addr; };

    private:
      ClockInfoResponse (const ClockInfoResponse& original);
      ClockInfoResponse& operator= (const ClockInfoResponse& rhs);
      void parse_line2();
      uint32_t _response_code;
      uint32_t _resp_seq_number;
      std::string _circle_plus_mac_addr;
      std::string _time;
      std::string _day_of_week;
  };
  
};


#endif /* LIBPLUGWISE_STICKINITRESPONSE_HPP */

