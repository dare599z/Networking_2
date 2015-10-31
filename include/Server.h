#ifndef MCBRIDE_INC_SERVER_H
#define MCBRIDE_INC_SERVER_H

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

#include "utilities.h"
#include "commands.h"


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

  static void callback_auth(bufferevent *ev, void *caller) {
    reinterpret_cast<Server*>(caller)->callback_auth(ev);
  }

  static void callback_put(bufferevent *ev, void *caller) {
    reinterpret_cast<Server*>(caller)->callback_put(ev);
  }

  static void callback_event(bufferevent *ev, short events, void *caller) {
    reinterpret_cast<Server*>(caller)->callback_event(ev, events);
  }

  static void callback_data_written(bufferevent *ev, void *caller) {
    reinterpret_cast<Server*>(caller)->callback_data_written(ev);
  }

  static void callback_timeout(evutil_socket_t fd, short what, void* caller) {
    reinterpret_cast<Server*>(caller)->callback_timeout();
  }

  void callback_timeout();
  void callback_auth(bufferevent* ev);
  void callback_put(bufferevent* ev);
  void callback_event(bufferevent* ev, short events);
  void callback_data_written(bufferevent *bev);

  bool Initialize();
  
  void Authenticated(bool b);
  bool Authenticate(const std::string& user, const std::string pass);

  bool Connect();
  void Disconnect();

  void Command();
  void Get(std::string);
  void Put(const std::string& filename, const PutInfo& pi );

};

#endif //MCBRIDE_INC_SERVER_H
