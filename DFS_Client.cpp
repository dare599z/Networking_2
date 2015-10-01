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
INITIALIZE_EASYLOGGINGPP
#include "utilities.h"
#include "commands.h"

struct Configuration {
  bool ParseFile(const std::string& filename)
  {
    int foundServers = 0;

    std::ifstream file;
    file.open(filename);

    if ( !file.is_open() )
    {
      LOG(FATAL) << "Couldn't open file: " << filename;
      return false;
    }

    std::string line;
    int lineCount = 0;
    while ( getline(file, line) )
    {
      lineCount++;
      std::string directive;
      if ( line.at(0) == '#' ) continue;
      std::istringstream ss(line);
      std::locale x(std::locale::classic(), new utils::colon_seperator);
      ss.imbue(x);

      if ( !(ss >> directive) )
      {
        LOG(ERROR) << "Conf parsing error: read line " << lineCount;
        return false;
      }

      if ( directive.compare("Server") == 0 )
      {
        std::string serverName;
        if ( !(ss >> serverName) )
        {
          LOG(ERROR) << "Conf parsing error: server name. Line " << lineCount;
          return false;
        }

        Server &s = servers[foundServers];

        if ( !(ss >> s.ip) )
        {
          LOG(ERROR) << "Conf parsing error: server ip. Line " << lineCount;
          return false;
        }
        if ( !(ss >> s.port) )
        {
          LOG(ERROR) << "Conf parsing error: server port. Line " << lineCount;
          return false;
        }
        LOG(INFO) << "Using server " << s.ip << " [" << s.port << "]";

        foundServers++;
        
      }
      else if ( directive.compare("Username:") == 0 )
      {
        std::string username;
        if ( !(ss >> username) )
        {
          LOG(ERROR) << "Conf parsing error: username. Line " << lineCount;
          return false;
        }
        this->user = username;
      }
      else if ( directive.compare("Password:") == 0 )
      {
        std::string pass;
        if ( !(ss >> pass) )
        {
          LOG(ERROR) << "Conf parsing error: password. Line " << lineCount;
          return false;
        }
        this->password = pass;
      }
    }

    file.close();
    if (foundServers == 4) return true;
    else {
      LOG(FATAL) << "Only found " << foundServers << " servers.";
      return false;
    }
  }

  std::string password;
  std::string user;

  struct Server {
    std::string ip;
    short port;
  } server1, server2, server3, server4;

  Server servers[4] = {server1,server2,server3,server4};

} conf;

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
    if ( iss >> c->info )
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
      if ( iss >> c->size )
      {
        c->valid = true;
      }
    }
    return c;
  }
  return NULL;
}

void DoGet(Command_Get *c)
{
  VLOG(2) << __PRETTY_FUNCTION__;
}
void DoPut(Command_Put *c)
{
  VLOG(2) << __PRETTY_FUNCTION__;
}
void DoList(Command_List *c)
{
  VLOG(2) << __PRETTY_FUNCTION__;
}

int main(const int argc, const char *argv[])
{
  START_EASYLOGGINGPP(argc, argv);
  std::string input_line;

  std::string confFilePath;
  if ( utils::cmdOptionExists(argv, argv+argc, "-c") ) confFilePath = utils::getCmdOption( argv, argv+argc, "-c" );
  else confFilePath = "./dfc.conf";

  if ( !conf.ParseFile(confFilePath) ) {
    LOG(FATAL) << "Errors while parsing the configuration file... Exiting";
    return -1;
  }

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
        DoGet(reinterpret_cast<Command_Get*>(pc));
        break;
      case Command::Type::Put:
        DoPut(reinterpret_cast<Command_Put*>(pc));
        break;
      case Command::Type::List:
        DoList(reinterpret_cast<Command_List*>(pc));
        break;
      default:
        LOG(ERROR) << "Unrecognized command.";
        break;
    }
  }
  return 0;
}
