#include <event2/listener.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/util.h>
#include <event2/thread.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include "utilities.h"
#include "easylogging++.h"
INITIALIZE_EASYLOGGINGPP


int main(int argc, char[]* argv)
{
  START_EASYLOGGINGPP(argc, argv);
  return 0;
}