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

#include "requestfactory.hpp"

using namespace plugwise;

EnableJoiningRequest::Ptr RequestFactory::getEnableJoiningRequest(const std::string& _mode) const {
  return EnableJoiningRequest::Ptr(new EnableJoiningRequest(_mode));
}

JoinCircleRequest::Ptr RequestFactory::getJoinCircleRequest(const std::string& _device_id, const std::string& _permission) const {
  return JoinCircleRequest::Ptr(new JoinCircleRequest(_device_id, _permission));
}

RemoveCircleRequest::Ptr RequestFactory::getRemoveCircleRequest(const std::string& _circleplus_mac, const std::string& _device_id) const {
  return RemoveCircleRequest::Ptr(new RemoveCircleRequest(_circleplus_mac, _device_id));
}

StickInitRequest::Ptr RequestFactory::getStickInitRequest() const {
  return StickInitRequest::Ptr(new StickInitRequest());
}

ClockSetRequest::Ptr RequestFactory::getClockSetRequest(const std::string& _device_id) const {
  return ClockSetRequest::Ptr(new ClockSetRequest(_device_id));
}

ClockInfoRequest::Ptr RequestFactory::getClockInfoRequest(const std::string& _device_id) const {
  return ClockInfoRequest::Ptr(new ClockInfoRequest(_device_id));
}

SetDateTimeRequest::Ptr RequestFactory::getSetDateTimeRequest(const std::string& _device_id) const {
  return SetDateTimeRequest::Ptr(new SetDateTimeRequest(_device_id));
}

DateTimeInfoRequest::Ptr RequestFactory::getDateTimeInfoRequest(const std::string& _device_id) const {
  return DateTimeInfoRequest::Ptr(new DateTimeInfoRequest(_device_id));
}

InfoRequest::Ptr RequestFactory::getInfoRequest(const std::string& _device_id) const {
  return InfoRequest::Ptr(new InfoRequest(_device_id));
}

PingRequest::Ptr RequestFactory::getPingRequest(const std::string& _device_id) const {
  return PingRequest::Ptr(new PingRequest(_device_id));
}

NodeQueryRequest::Ptr RequestFactory::getNodeQueryRequest(const std::string& _device_id, int _index) const {
  return NodeQueryRequest::Ptr(new NodeQueryRequest(_device_id, _index));
}

SwitchRequest::Ptr RequestFactory::getSwitchRequest(const std::string& _device_id, int _mode) const {
  return SwitchRequest::Ptr(new SwitchRequest(_device_id, _mode));
}

QueryCirclePlusRequest::Ptr RequestFactory::getQueryCirclePlusRequest(const std::string& _device_id) const {
  return QueryCirclePlusRequest::Ptr(new QueryCirclePlusRequest(_device_id));
}

CalibrationRequest::Ptr RequestFactory::getCalibrationRequest(const std::string& _device_id) const {
  return CalibrationRequest::Ptr(new CalibrationRequest(_device_id));
}

PowerInformationRequest::Ptr RequestFactory::getPowerInformationRequest(const std::string& _device_id) const {
  return PowerInformationRequest::Ptr(new PowerInformationRequest(_device_id));
}

FeatureSetRequest::Ptr RequestFactory::getFeatureSetRequest(const std::string& _device_id) const {
  return FeatureSetRequest::Ptr(new FeatureSetRequest(_device_id));
}

SetBroadcastIntervalRequest::Ptr RequestFactory::getSetBroadcastIntervalRequest(const std::string& _device_id) const {
  return SetBroadcastIntervalRequest::Ptr(new SetBroadcastIntervalRequest(_device_id));
}

