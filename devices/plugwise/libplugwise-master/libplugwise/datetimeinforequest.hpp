/**
 * This file is part of libplugwise.
 *
 * (c) Fraunhofer ITWM - Mathias Dalheimer <dalheimer@itwm.fhg.de>, 2010
 *
 * libplugwise is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * libplugwise is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libplugwise. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef LIBPLUGWISE_DATETIMEINFOREQUEST_HPP
#define LIBPLUGWISE_DATETIMEINFOREQUEST_HPP 1

#include <request.hpp>
#include <connection.hpp>

namespace plugwise {
  class DateTimeInfoRequest : public Request {
    public:
      typedef std::tr1::shared_ptr<DateTimeInfoRequest> Ptr;
      DateTimeInfoRequest (std::string circle_plus_mac) :
        _circle_plus_mac(circle_plus_mac) {};

      void send(plugwise::Connection::Ptr con);
      virtual ~DateTimeInfoRequest() {};

    private:
      DateTimeInfoRequest (const DateTimeInfoRequest& original);
      DateTimeInfoRequest& operator= (const DateTimeInfoRequest& rhs);
      std::string _circle_plus_mac;
      
  };
  
};


#endif /* LIBPLUGWISE_STICKINITREQUEST_HPP */

