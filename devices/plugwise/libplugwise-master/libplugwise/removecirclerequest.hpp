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

#ifndef LIBPLUGWISE_REMOVECIRCLEREQUEST_HPP
#define LIBPLUGWISE_REMOVECIRCLEREQUEST_HPP 1

#include <request.hpp>
#include <connection.hpp>

namespace plugwise {
  class RemoveCircleRequest : public Request {
    public:
      typedef std::tr1::shared_ptr<RemoveCircleRequest> Ptr;
      RemoveCircleRequest (std::string circleplus_mac, std::string device_id) :
        _circleplus_mac(circleplus_mac),_device_id(device_id) {};
      void send(plugwise::Connection::Ptr con);
      virtual ~RemoveCircleRequest() {};

    private:
      RemoveCircleRequest (const RemoveCircleRequest& original);
      RemoveCircleRequest& operator= (const RemoveCircleRequest& rhs);
      std::string _circleplus_mac;
      std::string _device_id;
      
  };
  
};


#endif /* LIBPLUGWISE_STICKINITREQUEST_HPP */

