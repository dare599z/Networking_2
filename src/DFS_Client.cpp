#include <event2/listener.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/util.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include <arpa/inet.h>

#include "easylogging++.h"
INITIALIZE_EASYLOGGINGPP
#include "utilities.h"
#include "commands.h"

#include "Server.h"


void callback_read(bufferevent *ev, void *conn_info)
{

}

class Connections {
private:
  std::vector<Server*> connections;
  event_base *m_base;

public:
  Connections() :
    connections(),
    m_base(NULL)
  {
    m_base = event_base_new();
  }
  ~Connections()
  {
    for (auto it = connections.begin(); it != connections.end(); ++it)
    {
      delete *it;
    }
  }

  event_base* GetBase()
  {
    return m_base;
  }

  Server* Get(int id)
  {
    for (auto it = connections.begin(); it != connections.end(); ++it)
    {
      if ((*it)->ID() == id) return *it;
    }
    return NULL;
  }

  void Add(Server *s)
  {
    connections.push_back(s);
    s->SetBase(m_base);
  }

  bool ConnectAll()
  {
    VLOG(9) << __PRETTY_FUNCTION__;
    bool b = true;
    for (auto it = connections.begin(); it != connections.end(); ++it)
    {
      if ( !( (*it)->Connect() ) ) b = false;
    }
    return b;
  }

  bool AuthenticateAll(std::string& user, std::string& password)
  {
    VLOG(9) << __PRETTY_FUNCTION__;
    bool b = true;
    for (auto it = connections.begin(); it != connections.end(); ++it)
    {
      if ( !( (*it)->Authenticate(user, password) ) ) b = false;
    }
    return b;
  }
};

struct Configuration {
  bool ParseFile(const std::string& filename, Connections &conns)
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

        std::string ip;
        if ( !(ss >> ip) )
        {
          LOG(ERROR) << "Conf parsing error: server ip. Line " << lineCount;
          return false;
        }

        short port;
        if ( !(ss >> port) )
        {
          LOG(ERROR) << "Conf parsing error: server port. Line " << lineCount;
          return false;
        }
        Server *s = new Server(foundServers+1, ip, port);
        conns.Add(s);
        LOG(INFO) << "Using server " << s->IP() << " [" << s->Port() << "]";

        foundServers++;
        
      }
      else if ( directive.compare("Username") == 0 )
      {
        std::string username;
        if ( !(ss >> username) )
        {
          LOG(ERROR) << "Conf parsing error: username. Line " << lineCount;
          return false;
        }
        VLOG(1) << "Using user: " << username;
        this->user = username;
      }
      else if ( directive.compare("Password") == 0 )
      {
        std::string pass;
        if ( !(ss >> pass) )
        {
          LOG(ERROR) << "Conf parsing error: password. Line " << lineCount;
          return false;
        }
        VLOG(1) << "Using user: " << pass;
        this->password = pass;
      }
      else
      {
        LOG(WARNING) << "Unknown conf option: " << line;
      }
    }

    file.close();

    if (foundServers == 4)
    {
      for (int i = 1; i <= 4; i++)
      {
        Server *s = conns.Get(i);
        if ( !(s->Initialize()) ) return false;
      }
      return true;
    }
    else {
      LOG(FATAL) << "Only found " << foundServers << " servers.";
      return false;
    }
  }

  std::string password;
  std::string user;

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

void DoGet(Command_Get *c, Connections& conns)
{
  VLOG(2) << __PRETTY_FUNCTION__;
  Server *s1 = conns.Get(1);
  Server *s2 = conns.Get(2);
  Server *s3 = conns.Get(3);
  Server *s4 = conns.Get(4);
  // s1->Command(conf.user, conf.password);
  

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
  el::Configurations elconf;
  elconf.setToDefault();
  elconf.setGlobally(el::ConfigurationType::Format, "<%levshort>: %msg");
  el::Loggers::reconfigureAllLoggers(elconf);
  elconf.clear();
  el::Loggers::addFlag( el::LoggingFlag::ColoredTerminalOutput );
  el::Loggers::addFlag( el::LoggingFlag::DisableApplicationAbortOnFatalLog );


  Connections conns;
  std::string input_line;

  std::string confFilePath;
  if ( utils::cmdOptionExists(argv, argv+argc, "-c") ) confFilePath = utils::getCmdOption( argv, argv+argc, "-c" );
  else confFilePath = "../conf/dfc.conf";

  if ( !conf.ParseFile(confFilePath, conns) ) {
    LOG(FATAL) << "Errors while parsing the configuration file... Exiting";
    return -1;
  }
  if ( !conns.ConnectAll() )
  {
    LOG(ERROR) << "Error connecting servers.";
  }
  if ( !conns.AuthenticateAll(conf.user, conf.password) )
  {
    LOG(ERROR) << "Error authenticating servers.";
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
        DoGet(reinterpret_cast<Command_Get*>(pc), conns);
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
