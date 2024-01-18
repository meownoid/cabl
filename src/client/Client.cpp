/*
        ##########    Copyright (C) 2015 Vincenzo Pacella
        ##      ##    Distributed under MIT license, see file LICENSE
        ##      ##    or <http://opensource.org/licenses/MIT>
        ##      ##
##########      ############################################################# shaduzlabs.com #####*/

#include "cabl/client/Client.h"

#include "devices/ableton/Push2.h"
#include "devices/ableton/Push2Display.h"
#include "devices/akai/Push.h"
#include "devices/ni/KompleteKontrol.h"
#include "devices/ni/MaschineJam.h"
#include "devices/ni/MaschineMK1.h"
#include "devices/ni/MaschineMK2.h"
#include "devices/ni/MaschineMikroMK1.h"
#include "devices/ni/MaschineMikroMK2.h"
#include "devices/ni/TraktorF1MK2.h"

namespace sl
{
namespace cabl
{

using namespace std::placeholders;

//--------------------------------------------------------------------------------------------------

void Client::onInitDevice()
{
  if (!m_pDevice)
  {
    return;
  }

  for (size_t i = 0; i < m_pDevice->numOfGraphicDisplays(); i++)
  {
    m_pDevice->graphicDisplay(i)->black();
  }

  for (size_t i = 0; i < m_pDevice->numOfTextDisplays(); i++)
  {
    m_pDevice->textDisplay(i)->clear();
  }

  m_pDevice->setCallbackDisconnect(std::bind(&Client::disconnected, this));
  m_pDevice->setCallbackRender(std::bind(&Client::onRender, this));

  m_pDevice->setCallbackButtonChanged(std::bind(&Client::buttonChanged, this, _1, _2, _3));
  m_pDevice->setCallbackEncoderChanged(std::bind(&Client::encoderChanged, this, _1, _2, _3));
  m_pDevice->setCallbackEncoderChangedRaw(std::bind(&Client::encoderChangedRaw, this, _1, _2, _3));
  m_pDevice->setCallbackKeyChanged(std::bind(&Client::keyChanged, this, _1, _2, _3));
  m_pDevice->setCallbackKeyUpdated(std::bind(&Client::keyUpdated, this, _1, _2, _3));
  m_pDevice->setCallbackControlChanged(std::bind(&Client::controlChanged, this, _1, _2, _3));

  initDevice();

  m_update = true;
}

//--------------------------------------------------------------------------------------------------

void Client::onRender()
{
  bool expected = true;
  if (m_update.compare_exchange_weak(expected, false) && m_pDevice && m_pDevice->hasDeviceHandle())
  {
    render();
  }
  else
  {
    std::this_thread::yield();
  }
}

} // namespace cabl
} // namespace sl
