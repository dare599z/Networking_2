#include <event2/listener.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/util.h>
#include <event2/thread.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include "easyloggingpp.h"
#include "utilities.h"


class Server {

};


int main(int argc, char[]* argv)
{
  int port = -1;
  std::istringstream(std::string(argv[1])) >> port;

  VLOG(3) << "Port: " << port;

  return 0;
}