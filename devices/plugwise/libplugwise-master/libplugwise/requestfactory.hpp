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


#ifndef LIBPLUGWISE_REQUESTFACTORY_HPP
#define LIBPLUGWISE_REQUESTFACTORY_HPP 1

#include "common.hpp"
#include <enablejoiningrequest.hpp>
#include <joincirclerequest.hpp>
#include <removecirclerequest.hpp>
#include <stickinitrequest.hpp>
#include <clocksetrequest.hpp>
#include <clockinforequest.hpp>
#include <setdatetimerequest.hpp>
#include <datetimeinforequest.hpp>
#include <inforequest.hpp>
#include <pingrequest.hpp>
#include <nodequeryrequest.hpp>
#include <querycircleplusrequest.hpp>
#include <calibrationrequest.hpp>
#include <powerinformationrequest.hpp>
#include <featuresetrequest.hpp>
#include <setbroadcastintervalrequest.hpp>
#include <switchrequest.hpp>

namespace plugwise {
  class RequestFactory {
    public:
      typedef std::tr1::shared_ptr<RequestFactory> Ptr;
      RequestFactory () {};
      EnableJoiningRequest::Ptr getEnableJoiningRequest(const std::string& _mode) const;
      JoinCircleRequest::Ptr getJoinCircleRequest(const std::string& _device_id, const std::string& _permission) const;
      RemoveCircleRequest::Ptr getRemoveCircleRequest(const std::string& circleplus_mac, const std::string& _device_id) const;
      StickInitRequest::Ptr getStickInitRequest() const;
      InfoRequest::Ptr getInfoRequest(const std::string& _device_id) const;
      PingRequest::Ptr getPingRequest(const std::string& _device_id) const;
      ClockSetRequest::Ptr getClockSetRequest(const std::string& _device_id) const;
      ClockInfoRequest::Ptr getClockInfoRequest(const std::string& _device_id) const;
      SetDateTimeRequest::Ptr getSetDateTimeRequest(const std::string& _device_id) const;
      DateTimeInfoRequest::Ptr getDateTimeInfoRequest(const std::string& _device_id) const;
      NodeQueryRequest::Ptr getNodeQueryRequest(const std::string& _device_id, int _index) const;
      QueryCirclePlusRequest::Ptr getQueryCirclePlusRequest(const std::string& _device_id) const;
      CalibrationRequest::Ptr getCalibrationRequest(const std::string& _device_id) const;
      PowerInformationRequest::Ptr getPowerInformationRequest(const std::string& _device_id) const;
      FeatureSetRequest::Ptr getFeatureSetRequest(const std::string& _device_id) const;
      SetBroadcastIntervalRequest::Ptr getSetBroadcastIntervalRequest(const std::string& _device_id) const;
      SwitchRequest::Ptr getSwitchRequest(const std::string& _device_id, int _mode) const;
      virtual ~RequestFactory() {};

    private:
      RequestFactory (const RequestFactory& original);
      RequestFactory& operator= (const RequestFactory& rhs);
      
  };
  
};


#endif /* LIBPLUGWISE_REQUESTFACTORY_HPP */

