/*
        ##########    Copyright (C) 2015 Vincenzo Pacella
        ##      ##    Distributed under MIT license, see file LICENSE
        ##      ##    or <http://opensource.org/licenses/MIT>
        ##      ##
##########      ############################################################# shaduzlabs.com #####*/

#pragma once

#include <thread>

#include "cabl/comm/DiscoveryPolicy.h"
#include "cabl/devices/Device.h"

//--------------------------------------------------------------------------------------------------

namespace sl
{
namespace cabl
{

//--------------------------------------------------------------------------------------------------

class Client
{
public:
  Client(DiscoveryPolicy discoveryPolicy_ = {}): m_discoveryPolicy(std::move(discoveryPolicy_)) {}

  virtual void disconnected() {}
  virtual void buttonChanged(Device::Button button_, bool buttonState_, bool shiftPressed_) {}
  virtual void encoderChanged(unsigned encoder_, bool valueIncreased_, bool shiftPressed_) {}
  virtual void encoderChangedRaw(unsigned encoder_, double delta_, bool shiftPressed_) {}
  virtual void keyChanged(unsigned index_, double value_, bool shiftPressed) {}
  virtual void keyUpdated(unsigned index_, double value_, bool shiftPressed) {}
  virtual void controlChanged(unsigned pot_, double value_, bool shiftPressed) {}

  virtual void initDevice() {}
  virtual void render() {}

  friend class Coordinator;

protected:
  std::shared_ptr<Device> device()
  {
    return m_pDevice;
  }
  void requestDeviceUpdate()
  {
    m_update = true;
  }

private:
  void onInitDevice();
  void onRender();

  DiscoveryPolicy discoveryPolicy() { return m_discoveryPolicy;}
  void setDevice(std::shared_ptr<Device> pDevice_) {m_pDevice = pDevice_; onInitDevice();}

  uint8_t encoderValue(
    bool valueIncreased_,
    unsigned step_,
	  unsigned currentValue_,
    unsigned minValue_,
	  unsigned maxValue_
  );

  std::shared_ptr<Device> m_pDevice;
  DiscoveryPolicy m_discoveryPolicy;

  std::atomic<bool> m_update{true};
};

//--------------------------------------------------------------------------------------------------

} // namespace cabl
} // namespace sl
