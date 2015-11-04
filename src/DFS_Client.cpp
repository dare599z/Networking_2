#include <event2/listener.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/util.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <functional>

#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <openssl/md5.h>

#include "easylogging++.h"
INITIALIZE_EASYLOGGINGPP
#include "utilities.h"
#include "commands.h"
#include "Client.h"

Command* ParseCommand(const std::string &line)
{
  VLOG(3) << "Parsing " << line;
  std::string command;
  std::istringstream iss(line);

  if ( !(iss >> command) )
  {
    LOG(ERROR) << "Error parsing command";
  }
  else if ( command.compare("LIST") == 0 )
  {
    Command_List *c = new Command_List();
    c->valid = true;
    return c;
  }
  else if ( command.compare("GET") == 0 )
  {
    Command_Get *c = new Command_Get();
    if ( iss >> c->filename )
    {
      c->valid = true;
    }
    return c;
  }
  else if ( command.compare("PUT") == 0 )
  {
    Command_Put *c = new Command_Put();
    if ( iss >> c->filename )
    {
      c->valid = true;
    }
    return c;
  }
  return NULL;
}


int main(const int argc, const char *argv[])
{
  START_EASYLOGGINGPP(argc, argv);
  el::Configurations elconf;
  elconf.setToDefault();
  elconf.setGlobally(el::ConfigurationType::Format, "<%levshort>: %msg");
  el::Loggers::reconfigureAllLoggers(elconf);
  elconf.clear();
  el::Loggers::addFlag( el::LoggingFlag::ColoredTerminalOutput );
  el::Loggers::addFlag( el::LoggingFlag::DisableApplicationAbortOnFatalLog );

  Client client;
  std::string input_line;

  std::string confFilePath;
  if ( utils::cmdOptionExists(argv, argv+argc, "-c") ) confFilePath = utils::getCmdOption( argv, argv+argc, "-c" );
  else confFilePath = "conf/dfc.conf";

  if ( !client.ParseConfFile(confFilePath) ) {
    LOG(FATAL) << "Errors while parsing the configuration file... Exiting";
    return -1;
  }

  client.Initialize();

  while (std::getline(std::cin,input_line))
  {
    VLOG(2) << "Input: " << input_line;
    Command *pc = ParseCommand(input_line);
    if ( !pc )
    {
      LOG(ERROR) << "Invalid command.";
      continue;
    }
    switch (pc->Type())
    {
      case Command::Type::Get:
        client.Get(reinterpret_cast<Command_Get*>(pc));
        break;
      case Command::Type::Put:
        client.Put(reinterpret_cast<Command_Put*>(pc));
        break;
      case Command::Type::List:
        client.List(reinterpret_cast<Command_List*>(pc));
        break;
      default:
        LOG(ERROR) << "Unrecognized command.";
        break;
    }
  }
  return 0;
}
