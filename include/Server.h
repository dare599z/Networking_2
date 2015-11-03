#ifndef MCBRIDE_INC_SERVER_H
#define MCBRIDE_INC_SERVER_H

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

#include "utilities.h"
#include "commands.h"
#include "easylogging++.h"

#include <map>

typedef std::map<std::string, std::string> user_map;

class ServerInstance {
private:
  event_base *m_base;
  sockaddr_in m_incomingSocket;
  evconnlistener *m_listener;

  std::string m_folder;
  int m_port;
  user_map m_users;

public:
  ServerInstance(std::string folder, int port);

  static Command* ParseCommand(const std::string& line);
  bool Initialize();
  void Start();

  bool IsValidUser(const std::string &user, const std::string &pass);

  std::string UserFolder(const std::string &user) const;
  void CreateUserFolder(const std::string& user);

  void callback_accept_connection(evutil_socket_t newSocket, sockaddr *address, int socklen);
  static void callback_accept_connection(evconnlistener *listener, evutil_socket_t newSocket, sockaddr *address, int socklen, void *caller) {
    reinterpret_cast<ServerInstance*>(caller)->callback_accept_connection(newSocket, address, socklen);
  }

  void callback_accept_error();
  static void callback_accept_error(evconnlistener *listener, void *caller) {
    reinterpret_cast<ServerInstance*>(caller)->callback_accept_error();
  }

};

class ServerConnection {
public:
  ServerConnection(bufferevent*, int, ServerInstance*);

  static void callback_event(bufferevent *bev, short events, void *caller) {
    reinterpret_cast<ServerConnection*>(caller)->callback_event(events);
  }

  static void callback_put(bufferevent *bev, void *caller) {
    reinterpret_cast<ServerConnection*>(caller)->callback_put();
  }

  static void callback_read(bufferevent *bev, void *caller) {
    reinterpret_cast<ServerConnection*>(caller)->callback_read();
  }

private:
  ServerInstance *m_server;
  bufferevent *m_bev;
  evbuffer *m_input;
  evbuffer *m_output;

  static const char* INVALID_CREDENTIALS;

  int m_port;

  struct PutInfo {
    std::string filename;
    int putState;
    size_t wantedSize;
    size_t haveSize;
    int partnum;
  } m_putInfo;

  void ResetCallbacks();
  bool Authenticate(const std::string& line, Command* const c);

  void callback_read();
  void callback_put();
  void callback_event(short events);
};

#endif //MCBRIDE_INC_SERVER_H
