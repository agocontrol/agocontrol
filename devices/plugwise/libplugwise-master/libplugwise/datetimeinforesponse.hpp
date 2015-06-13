#ifndef LIBPLUGWISE_DATETIMEINFORESPONSE_HPP
#define LIBPLUGWISE_DATETIMEINFORESPONSE_HPP 1

#include <common.hpp>
#include <response.hpp>
#include <string>

namespace plugwise {
  class DateTimeInfoResponse : public Response {
    public:
      typedef std::tr1::shared_ptr<DateTimeInfoResponse> Ptr;
      DateTimeInfoResponse ( const std::string& line1, 
                          const std::string& line2) :
        Response(line1, line2) { parse_line2(); };
      virtual std::string str();
      virtual ~DateTimeInfoResponse() {};
      bool is_ok();
      virtual bool req_successful();
      std::string circle_plus_mac_addr() { return _circle_plus_mac_addr; };

    private:
      DateTimeInfoResponse (const DateTimeInfoResponse& original);
      DateTimeInfoResponse& operator= (const DateTimeInfoResponse& rhs);
      void parse_line2();
      uint32_t _response_code;
      uint32_t _resp_seq_number;
      std::string _circle_plus_mac_addr;
      std::string _time;
      std::string _day_of_week;
      std::string _date;
  };
  
};


#endif /* LIBPLUGWISE_STICKINITRESPONSE_HPP */

