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
  m_base = event_base_new();
}

Server::~Server()
{
  bufferevent_free(m_bev);
  event_base_free(m_base);
}

bool
Server::Initialize()
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

void
Server::Authenticated(bool b)
{
  m_authenticated = b;
}

bool
Server::Connect()
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

void
Server::Disconnect()
{
  VLOG(9) << __PRETTY_FUNCTION__;
  bufferevent_free(m_bev);
}

bool
Server::Authenticate(const std::string& user, const std::string pass)
{
  VLOG(9) << __PRETTY_FUNCTION__;
  std::string command = "AUTH " + user + " " + pass + "\n";
  bufferevent_write(m_bev, command.c_str(), command.length());
  bufferevent_setcb(m_bev, callback_auth, NULL, callback_event, this);
  event_base_loop(m_base, EVLOOP_NONBLOCK);
  return true;
}

void
Server::Get(std::string file)
{
  VLOG(9) << __PRETTY_FUNCTION__;
  std::string command = "GET " + file + "\n";
  bufferevent_write(m_bev, command.c_str(), command.length());
  event_base_loop(m_base, EVLOOP_NONBLOCK);
}

void
Server::Put(const std::string& filename, const PutInfo& pi )
{
  VLOG(9) << __PRETTY_FUNCTION__;
  evbuffer *output = bufferevent_get_output(m_bev);
  std::string command = "PUT " +  filename + "\n";
  evbuffer_add(output, command.c_str(), command.length());
  m_putinfo = pi;

  bufferevent_setcb(m_bev, callback_put, NULL, callback_event, this);
  event_base_dispatch(m_base);
}

void
Server::callback_auth(bufferevent* bev)
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

void
Server::callback_data_written(bufferevent *bev)
{
  VLOG(1) << "(WRITEOUT)";
  // NormalizeCallbacks(bev, conn_info);
  event_base_loopexit( m_base, 0 );
}

void
Server::callback_timeout()
{
  LOG(WARNING) << "timeout...";
  evbuffer *output = bufferevent_get_output(m_bev);
  size_t available = evbuffer_get_length(output);
  LOG(WARNING) << "available to write: " << available;
  // char *data = new char[available]; 
  // size_t copied = evbuffer_remove(output, data, available);
  // for (int i = 0; i < copied; i++) std::cout << data[i];

}

void
Server::callback_put(bufferevent *bev)
{
  VLOG(9) << __PRETTY_FUNCTION__;
  evbuffer *input = bufferevent_get_input(bev);
  evbuffer *output = bufferevent_get_output(bev);

  VLOG(8) << "Reading from buffer";  
  size_t bytes_read = 0;
  char *l = evbuffer_readln(input, &bytes_read, EVBUFFER_EOL_CRLF);
  if ( !l )
  {
    LOG(ERROR) << "Error reading line";
    return;
  }
  std::string rv(l);

  if ( rv.compare(Command_Put::PUT_READY()) != 0 )
  {
    LOG(ERROR) << "Put was started, but server didn't respond properly.";
    LOG(ERROR) << "Response: " << rv;
    bufferevent_setcb(bev, NULL, NULL, callback_event, this);
    return;
  }

  VLOG(8) << "Sending data";

  std::string command;
  command = std::to_string(m_putinfo.part1_number) + " " + std::to_string(m_putinfo.length1) + "\n";
  evbuffer_add(output, command.c_str(), command.length());

  int file = open(m_putinfo.name.c_str(), O_RDONLY);
  int r = evbuffer_add_file(output, file, m_putinfo.offset1, m_putinfo.length1);
  VLOG(7) << "Write of first part: " << r;
  command = std::to_string(m_putinfo.part2_number) + " " + std::to_string(m_putinfo.length2) + "\n";
  evbuffer_add(output, command.c_str(), command.length());

  int file2 = open(m_putinfo.name.c_str(), O_RDONLY);
  r = evbuffer_add_file(output, file2, m_putinfo.offset2, m_putinfo.length2);
  VLOG(7) << "Write of second part: " << r;
  
  // const char* newLine = "\n";
  // evbuffer_add(output, newLine, strlen(newLine));
  bufferevent_setcb(m_bev, callback_data_written, NULL, callback_event, this);
  VLOG(8) << "Done sending data";

  timeval to = {3, 0}; 
  event *e = event_new(m_base, -1, EV_TIMEOUT, callback_timeout, this);
  event_add(e, &to);
}

void
Server::callback_event(bufferevent *event, short events)
{
  if ( (events & (BEV_EVENT_READING|BEV_EVENT_EOF)) )
  {
    VLOG(1) << "Closing (CLIENT EOF)";
    Disconnect();
    return;
  }
  if ( events & BEV_EVENT_ERROR )
  {
    LOG(ERROR) << "Event error.." << evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR());
  }
}
