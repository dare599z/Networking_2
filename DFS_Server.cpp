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
#include "utilities.h"

const char* INVALID_CREDENTIALS = "Invalid Username/Password. Please try again.\n";

typedef std::map<std::string, std::string> user_map;

struct Command 
{ 
  enum Type {User, Password, Get, Put, List};
  bool valid;
  virtual enum Type Type() const = 0;

  Command() { valid = false; }
};
struct Command_User : public Command
{
  std::string user;
  virtual enum Type Type() const { return Type::User; }
};
struct Command_Password : public Command
{
  std::string password;
  virtual enum Type Type() const { return Type::Password; }
};
struct Command_Get : public Command
{
  std::string info;
  virtual enum Type Type() const { return Type::Get; }
};
struct Command_Put : public Command
{
  std::string filename;
  size_t size;
  virtual enum Type Type() const { return Type::Put; }
};
struct Command_List : public Command
{
  virtual enum Type Type() const { return Type::List; }
};

struct Server {
  int port;
  std::string folder;
  user_map users;

  bool IsValidUser(const std::string &user, const std::string &pass)
  {
    if (users[user].compare(pass) == 0) return true;
    else return false;
  }

  std::string UserFolder(const std::string &user) const
  {
    return std::string() + this->folder + user;
  }

  Server() :
    port(-1),
    folder(),
    users()
  {
    users["admin"] = "admin";
  }
} server;

struct Connection {
  int port;
  bufferevent *bev;
  std::string user;
  std::string password;
  std::string userFolder;

  std::string putDest;
  size_t putSize;


  bool userValid;

  std::string port_s() const {
    return (std::stringstream() << "[" << port << "]: ").str();
  }

  Connection() :
    port(-1),
    bev( NULL ),
    user(),
    password(),
    putDest(),
    putSize(0),
    userValid( false )
    {

    }
};


void PrintUsage() 
{
  LOG(WARNING) << "--> USAGE <--";
  LOG(WARNING) << "dfs port folder";
  LOG(WARNING) << "\tport = port to use for this server instance";
  LOG(WARNING) << "\tfolder = subdirectory of current to use for this server instance";
}

void
close_connection(Connection* ci)
{
  bufferevent_free(ci->bev);
  free(ci);
}

void
callback_data_written(bufferevent *bev, void *conn_info)
{
  Connection *ci = reinterpret_cast<Connection*>(conn_info);
  VLOG(1) << ci->port_s() << "Closing (WRITEOUT)";
  close_connection(ci);
}

void callback_event(bufferevent *event, short events, void *conn_info)
{
  Connection* ci = reinterpret_cast<Connection*>(conn_info);

  if ( (events & (BEV_EVENT_READING|BEV_EVENT_EOF)) )
  {
    VLOG(1) << ci->port_s() << "Closing (CLIENT EOF)";
    close_connection(ci);
    return;
  }
}

Command* ParseCommand(const std::string line, Connection *ci)
{
  VLOG(3) << "Parsing " << line;
  std::string command;
  std::istringstream iss(line);

  if ( !(iss >> command) )
  {
    LOG(ERROR) << "Error parsing command";
  }

  if ( command.compare("USER") == 0 )
  {
    Command_User *c = new Command_User();
    if ( iss >> c->user )
    {
      c->valid = true;
    }
    return c;
  }
  else if ( command.compare("PASS") == 0 )
  {
    Command_Password *c = new Command_Password();
    if ( iss >> c->password )
    {
      c->valid = true;
    }
    return c;
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

void callback_put(bufferevent *bev, void *conn_info)
{
  Connection *ci = reinterpret_cast<Connection*>(conn_info);

}

void callback_read(bufferevent *bev, void *conn_info)
{
  Connection* ci = reinterpret_cast<Connection*>(conn_info);
  evbuffer *input = bufferevent_get_input(bev);
  evbuffer *output = bufferevent_get_output(bev);

  int counter = 1;
  while (true)
  {
    size_t bytes_read = 0;
    char *l = evbuffer_readln(input, &bytes_read, EVBUFFER_EOL_CRLF);
    if (!l) break;
    Command *pc = ParseCommand(l, ci);
    if ( !pc )
    {
      LOG(ERROR) << ci->port_s() << "Error reading last command.";
      continue;
    }
    switch ( pc->Type() ) {
      case Command::Type::User:
      {
        Command_User *c = reinterpret_cast<Command_User*>(pc);
        if ( !c->valid ) LOG(WARNING) << ci->port_s() << "Invalid USER command.";
        else
        {
          LOG(INFO) << ci->port_s() << "User: " << c->user;
          ci->user = c->user;
        }
        break;
      }
      case Command::Type::Password:
      {
        Command_Password *c = reinterpret_cast<Command_Password*>(pc);
        if ( !c->valid ) LOG(WARNING) << ci->port_s() << "Invalid PASS command.";
        else
        {
          LOG(INFO) << ci->port_s() << "Password: " << c->password;
          ci->password = c->password;
          if (!server.IsValidUser(ci->user, ci->password))
          {
            LOG(WARNING) << ci->port_s() << "Invalid credentials: " << ci->user << ", " << ci->password;
            bufferevent_write(bev, INVALID_CREDENTIALS, strlen(INVALID_CREDENTIALS));
            break;
          }
          ci->userValid = true;
          ci->userFolder = server.UserFolder(ci->user);
          if ( !utils::DirectoryExists(ci->userFolder) )
          {
            int err = mkdir(ci->userFolder.c_str(), 0644);
            if (err)
            {
              LOG(FATAL) << ci->port_s() << "Error creating user directory: " << ci->userFolder;
              break;
            }
            LOG(INFO) << ci->port_s() << "Created user directory: " << ci->userFolder;
          }
        }
        break;
      }
      case Command::Type::Get:
      {
        Command_Get *c = reinterpret_cast<Command_Get*>(pc);
        if ( !c->valid ) LOG(WARNING) << ci->port_s() << "Invalid GET command.";
        else LOG(INFO) << "Get: " << ci->user;
        break;
      }
      case Command::Type::List:
      {
        Command_List *c = reinterpret_cast<Command_List*>(pc);
        if ( !c->valid ) LOG(WARNING) << ci->port_s() << "Invalid LIST command.";
        else if ( !ci->userValid )
        {
          LOG(WARNING) << ci->port_s() << "List: " << INVALID_CREDENTIALS;
          bufferevent_write(bev, INVALID_CREDENTIALS, strlen(INVALID_CREDENTIALS));
        }
        else
        {
          std::vector<std::string> files;
          DIR *dirp = opendir(ci->userFolder.c_str());
          struct dirent *dp;
          while ( (dp = readdir(dirp)) != NULL )
          {
            LOG(INFO) << ci->port_s() << "Found file: " << dp->d_name;
            files.push_back(std::string(dp->d_name));
          }
        }
        break;
      }
      case Command::Type::Put:
      {
        Command_Put *c = reinterpret_cast<Command_Put*>(pc);
        if ( !c->valid ) LOG(WARNING) << ci->port_s() << "Invalid PUT command.";
        else if ( !ci->userValid )
        {
          LOG(WARNING) << ci->port_s() << "Put: " << INVALID_CREDENTIALS;
          bufferevent_write(bev, INVALID_CREDENTIALS, strlen(INVALID_CREDENTIALS));
        }
        else
        {
          bufferevent_setcb(bev, callback_put, NULL, callback_event, (void*)ci);
          ci->putDest = c->filename;
          ci->putSize = c->size;
          LOG(INFO) << ci->port_s() << "Entering PUT-- " << ci->putDest << " (" << ci->putSize << ")";
        }
      }
    }
  }
}

void callback_accept_connection(
  evconnlistener *listener,
  evutil_socket_t newSocket,
  sockaddr *address,
  int socklen,
  void *context
  )
{
  event_base *eventBase = evconnlistener_get_base(listener);

  bufferevent *bev = bufferevent_socket_new(eventBase,
                                            newSocket,
                                            BEV_OPT_CLOSE_ON_FREE);

  if (!bev)
  {
    LOG(FATAL) << "Couldn't create bufferevent.. ignoring connection";
    return;
  }

  Connection *ci = new Connection();
  ci->port = newSocket;
  ci->bev = bev;

  bufferevent_setcb(bev, callback_read, NULL, callback_event, (void*)ci);
  bufferevent_setwatermark(bev, EV_WRITE, 0, 0);
  bufferevent_enable(ci->bev, EV_READ|EV_WRITE);
  
  VLOG(1) << ci->port_s() << "Opened";
}

void callback_accept_error(struct evconnlistener *listener, void *ctx)
{
  struct event_base *base = evconnlistener_get_base(listener);
  int err = EVUTIL_SOCKET_ERROR();
  LOG(FATAL) << "Got error <" << err << ": " << evutil_socket_error_to_string(err) << "> on the connection listener. Shutting down.";

  event_base_loopexit(base, NULL);
}

int main(int argc, char* argv[])
{
  START_EASYLOGGINGPP(argc, argv);
  el::Configurations conf;
  conf.setToDefault();
  conf.setGlobally(el::ConfigurationType::Format, "<%levshort>: %msg");
  el::Loggers::reconfigureAllLoggers(conf);
  conf.clear();
  el::Loggers::addFlag( el::LoggingFlag::ColoredTerminalOutput );
  el::Loggers::addFlag( el::LoggingFlag::DisableApplicationAbortOnFatalLog );

  if (argc < 3)
  {
    PrintUsage();
    return -1;
  }

  // Parse out the port number to use
  int port = -1;
  std::istringstream(std::string(argv[1])) >> port;
  if (port == -1)
  {
    LOG(FATAL) << "Could not get port number";
    PrintUsage();
    return -1;
  }
  VLOG(3) << "Port: " << port;

  // Parse out the subdirecory to use
  std::string folder(argv[2]);
  if ( !utils::DirectoryExists(folder) )
  {
    LOG(FATAL) << "Folder (" << folder << ") is not valid.";
    PrintUsage();
    return -1;
  }
  VLOG(3) << "Folder: " << folder;

    event_base *listeningBase = event_base_new();
  if ( !listeningBase )
  {
    LOG(FATAL) << "Error creating an event loop.. Exiting";
    return -2;
  }

  sockaddr_in incomingSocket; 
  memset(&incomingSocket, 0, sizeof incomingSocket);

  incomingSocket.sin_family = AF_INET;
  incomingSocket.sin_addr.s_addr = 0; // local host
  incomingSocket.sin_port = htons(port);

  evconnlistener *listener = evconnlistener_new_bind(
                                     listeningBase,
                                     callback_accept_connection,
                                     NULL,
                                     LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE  ,
                                     -1,
                                     (sockaddr*)&incomingSocket,
                                     sizeof incomingSocket
                                     );

  if ( !listener )
  {
    LOG(FATAL) << "Error creating a TCP socket listener.. Exiting.";
    return -3;
  }

  evconnlistener_set_error_cb(listener, callback_accept_error);
  event_base_dispatch(listeningBase);

  return 0;
}