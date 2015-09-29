#include <event2/listener.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/util.h>
#include <event2/thread.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include "easylogging++.h"
#include "utilities.h"
INITIALIZE_EASYLOGGINGPP

int main(int argc, char *argv[])
{
  START_EASYLOGGINGPP(argc, argv);
  std::string input_line;

  while (std::getline(std::cin,input_line))
  {
    VLOG(2) << "Input: " << input_line;
  }
  return 0;
}