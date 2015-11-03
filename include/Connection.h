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

class Connection {
  int m_id;
  std::string m_IP;
  short m_port;

  sockaddr_in m_socket;
  bool m_initialized;
  bool m_connected;

  event_base *m_base;
  bufferevent *m_bev;

  ConnectionManager* const m_manager;

public:
  Connection(int id, const std::string& ip, short port, ConnectionManager* const manager);
  ~Connection();

  struct PutInfo {
    int part1_number;
    size_t length1;
    size_t offset1;

    int part2_number;
    size_t length2;
    size_t offset2;

    std::string name;
  } m_putinfo;

  int ID() const {return m_id;}
  short Port() const {return m_port;}
  std::string IP() const {return m_IP;}


  void callback_put();
  static void callback_put(bufferevent *ev, void *caller) {
    reinterpret_cast<Connection*>(caller)->callback_put();
  }

  void callback_event(bufferevent* ev, short events);
  static void callback_event(bufferevent *ev, short events, void *caller) {
    reinterpret_cast<Connection*>(caller)->callback_event(ev, events);
  }

  void callback_data_written();
  static void callback_data_written(bufferevent *ev, void *caller) {
    reinterpret_cast<Connection*>(caller)->callback_data_written();
  }

  void callback_timeout();
  static void callback_timeout(evutil_socket_t fd, short what, void* caller) {
    reinterpret_cast<Connection*>(caller)->callback_timeout();
  }
  bool Initialize();


  bool Connect();
  void Disconnect();

  void Get(std::string);
  void Put(const std::string& filename, const PutInfo& pi );

};

class ConnectionManager {
private:
  std::map<int, Connection*> m_connections;
  event_base *m_base;
  std::string m_auth_user;
  std::string m_auth_password;

public:
  typedef std::pair<int, Connection*> ConnectionPair;
  ConnectionManager();
  ~ConnectionManager();

  Connection* Get(int id);

  void SetUser(const std::string&);
  void SetPassword(const std::string&);

  std::string AuthLine() const;

  Connection* CreateNewConnection(int id, const std::string& ip, int port);

  bool ConnectAll() const;

};

#endif //MCBRIDE_INC_CONNECTION_H
