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
#include "commands.h"

/**************
Forward declarations
**************/
void callback_read(bufferevent *bev, void *conn_info);
void callback_event(bufferevent *event, short events, void *conn_info);

const char* INVALID_CREDENTIALS = "Invalid Username/Password. Please try again.\n";

typedef std::map<std::string, std::string> user_map;

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
    return std::string() + this->folder + user + "/";
  }

  Server() :
    port(-1),
    folder(),
    users()
  {
    users["Alice"] = "SimplePassword";
  }
} server;

struct PutInfo {
  size_t putTotalSize;
  size_t putReadSize;
  std::string putDest;
  std::ofstream* fileDest;

  PutInfo(const std::string& dest, size_t size) :
    putDest(dest),
    putTotalSize(size),
    putReadSize(0),
    fileDest(NULL)
  {
    fileDest = new std::ofstream();
    fileDest->open(putDest, std::ios_base::out);
    if (!fileDest)
    {
      LOG(FATAL) << "Error opening file: " << fileDest;
    }

  }
  ~PutInfo()
  {
    delete fileDest;
  }
};

struct Connection {
  int port;
  bufferevent *bev;
  std::string user;
  std::string password;
  std::string userFolder;
  bool userValid;
  PutInfo* putInfo;

  std::string port_s() const {
    return (std::stringstream() << "[" << port << "]: ").str();
  }

  Connection() :
    port(-1),
    bev( NULL ),
    user(),
    password(),
    userValid( false ),
    putInfo( NULL )
    {

    }
};

void PrintUsage() 
{
  LOG(WARNING) << "--> USAGE <--";
  LOG(WARNING) << "dfs folder port";
  LOG(WARNING) << "\tfolder = subdirectory of current to use for this server instance";
  LOG(WARNING) << "\tport = port to use for this server instance";
}

void
close_connection(Connection* ci)
{
  VLOG(9) << __PRETTY_FUNCTION__;
  bufferevent_free(ci->bev);
  free(ci);
}

void
callback_data_written(bufferevent *bev, void *conn_info)
{
  VLOG(9) << __PRETTY_FUNCTION__;
  Connection *ci = reinterpret_cast<Connection*>(conn_info);
  VLOG(1) << ci->port_s() << "Closing (WRITEOUT)";
  close_connection(ci);
}

void callback_event(bufferevent *event, short events, void *conn_info)
{
  VLOG(9) << __PRETTY_FUNCTION__;
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

  if ( command.compare("AUTH") == 0 )
  {
    Command_Auth *c = new Command_Auth();
    if ( iss >> c->user )
    {   
      if ( iss >> c->password )
      {
        c->valid = true;
      }
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
  PutInfo &pi = *(ci->putInfo);
  evbuffer *input = bufferevent_get_input(bev);
  size_t currentLength = evbuffer_get_length(input);
  VLOG(2) << ci->port_s() << "PUT: Bytes ready " << currentLength << " / " << pi.putTotalSize;

  // pi.putReadSize
  char *data = new char[currentLength];
  size_t copied = evbuffer_remove(input, data, currentLength);
  if (copied != currentLength) LOG(WARNING) << ci->port_s() << "Copied fewer bytes than available";
  pi.fileDest->write(data, copied);
  delete data;

  pi.putReadSize += copied;
  if (pi.putReadSize == pi.putTotalSize) 
  {
    LOG(INFO) << ci->port_s() << "Finished reading file.";
    pi.fileDest->close();
    delete ci->putInfo;
    // Set the callbacks back
    bufferevent_setcb(bev, callback_read, NULL, callback_event, (void*)ci);
  }
}

void callback_read(bufferevent *bev, void *conn_info)
{
  VLOG(9) << __PRETTY_FUNCTION__;
  Connection* ci = reinterpret_cast<Connection*>(conn_info);
  evbuffer *input = bufferevent_get_input(bev);
  evbuffer *output = bufferevent_get_output(bev);

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
      case Command::Type::Auth:
      {
        Command_Auth *c = reinterpret_cast<Command_Auth*>(pc);
        if ( !c->valid ) LOG(WARNING) << ci->port_s() << "Invalid AUTH command.";
        else
        {
          LOG(INFO) << ci->port_s() << "User: " << c->user;
          ci->user = c->user;

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
            int err = mkdir(ci->userFolder.c_str(), 0744);
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
          ci->putInfo = new PutInfo(ci->userFolder + c->filename, c->size);
          LOG(INFO) << ci->port_s() << "Entering PUT-- " << ci->putInfo->putDest << " (" << ci->putInfo->putTotalSize << ")";
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


  // Parse out the subdirecory to use
  server.folder = std::string(argv[1]) + "/";
  if ( !utils::DirectoryExists(server.folder) )
  {
    LOG(FATAL) << "Folder (" << server.folder << ") is not valid.";
    PrintUsage();
    return -1;
  }
  VLOG(3) << "Folder: " << server.folder;

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