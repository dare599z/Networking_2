#include "Server.h"

Server::Server(int id, const std::string& ip, short port) :
  m_id(id),
  m_IP(ip),
  m_port(port),
  m_initialized(false),
  m_authenticated(false)
{
  memset(&m_socket, 0, sizeof m_socket);
  m_socket.sin_family = AF_INET;
  if ( inet_pton(AF_INET, m_IP.c_str(), &(m_socket.sin_addr)) != 1 )
  {
    LOG(FATAL) << "IP address from string.";
  }
  m_socket.sin_port = htons(m_port);
}

Server::~Server()
{
  bufferevent_free(m_bev);
}

bool Server::Initialize()
{
  VLOG(9) << __PRETTY_FUNCTION__;
  if (!m_initialized)
  {
    if (!m_base)
    {
      LOG(ERROR) << "Event base not set.";
      return false;
    } 
    m_bev = bufferevent_socket_new(m_base, -1, BEV_OPT_CLOSE_ON_FREE|BEV_OPT_DEFER_CALLBACKS);
    if (!m_bev) {
      LOG(FATAL) << "Creating bufferevent";
      return false;
    } 
    bufferevent_setcb(m_bev, NULL, NULL, callback_event, this);

    m_initialized = true;
  }
  return true;
}

void Server::Authenticated(bool b)
{
  m_authenticated = b;
}

void Server::SetBase(event_base *eb)
{
  m_base = eb;
}

bool Server::Connect()
{
  VLOG(9) << __PRETTY_FUNCTION__;
  if (!m_initialized) {
    if (!Initialize()) return false;
  }
  if (bufferevent_socket_connect(m_bev, (sockaddr*)&m_socket, sizeof m_socket) < 0) {
    return false;
  }
  bufferevent_setwatermark(m_bev, EV_WRITE, 0, 0);
  bufferevent_enable(m_bev, EV_READ|EV_WRITE);
  return true;
}

void Server::Disconnect()
{
  VLOG(9) << __PRETTY_FUNCTION__;
}

bool Server::Authenticate(const std::string& user, const std::string pass)
{
  VLOG(9) << __PRETTY_FUNCTION__;
  std::string command = "AUTH " + user + " " + pass + "\n";
  bufferevent_write(m_bev, command.c_str(), command.length());
  bufferevent_setcb(m_bev, callback_auth, NULL, callback_event, this);
  event_base_loop(m_base, EVLOOP_NONBLOCK);
  // 
  return true;
}

void Server::Command()
{
  VLOG(9) << __PRETTY_FUNCTION__;
  
  event_base_loop(m_base, EVLOOP_NONBLOCK);
}

void Server::callback_auth(bufferevent* bev)
{
  VLOG(9) << __PRETTY_FUNCTION__;
  evbuffer *input = bufferevent_get_input(bev);
  size_t bytes_read = 0;
  char *l = evbuffer_readln(input, &bytes_read, EVBUFFER_EOL_CRLF);

  std::string rv;
  std::istringstream iss(l);
  iss >> rv;
  if ( rv.compare("AUTHOK") == 0 ) Authenticated(true);
  else Authenticated(false);
}

void Server::callback_event(bufferevent *event, short events)
{

}
