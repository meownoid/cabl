/*
        ##########    Copyright (C) 2015 Vincenzo Pacella
        ##      ##    Distributed under MIT license, see file LICENSE
        ##      ##    or <http://opensource.org/licenses/MIT>
        ##      ##
##########      ############################################################# shaduzlabs.com #####*/

#pragma once

#include <atomic>
#include <map>
#include <thread>

#include "cabl/client/Client.h"
#include "cabl/comm/DeviceDescriptor.h"
#include "cabl/comm/Driver.h"
#include "cabl/devices/Device.h"

//--------------------------------------------------------------------------------------------------

namespace sl
{
namespace cabl
{

//--------------------------------------------------------------------------------------------------

class Coordinator
{
public:
  using tCollDeviceDescriptor = std::vector<DeviceDescriptor>;
  using tDevicePtr = std::shared_ptr<Device>;
  using tCollDevices = std::map<DeviceDescriptor, tDevicePtr>;

  using tDriverPtr = std::shared_ptr<Driver>;
  using tCollDrivers = std::map<Driver::Type, tDriverPtr>;

  Coordinator(Client* client);
  ~Coordinator();

  void scan();
  void run();
  void stop();

private:
  bool checkAndAddDeviceDescriptor(const DeviceDescriptor&);
  tDevicePtr connect(const DeviceDescriptor&);

  tDriverPtr driver(Driver::Type);

  std::thread m_cablThread;

  std::atomic<bool> m_running{false};
  std::atomic<bool> m_scanDone{false};

  std::mutex m_mtxDevices;
  std::mutex m_mtxDeviceDescriptors;

  tCollDrivers m_collDrivers;
  tCollDeviceDescriptor m_collDeviceDescriptors;
  tCollDevices m_collDevices;

  Client* m_pClient{nullptr};
};

//--------------------------------------------------------------------------------------------------

} // namespace cabl
} // namespace sl
