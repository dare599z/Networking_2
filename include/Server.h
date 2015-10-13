#ifndef MCBRIDE_INC_SERVER_H
#define MCBRIDE_INC_SERVER_H

#include <event2/listener.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/util.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include "utilities.h"
#include "commands.h"
#include <arpa/inet.h>

#include "easylogging++.h"

class Server {
  int m_id;
  std::string m_IP;
  short m_port;

  sockaddr_in m_socket;
  bool m_initialized;
  bool m_authenticated;

  event_base *m_base;
  bufferevent *m_bev;

public:
  Server(int id, const std::string& ip, short port);
  ~Server();

  int ID() const {return m_id;}
  short Port() const {return m_port;}
  std::string IP() const {return m_IP;}

  static void callback_auth(bufferevent *ev, void *conn_info) {
    reinterpret_cast<Server*>(conn_info)->callback_auth(ev);
  }

  static void callback_event(bufferevent *ev, short events, void *conn_info) {
    reinterpret_cast<Server*>(conn_info)->callback_event(ev, events);
  }

  void callback_auth(bufferevent* ev);
  void callback_event(bufferevent* ev, short events);

  bool Initialize();
  void SetBase(event_base *eb);
  
  void Authenticated(bool b);
  bool Authenticate(const std::string& user, const std::string pass);

  bool Connect();
  void Disconnect();

  void Command();
};

#endif //MCBRIDE_INC_SERVER_H
