#include "Connection.h"

Connection::Connection(int id, const std::string& ip, short port, ConnectionManager* const manager) :
  m_id( id ),
  m_IP( ip ),
  m_port( port ),
  m_initialized( false ),
  m_manager( manager )
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

Connection::~Connection()
{
  bufferevent_free(m_bev);
  event_base_free(m_base);
}

bool
Connection::Initialize()
{
  VLOG(2) << __PRETTY_FUNCTION__;
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


bool
Connection::Connect()
{
  VLOG(2) << __PRETTY_FUNCTION__;
  if (!m_initialized) {
    if (!Initialize()) return false;
  }
  if (bufferevent_socket_connect(m_bev, (sockaddr*)&m_socket, sizeof m_socket) < 0) {
    LOG(ERROR) << "Couldn't open socket";
    return false;
  }
  bufferevent_setwatermark(m_bev, EV_WRITE, 0, 0);
  bufferevent_enable(m_bev, EV_READ|EV_WRITE);
  m_connected = true;
  return true;
}

void
Connection::Disconnect()
{
  VLOG(2) << __PRETTY_FUNCTION__;
  m_connected = false;
}

// bool
// Connection::Authenticate(const std::string& user, const std::string pass)
// {
//   VLOG(2) << __PRETTY_FUNCTION__;
//   std::string command = "AUTH " + user + " " + pass + "\n";
//   bufferevent_write(m_bev, command.c_str(), command.length());
//   bufferevent_setcb(m_bev, callback_auth, NULL, callback_event, this);
//   event_base_loop(m_base, EVLOOP_NONBLOCK);
//   return true;
// }

void
Connection::Get(std::string file)
{
  VLOG(2) << __PRETTY_FUNCTION__;
  std::string command = "GET " + file + "\n";
  bufferevent_write(m_bev, command.c_str(), command.length());
  event_base_loop(m_base, EVLOOP_NONBLOCK);
}

void
Connection::Put(const std::string& filename, const PutInfo& pi )
{
  VLOG(2) << __PRETTY_FUNCTION__;
  evbuffer *output = bufferevent_get_output(m_bev);
  std::string command;
  
  command = "PUT " +  filename + "\n";
  evbuffer_add(output, command.c_str(), command.length());

  command = m_manager->AuthLine();
  evbuffer_add(output, command.c_str(), command.length());

  m_putinfo = pi;

  bufferevent_setcb(m_bev, callback_put, NULL, callback_event, this);
  int rv = event_base_dispatch(m_base);
  LOG(INFO) << "In put, dispatch rv=" << rv;
}

void
Connection::callback_data_written()
{
  VLOG(2) << "(WRITEOUT)";
  // NormalizeCallbacks(bev, conn_info);
  event_base_loopbreak(m_base);
}

void
Connection::callback_timeout()
{
  LOG(WARNING) << "timeout...";
  evbuffer *output = bufferevent_get_output(m_bev);
  size_t available = evbuffer_get_length(output);
  LOG(WARNING) << "available to write: " << available;
  event_base_loopexit( m_base, 0 );
}

void
Connection::callback_put()
{
  VLOG(2) << __PRETTY_FUNCTION__;
  evbuffer *input = bufferevent_get_input(m_bev);
  evbuffer *output = bufferevent_get_output(m_bev);

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
    LOG(ERROR) << "\tResponse: " << rv;
    bufferevent_setcb(m_bev, NULL, NULL, callback_event, this);
    return;
  }

  bufferevent_setcb(m_bev, NULL, callback_data_written, callback_event, this);
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

  
  VLOG(8) << "Done sending data";

  timeval to = {3, 0}; 
  event *e = event_new(m_base, -1, EV_TIMEOUT, callback_timeout, this);
  // event_add(e, &to);
  // 
  // event_base_dump_events(m_base, stdout);
}

void
Connection::callback_event(bufferevent *event, short events)
{
  if ( (events & (BEV_EVENT_READING|BEV_EVENT_EOF)) )
  {
    VLOG(1) << "Closing (SERVER EOF)";
    Disconnect();
    return;
  }
  if ( events & BEV_EVENT_ERROR )
  {
    LOG(ERROR) << "Event error.." << evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR());
  }
}
/*******************************************************************************

                                          __  .__                                                                   
  ____  ____   ____   ____   ____   _____/  |_|__| ____   ____     _____ _____    ____ _____     ____   ___________ 
_/ ___\/  _ \ /    \ /    \_/ __ \_/ ___\   __\  |/  _ \ /    \   /     \\__  \  /    \\__  \   / ___\_/ __ \_  __ \
\  \__(  <_> )   |  \   |  \  ___/\  \___|  | |  (  <_> )   |  \ |  Y Y  \/ __ \|   |  \/ __ \_/ /_/  >  ___/|  | \/
 \___  >____/|___|  /___|  /\___  >\___  >__| |__|\____/|___|  / |__|_|  (____  /___|  (____  /\___  / \___  >__|   
     \/           \/     \/     \/     \/                    \/        \/     \/     \/     \//_____/      \/       

*******************************************************************************/
ConnectionManager::ConnectionManager() :
  m_connections()
{

}

ConnectionManager::~ConnectionManager()
{
  for (auto it = m_connections.begin(); it != m_connections.end(); ++it)
  {
    delete it->second;
  }
}

Connection*
ConnectionManager::Get(int id)
{
  auto pair = m_connections.find(id);
  if ( pair == m_connections.end() ) return NULL;
  else                               return pair->second;
}

std::string
ConnectionManager::AuthLine() const
{
  return m_auth_user + " " + m_auth_password + "\n";
}

Connection*
ConnectionManager::CreateNewConnection(int id, const std::string& ip, int port)
{
  Connection *newConnection = new Connection(id, ip, port, this);
  m_connections.insert(ConnectionPair(id, newConnection));
  return newConnection;
}

bool
ConnectionManager::ConnectAll() const
{
  VLOG(2) << __PRETTY_FUNCTION__;
  bool b = true;
  for (auto it = m_connections.begin(); it != m_connections.end(); ++it)
  {
    if ( !( ((it->second))->Connect() ) ) b = false;
  }
  return b;
}

void
ConnectionManager::SetUser(const std::string& user)
{
  m_auth_user = user;
}

void
ConnectionManager::SetPassword(const std::string& password)
{
  m_auth_password = password;
}



