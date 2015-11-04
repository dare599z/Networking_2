#ifndef MCBRIDE_INC_CONNECTION_H
#define MCBRIDE_INC_CONNECTION_H

#include <event2/listener.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/util.h>

#include <fcntl.h>
#include <arpa/inet.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <map>

#include "utilities.h"
#include "commands.h"


#include "easylogging++.h"

class ConnectionManager;
class Client;

class Connection {
public:
  Connection(int id, const std::string& ip, short port, ConnectionManager* const manager, Client& client);
  ~Connection();

  struct PutInfo {
    int part1_number;
    size_t length1;
    size_t offset1;

    int part2_number;
    size_t length2;
    size_t offset2;

    std::string name;
  };

  bool Initialize();

  bool Connect();
  void Disconnect();

  void Get(std::string file, int part);
  void List();
  void Put(const std::string& filename, const PutInfo& pi );

  static void callback_get(bufferevent *ev, void *caller) {
    reinterpret_cast<Connection*>(caller)->callback_get();
  }
  static void callback_put(bufferevent *ev, void *caller) {
    reinterpret_cast<Connection*>(caller)->callback_put();
  }
  static void callback_list(bufferevent *ev, void *caller) {
    reinterpret_cast<Connection*>(caller)->callback_list();
  }
  static void callback_event(bufferevent *ev, short events, void *caller) {
    reinterpret_cast<Connection*>(caller)->callback_event(ev, events);
  }
  static void callback_data_written(bufferevent *ev, void *caller) {
    reinterpret_cast<Connection*>(caller)->callback_data_written();
  }
  static void callback_timeout(evutil_socket_t fd, short what, void* caller) {
    reinterpret_cast<Connection*>(caller)->callback_timeout();
  }

  int ID() const {return m_id;}
  short Port() const {return m_port;}
  std::string IP() const {return m_IP;}


private:
  int m_id;
  std::string m_IP;
  short m_port;

  sockaddr_in m_socket;
  bool m_initialized;
  bool m_connected;
  PutInfo m_putinfo;

  std::string m_requested_get;
  int m_requested_part;

  int m_get_state;

  event_base *m_base;
  bufferevent *m_bev;
  evbuffer *m_input;
  evbuffer *m_output;

  ConnectionManager* const m_manager;
  Client& m_client;

  void callback_get();
  void callback_put();
  void callback_list();

  void callback_event(bufferevent* ev, short events);
  void callback_data_written();
  void callback_timeout();
};

class ConnectionManager {
public:
  typedef std::pair<int, Connection*> ConnectionPair;
  ConnectionManager();
  ~ConnectionManager();

  Connection* Get(int id);

  void SetUser(const std::string&);
  void SetPassword(const std::string&);

  std::string AuthLine() const;

  Connection* CreateNewConnection(int id, const std::string& ip, int port, Client& client);

  bool ConnectAll() const;

private:
  std::map<int, Connection*> m_connections;
  event_base *m_base;
  std::string m_auth_user;
  std::string m_auth_password;
};

#endif //MCBRIDE_INC_CONNECTION_H
