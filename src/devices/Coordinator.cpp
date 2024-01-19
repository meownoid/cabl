/*
        ##########    Copyright (C) 2015 Vincenzo Pacella
        ##      ##    Distributed under MIT license, see file LICENSE
        ##      ##    or <http://opensource.org/licenses/MIT>
        ##      ##
##########      ############################################################# shaduzlabs.com #####*/

#include "cabl/devices/Coordinator.h"

#include <algorithm>

#include "cabl/cabl.h"
#include "cabl/devices/DeviceFactory.h"
#include "comm/drivers/LibUSB/DriverLibUSB.h"

//--------------------------------------------------------------------------------------------------

namespace sl
{
namespace cabl
{

//--------------------------------------------------------------------------------------------------

Coordinator::Coordinator(Client* client)
{
  M_LOG("[Coordinator] constructor");
  m_pClient = client;

  driver(Driver::Type::LibUSB)->setHotplugCallback(
    [this](DeviceDescriptor deviceDescriptor_, bool plugged_) { scan(); }
  );
}

//--------------------------------------------------------------------------------------------------

Coordinator::~Coordinator()
{
  M_LOG("[Coordinator] destructor");

  driver(Driver::Type::LibUSB)->removeHotplugCallback();

  for (auto& device : m_collDevices)
  {
    device.second->onDisconnect();
  }

  stop();
}

//--------------------------------------------------------------------------------------------------

void Coordinator::run()
{
  M_LOG("[Coordinator] run");
  bool expected = false;
  if (!m_running.compare_exchange_strong(expected, true))
  {
    M_LOG("[Coordinator] already running");
    return;
  }

  m_cablThread = std::thread([this]() {
    while (m_running)
    {
      std::lock_guard<std::mutex> lock(m_mtxDevices);
      for (const auto& device : m_collDevices)
      {
        if (device.second)
        {
          device.second->onTick();
        }
      }
      std::this_thread::yield();
    }
  });
}

//--------------------------------------------------------------------------------------------------

void Coordinator::stop()
{
  M_LOG("[Coordinator] stop");
  bool expected = true;
  if (!m_running.compare_exchange_strong(expected, false))
  {
    M_LOG("[Coordinator] already stopped");
    return;
  }

  if (m_cablThread.joinable())
  {
    m_cablThread.join();
  }
}

//--------------------------------------------------------------------------------------------------

Coordinator::tDevicePtr Coordinator::connect(const DeviceDescriptor& deviceDescriptor_)
{
  M_LOG("[Coordinator] connect");

  if (!deviceDescriptor_)
  {
    return nullptr;
  }

#if defined(_WIN32) || defined(__APPLE__) || defined(__linux)
  Driver::Type driverType;
  switch (deviceDescriptor_.type())
  {
    case DeviceDescriptor::Type::HID:
    {
      driverType = Driver::Type::HIDAPI;
      break;
    }
    case DeviceDescriptor::Type::MIDI:
    {
      driverType = Driver::Type::MIDI;
      break;
    }
    case DeviceDescriptor::Type::USB:
    default:
    {
      driverType = Driver::Type::LibUSB;
      break;
    }
  }
#endif
  auto deviceHandle = driver(driverType)->connect(deviceDescriptor_);

  if (deviceHandle)
  {
    auto device = m_collDevices.find(deviceDescriptor_);
    if (device != m_collDevices.end())
    {
      device->second->setDeviceHandle(std::move(deviceHandle));
    }
    else
    {
      auto device = DeviceFactory::instance().device(deviceDescriptor_, std::move(deviceHandle));
      m_collDevices.insert(std::pair<DeviceDescriptor, tDevicePtr>(deviceDescriptor_, device));
    }
    m_collDevices[deviceDescriptor_]->onConnect();
  }

  return m_collDevices[deviceDescriptor_];
}

//--------------------------------------------------------------------------------------------------

void Coordinator::scan()
{
  M_LOG("[Coordinator] scan"); 

  std::lock_guard<std::mutex> lock(m_mtxDeviceDescriptors);
  tCollDeviceDescriptor deviceDescriptors{m_collDeviceDescriptors};
  m_collDeviceDescriptors.clear();

#if defined(_WIN32) || defined(__APPLE__) || defined(__linux)
  for (const auto& deviceDescriptor : driver(Driver::Type::HIDAPI)->enumerate())
  {
    if (checkAndAddDeviceDescriptor(deviceDescriptor))
    {
      M_LOG("[Coordinator] scan: new device found via HIDAPI");
    }
  }

  for (const auto& deviceDescriptor : driver(Driver::Type::MIDI)->enumerate())
  {
    if (checkAndAddDeviceDescriptor(deviceDescriptor))
    {
      M_LOG("[Coordinator] scan: new device found via MIDI");
    }
  }
#endif

  Driver::Type tMainDriver(Driver::Type::LibUSB);

  for (const auto& deviceDescriptor : driver(tMainDriver)->enumerate())
  {
    if (checkAndAddDeviceDescriptor(deviceDescriptor))
    {
      M_LOG("[Coordinator] scan: new device found via main driver");
    }
  }

  std::sort(m_collDeviceDescriptors.begin(), m_collDeviceDescriptors.end());

  {
    std::lock_guard<std::mutex> lock(m_mtxDevices);
    auto it = m_collDevices.begin();
    while (it != m_collDevices.end())
    {
      bool found{false};
      for (const auto& deviceDescriptor : m_collDeviceDescriptors)
      {
        if (deviceDescriptor == it->first)
        {
          found = true;
          break;
        }
      }

      if (!found)
      {
        it->second->onDisconnect();
      }

      it++;
    }
  }

  m_scanDone = true;

  for (const auto& deviceDescriptor : m_collDeviceDescriptors)
  {
    if (!m_pClient->discoveryPolicy().matches(deviceDescriptor))
    {
      continue;
    }
    m_pClient->setDevice(connect(deviceDescriptor));
  }
}

//--------------------------------------------------------------------------------------------------

bool Coordinator::checkAndAddDeviceDescriptor(const sl::cabl::DeviceDescriptor& deviceDescriptor)
{
  if ((!DeviceFactory::instance().isKnownDevice(deviceDescriptor))
      || std::find(m_collDeviceDescriptors.begin(), m_collDeviceDescriptors.end(), deviceDescriptor)
           != m_collDeviceDescriptors.end())
  {
    return false; // unknown
  }
  m_collDeviceDescriptors.push_back(deviceDescriptor);
  return true;
}

//--------------------------------------------------------------------------------------------------

Coordinator::tDriverPtr Coordinator::driver(Driver::Type tDriver_)
{
  if (m_collDrivers.find(tDriver_) == m_collDrivers.end())
  {
    m_collDrivers.emplace(tDriver_, std::make_shared<Driver>(tDriver_));
  }

  return m_collDrivers[tDriver_];
}

//--------------------------------------------------------------------------------------------------

} // namespace cabl
} // namespace sl
