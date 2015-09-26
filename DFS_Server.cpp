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

#include "easylogging++.h"
INITIALIZE_EASYLOGGINGPP
#include "utilities.h"

typedef std::map<std::string, std::string> user_map;

struct Server {
  int port;
  std::string folder;
  user_map users;
};

struct Connection {
  int port;
  bufferevent *bev;

  friend std::ostream& operator<<(std::ostream& os, const Connection& ci);
};

std::ostream& operator<<(std::ostream& os, const Connection& ci)
{
  os << "[" << ci.port << "]: ";
  return os;
}

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
  // event_del( ci->timeout_event );
  free(ci);
}

void
callback_data_written(bufferevent *bev, void *conn_info)
{
  Connection *ci = reinterpret_cast<Connection*>(conn_info);
  VLOG(1) << ci << "Closing (WRITEOUT)";
  close_connection(ci);
}

void callback_event(bufferevent *event, short events, void *conn_info)
{
  Connection* ci = reinterpret_cast<Connection*>(conn_info);

  if ( (events & (BEV_EVENT_READING|BEV_EVENT_EOF)) )
  {
    VLOG(1) << ci << "Closing (CLIENT EOF)";
    close_connection(ci);
    return;
  }
}

void callback_read(bufferevent *bev, void *ci)
{

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

  // bufferevent_setcb(bev, callback_read, NULL, callback_event, (void*)ci);
  bufferevent_setwatermark(bev, EV_WRITE, 0, 0);
  
  VLOG(1) << ci << "Opened";
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