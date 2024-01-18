/*
        ##########    Copyright (C) 2015 Vincenzo Pacella
        ##      ##    Distributed under MIT license, see file LICENSE
        ##      ##    or <http://opensource.org/licenses/MIT>
        ##      ##
##########      ############################################################# shaduzlabs.com #####*/

#include <cstdio>
#include <thread>

#include <cabl/cabl.h>
#include <cabl/devices/Coordinator.h>

#include "Euklid.h"


using namespace sl;
using namespace sl::cabl;

//--------------------------------------------------------------------------------------------------

int main(int argc, const char* argv[])
{
  Euklid euklid;
  Coordinator coordinator(&euklid);
  coordinator.scan();
  coordinator.run();

  std::cout << "Type 'q' and hit ENTER to quit." << std::endl;

  while (std::cin.get() != 'q')
  {
    std::this_thread::yield();
  }

  return 0;
}

//--------------------------------------------------------------------------------------------------
