#include <event2/listener.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/util.h>
#include <event2/thread.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <dirent.h>

#include "easylogging++.h"
INITIALIZE_EASYLOGGINGPP

#include "Server.h"
#include "utilities.h"
#include "commands.h"

// void
// close_connection(Connection* ci)
// {
//   VLOG(2) << __PRETTY_FUNCTION__;
//   bufferevent_free(ci->bev);
//   free(ci);
// }

void
PrintUsage() 
{
  LOG(WARNING) << "--> USAGE <--";
  LOG(WARNING) << "dfs folder port";
  LOG(WARNING) << "\tfolder = subdirectory of current to use for this server instance";
  LOG(WARNING) << "\tport = port to use for this server instance";
}

int
main(int argc, char* argv[])
{
  START_EASYLOGGINGPP(argc, argv);
  el::Configurations conf;
  el::Loggers::addFlag( el::LoggingFlag::ColoredTerminalOutput );
  el::Loggers::addFlag( el::LoggingFlag::DisableApplicationAbortOnFatalLog );

  if (argc < 3)
  {
    PrintUsage();
    return -1;
  }

  // Parse out the subdirectory to use
  std::string folder = std::string(argv[1]) + "/";
  if ( !utils::DirectoryExists(folder) )
  {
    LOG(FATAL) << "Folder (" << folder << ") is not valid.";
    PrintUsage();
    return -1;
  }
  VLOG(3) << "Folder: " << folder;

  // Parse out the port number to use
  int port = -1;
  std::istringstream(std::string(argv[2])) >> port;
  if (port == -1)
  {
    LOG(FATAL) << "Could not get port number";
    PrintUsage();
    return -1;
  }
  VLOG(3) << "Port: " << port;

  conf.setGlobally(el::ConfigurationType::Format, "<%levshort>:[" + std::to_string(port) + "] %msg");
  el::Loggers::reconfigureAllLoggers(conf);
  conf.clear();

  ServerInstance server(folder, port);

  bool initGood = server.Initialize();
  if ( !initGood ) return -1;

  
  server.Start();

  return 0;
}