/*
        ##########    Copyright (C) 2015 Vincenzo Pacella
        ##      ##    Distributed under MIT license, see file LICENSE
        ##      ##    or <http://opensource.org/licenses/MIT>
        ##      ##
##########      ############################################################# shaduzlabs.com #####*/

#include "DriverLibUSB.h"

#include <thread>

#include "DeviceHandleLibUSB.h"

namespace
{

//--------------------------------------------------------------------------------------------------

std::string stringDescriptor(libusb_device_handle* pHandle_, uint8_t uDescriptor)
{
  if (uDescriptor != 0)
  {
    char sDescriptor[256];

    if (libusb_get_string_descriptor_ascii(
          pHandle_, uDescriptor, (unsigned char*)sDescriptor, sizeof(sDescriptor))
        > 0)
    {
      return sDescriptor;
    }
  }
  return std::string();
}

//--------------------------------------------------------------------------------------------------

int cbHotplug(
  libusb_context* /*ctx_*/, libusb_device* device_, libusb_hotplug_event event_, void* userData_)
{
  static libusb_device_handle* pHandle = nullptr;
  struct libusb_device_descriptor descriptor;

  libusb_get_device_descriptor(device_, &descriptor);
  sl::cabl::DriverLibUSB* pDriver = static_cast<sl::cabl::DriverLibUSB*>(userData_);

  if (LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED == event_)
  {
    if (libusb_open(device_, &pHandle) < 0)
    {
      return -1;
    }
    std::string strSerialNum = ::stringDescriptor(pHandle, descriptor.iSerialNumber);
    std::string strProd = ::stringDescriptor(pHandle, descriptor.iProduct);

    libusb_close(pHandle);
    sl::cabl::DeviceDescriptor deviceDescriptor(strProd,
      sl::cabl::DeviceDescriptor::Type::USB,
      descriptor.idVendor,
      descriptor.idProduct,
      strSerialNum);

    if (pDriver != nullptr)
    {
      pDriver->hotplug(deviceDescriptor, true);
    }
  }
  else if (LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT == event_)
  {
    // Device name and serial number are unknown because stringDescriptor cannot be used (pHandle is
    // null)
    sl::cabl::DeviceDescriptor deviceDescriptor(
      "", sl::cabl::DeviceDescriptor::Type::USB, descriptor.idVendor, descriptor.idProduct, "");

    if (pDriver != nullptr)
    {
      pDriver->hotplug(deviceDescriptor, false);
    }
  }
  else
  {
    // unhandled event
  }
  return 0;
}

//--------------------------------------------------------------------------------------------------

} // namespace

//--------------------------------------------------------------------------------------------------

namespace sl
{
namespace cabl
{

//--------------------------------------------------------------------------------------------------

DriverLibUSB::DriverLibUSB() : m_usbThreadRunning(true)
{
  M_LOG("[DriverLibUSB] constructor");

  libusb_init(&m_pContext);
#if !defined(NDEBUG)
  libusb_set_option(m_pContext, LIBUSB_OPTION_LOG_LEVEL, 1);
#endif

  libusb_hotplug_register_callback(m_pContext,
    static_cast<libusb_hotplug_event>(
      LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT),
    static_cast<libusb_hotplug_flag>(0),
    LIBUSB_HOTPLUG_MATCH_ANY,
    LIBUSB_HOTPLUG_MATCH_ANY,
    LIBUSB_HOTPLUG_MATCH_ANY,
    (libusb_hotplug_callback_fn)::cbHotplug,
    this,
    m_pHotplugHandle);

  m_usbThread = std::thread([this]() {
    struct timeval tv = {0, 100000};
    int completed = 0;
    while (m_usbThreadRunning)
    {
      libusb_handle_events_timeout_completed(m_pContext, &tv, &completed);
    }
  });
}

//--------------------------------------------------------------------------------------------------

DriverLibUSB::~DriverLibUSB()
{
  M_LOG("[DriverLibUSB] destructor");
  m_usbThreadRunning = false;

  if (m_usbThread.joinable())
  {
    m_usbThread.join();
  }
  libusb_exit(m_pContext);
}

//--------------------------------------------------------------------------------------------------

Driver::tCollDeviceDescriptor DriverLibUSB::enumerate()
{
  M_LOG("[DriverLibUSB] enumerate");
  Driver::tCollDeviceDescriptor collDeviceDescriptor;

  libusb_device** devices;
  ssize_t nDevices = libusb_get_device_list(m_pContext, &devices);

  libusb_device_handle* pHandle = nullptr;
  for (int i = 0; i < nDevices; ++i)
  {
    libusb_device* device = devices[i];
    if (libusb_open(device, &pHandle) < 0)
    {
      continue;
    }
    struct libusb_device_descriptor descriptor;
    libusb_get_device_descriptor(device, &descriptor);
    std::string strSerialNum = ::stringDescriptor(pHandle, descriptor.iSerialNumber);

    std::string strProd = ::stringDescriptor(pHandle, descriptor.iProduct);
#ifndef NDEBUG
    std::string strManuf = ::stringDescriptor(pHandle, descriptor.iManufacturer);

    if (strSerialNum.empty())
    {
      M_LOG("[DriverLibUSB] enumerate: " << strProd << "(" << strManuf << ")");
    }
    else
    {
      M_LOG("[DriverLibUSB] enumerate: " << strProd << "(" << strManuf << ") with S/N \"" << strSerialNum
                                         << "\"");
    }
#endif
    libusb_close(pHandle);
    DeviceDescriptor deviceDescriptor(
      strProd, DeviceDescriptor::Type::USB, descriptor.idVendor, descriptor.idProduct, strSerialNum);
    collDeviceDescriptor.push_back(deviceDescriptor);
  }

  libusb_free_device_list(devices, 1);

  return collDeviceDescriptor;
}

//--------------------------------------------------------------------------------------------------

tPtr<DeviceHandleImpl> DriverLibUSB::connect(const DeviceDescriptor& device_)
{
  M_LOG("[DriverLibUSB] connecting to " << device_.vendorId() << ":" << device_.productId() << ":"
                                        << device_.serialNumber());
  bool bConnected = false;
  libusb_device** devices;
  ssize_t nDevices = libusb_get_device_list(m_pContext, &devices);

  libusb_device_handle* pCurrentDevice = nullptr;
  for (int i = 0; i < nDevices; ++i)
  {
    libusb_device* device = devices[i];
    struct libusb_device_descriptor descriptor;
    libusb_get_device_descriptor(device, &descriptor);
    if (descriptor.idVendor != device_.vendorId() || descriptor.idProduct != device_.productId())
    {
      continue;
    }

    int e = libusb_open(device, &pCurrentDevice);
    if (e != 0)
    {
      continue;
    }

    std::string strSerialNum = ::stringDescriptor(pCurrentDevice, descriptor.iSerialNumber);
    if (strSerialNum == device_.serialNumber())
    {
      bConnected = true;
      break;
    }
    else
    {
      libusb_close(pCurrentDevice);
    }
  }

  libusb_free_device_list(devices, 1);

  libusb_set_configuration(pCurrentDevice, 1);

  int e = libusb_set_auto_detach_kernel_driver(pCurrentDevice, 1);
  if (e == LIBUSB_ERROR_NOT_SUPPORTED)
  {
    M_LOG("[DriverLibUSB] note: automatic kernel driver detaching is not supported on this platform");
  }

  libusb_claim_interface(pCurrentDevice, 0);

  libusb_set_interface_alt_setting(pCurrentDevice, 0, 1);

  if (pCurrentDevice == nullptr || !bConnected)
    return nullptr;

  M_LOG("[DriverLibUSB] connected to " << device_.vendorId() << ":" << device_.productId() << ":"
                                       << device_.serialNumber());

  return tPtr<DeviceHandleImpl>(new DeviceHandleLibUSB(pCurrentDevice));
}

//--------------------------------------------------------------------------------------------------

void DriverLibUSB::setHotplugCallback(Driver::tCbHotplug cbHotplug_)
{
  m_cbHotplug = cbHotplug_;
}

//--------------------------------------------------------------------------------------------------

void DriverLibUSB::removeHotplugCallback()
{
  m_cbHotplug = nullptr;
}

//--------------------------------------------------------------------------------------------------

void DriverLibUSB::hotplug(const DeviceDescriptor& deviceDescriptor_, bool plugged_)
{
  if (m_cbHotplug)
  {
    m_cbHotplug(deviceDescriptor_, plugged_);
  }
}

//--------------------------------------------------------------------------------------------------

} // namespace cabl
} // namespace sl
