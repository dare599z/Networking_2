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

struct file_part
{
  std::string file;
  int part;
};


std::vector<file_part> file_parts(const std::string file)
{
  std::vector<file_part> files;
  for (int i = 1; i <= 4; i++)
  {
    std::string s = file + "." + std::to_string(i);
    if ( utils::file_exists(s) )
    {
      VLOG(2) << "Found file: " << s;
      file_part fp;
      fp.file = s;
      fp.part = i;
      files.push_back(fp);
    }
    else
    {
      VLOG(2) << "Not found: " << s;
    }
  }
  return files;
}


// void
// close_connection(Connection* ci)
// {
//   VLOG(2) << __PRETTY_FUNCTION__;
//   bufferevent_free(ci->bev);
//   free(ci);
// }


void DoGet(Command_Get *c)
{
  VLOG(2) << __PRETTY_FUNCTION__;
  std::vector<file_part> files = file_parts(c->info);
  for (auto it = files.begin(); it != files.end(); it++)
  {

  }
}

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