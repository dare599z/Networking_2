#include "Server.h"

const char* ServerConnection::INVALID_CREDENTIALS = "Invalid Username/Password. Please try again.\n";

ServerInstance::ServerInstance(std::string folder, int port) :
  m_base( NULL ),
  m_listener( NULL ),
  m_incomingSocket(),
  m_port( port ),
  m_folder( folder )
{
  memset(&m_incomingSocket, 0, sizeof m_incomingSocket);
  m_users["Alice"] = "SimplePassword";
}

bool
ServerInstance::Initialize()
{
  m_base = event_base_new();
  if ( !m_base )
  {
    LOG(FATAL) << "Error creating an event loop..";
    return false;
  }

  m_incomingSocket.sin_family = AF_INET;
  m_incomingSocket.sin_addr.s_addr = 0; // local host
  m_incomingSocket.sin_port = htons(m_port);

  m_listener = evconnlistener_new_bind(
                 m_base,
                 callback_accept_connection,
                 this,
                 LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE,
                 -1,
                 (sockaddr*)&m_incomingSocket,
                 sizeof m_incomingSocket
               );

  if ( !m_listener )
  {
    LOG(FATAL) << "Error creating a TCP socket listener.. Exiting.";
    return false;
  }

  evconnlistener_set_error_cb(m_listener, callback_accept_error);

  return true;
}

void
ServerInstance::Start()
{
  event_base_dispatch(m_base);
}

Command*
ServerInstance::ParseCommand(const std::string& line)
{
  VLOG(3) << "Parsing " << line;
  std::string command;
  std::istringstream iss(line);

  if ( !(iss >> command) )
  {
    LOG(ERROR) << "Error parsing command";
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
      c->valid = true;
    }
    return c;
  }
  return NULL;
}

void
ServerInstance::callback_accept_connection(
  evutil_socket_t newSocket,
  sockaddr *remote_address,
  int socklen
  )
{
  bufferevent *bev = bufferevent_socket_new(
    m_base,
    newSocket,
    BEV_OPT_CLOSE_ON_FREE
    );

  if (!bev)
  {
    LOG(FATAL) << "Couldn't create bufferevent.. ignoring connection";
    return;
  }

  ServerConnection *ci = new ServerConnection(bev, m_port, this);

  
  bufferevent_enable(bev, EV_READ|EV_WRITE);
}

void
ServerInstance::callback_accept_error()
{
  int err = EVUTIL_SOCKET_ERROR();
  LOG(FATAL) << "Got error <" << err << ": " << evutil_socket_error_to_string(err) << "> on the connection listener. Shutting down.";

  event_base_loopexit(m_base, NULL);
}

std::string
ServerInstance::UserFolder(const std::string &user) const
{
  return std::string() + this->m_folder + user + "/";
}

bool
ServerInstance::IsValidUser(const std::string &user, const std::string &pass)
{
  if (m_users[user].compare(pass) == 0) return true;
  else return false;
}


void ServerInstance::CreateUserFolder(const std::string& user)
{
  std::string dir = UserFolder(user);
  if ( !utils::DirectoryExists(dir) )
  {
    int err = mkdir(dir.c_str(), 0744);
    if (err)
    {
      LOG(FATAL) << "Error creating user directory: " << dir;
      return;
    }
    LOG(INFO) << "Created user directory: " << dir;
  }
}

/************************************************************************

                                          __  .__                      
  ____  ____   ____   ____   ____   _____/  |_|__| ____   ____   ______
_/ ___\/  _ \ /    \ /    \_/ __ \_/ ___\   __\  |/  _ \ /    \ /  ___/
\  \__(  <_> )   |  \   |  \  ___/\  \___|  | |  (  <_> )   |  \\___ \ 
 \___  >____/|___|  /___|  /\___  >\___  >__| |__|\____/|___|  /____  >
     \/           \/     \/     \/     \/                    \/     \/ 

************************************************************************/


ServerConnection::ServerConnection(bufferevent *bev, int port, ServerInstance* server) :
  m_bev( bev ),
  m_port( port ),
  m_server( server ),
  m_input( NULL ),
  m_output( NULL ),
  m_putInfo()
{
  bufferevent_setcb(bev, callback_read, NULL, callback_event, this);
  bufferevent_setwatermark(bev, EV_WRITE, 0, 0);
  m_input = bufferevent_get_input(m_bev);
  m_output = bufferevent_get_output(m_bev);

  memset(&m_putInfo, 0, sizeof m_putInfo);
}

void
ServerConnection::ResetCallbacks()
{
  bufferevent_setcb(m_bev, callback_read, NULL, callback_event, this);
}

bool
ServerConnection::Authenticate(const std::string& line, Command* const c)
{
  VLOG(2) << __PRETTY_FUNCTION__;
  std::istringstream ss(line);

  // std::string user;
  if ( !(ss >> c->user) )
  {
    LOG(ERROR) << "Couldn't read user: " << line;
    return false;
  }
  // LOG(INFO) << "User: " << c->user;


  // std::string pass;
  if ( !(ss >> c->pass) )
  {
    LOG(ERROR) << "Couldn't read pass: " << line;
    return false;
  }
  // LOG(INFO) << "Password: " << c->pass;

  if ( !m_server->IsValidUser(c->user, c->pass) )
  {
    LOG(WARNING) << "Invalid credentials: " << c->user << ", " << c->pass;
    bufferevent_write(m_bev, INVALID_CREDENTIALS, strlen(INVALID_CREDENTIALS));
    return false;
  }
  m_server->CreateUserFolder(c->user);
  return true;
}

void
ServerConnection::callback_event(short events)
{
  VLOG(2) << __PRETTY_FUNCTION__;

  if ( (events & (BEV_EVENT_READING|BEV_EVENT_EOF)) )
  {
    VLOG(1) << "Closing (CLIENT EOF)";
    // close_connection(ci);
    return;
  }
}

void
ServerConnection::callback_read()
{
  VLOG(2) << __PRETTY_FUNCTION__;

  size_t bytes_read = 0;
  char *line_command = evbuffer_readln(m_input, &bytes_read, EVBUFFER_EOL_CRLF);
  char *line_auth = evbuffer_readln(m_input, &bytes_read, EVBUFFER_EOL_CRLF);
  if (!line_command || !line_auth) return;

  Command *pc = ServerInstance::ParseCommand(line_command);
  if ( !pc )
  {
    LOG(ERROR) << "Error reading last command.";
    return;
  }

  bool authGood = Authenticate(line_auth, pc);
  if ( !authGood ) return;
  
  switch ( pc->Type() ) {
    case Command::Type::Get:
    {
      Command_Get *c = reinterpret_cast<Command_Get*>(pc);
      if ( !c->valid ) LOG(WARNING) << "Invalid GET command.";
      else LOG(INFO) << "Get: " << c->info;
      // DoGet(c);
      break;
    }
    case Command::Type::List:
    {
      Command_List *c = reinterpret_cast<Command_List*>(pc);
      if ( !c->valid ) LOG(WARNING) << "Invalid LIST command.";
      // else if ( !ci->userValid )
      // {
      //   LOG(WARNING) << "List: " << INVALID_CREDENTIALS;
      //   bufferevent_write(m_bev, INVALID_CREDENTIALS, strlen(INVALID_CREDENTIALS));
      // }
      else
      {
        std::vector<std::string> files;
        DIR *dirp = opendir(m_server->UserFolder(c->user).c_str());
        struct dirent *dp;
        while ( (dp = readdir(dirp)) != NULL )
        {
          LOG(INFO) << "Found file: " << dp->d_name;
          files.push_back(std::string(dp->d_name));
        }
      }
      break;
    }
    case Command::Type::Put:
    {
      Command_Put *c = reinterpret_cast<Command_Put*>(pc);
      if ( !c->valid ) LOG(WARNING) << "Invalid PUT command.";
      // else if ( !ci->userValid )
      // {
      //   LOG(WARNING) << "Put: " << INVALID_CREDENTIALS;
      //   bufferevent_write(m_bev, INVALID_CREDENTIALS, strlen(INVALID_CREDENTIALS));
      // }
      else
      {
        m_putInfo.filename = std::string(m_server->UserFolder(c->user) + c->filename);
        std::string ready_resp = Command_Put::PUT_READY() + "\n";
        bufferevent_write(m_bev, ready_resp.c_str(), ready_resp.length());
        bufferevent_setcb(m_bev, callback_put, NULL, callback_event, this);
        // LOG(INFO) << "Entering PUT-- " << ci->putInfo->putDest << " (" << ci->putInfo->putTotalSize << ")";
      }
    }
  }
}

void
ServerConnection::callback_put()
{
  VLOG(2) << __PRETTY_FUNCTION__;
  size_t available = 0;
  std::string filename = m_putInfo.filename;
  
  size_t bytes_read = 0;
  
  while (true)
  {
    VLOG(4) << "Looping";
    if (m_putInfo.putState == 0 || m_putInfo.putState == 2)
    {
      VLOG(4) << "In put state " << m_putInfo.putState;
      char *l = evbuffer_readln(m_input, &bytes_read, EVBUFFER_EOL_CRLF);
      std::istringstream ss(l);
      
      if ( !(ss >> m_putInfo.partnum) ) {
        LOG(ERROR) << "\tError reading partnum: " << l;
        ResetCallbacks();
        return;
      }
  
      if ( !(ss >> m_putInfo.wantedSize) ) {
        LOG(ERROR) << "\tError reading wantedSize: " << l;
        ResetCallbacks();
        return;
      }
      VLOG(4) << "\tPart number=" << m_putInfo.partnum << ", length=" << m_putInfo.wantedSize;
      
      m_putInfo.putState++;
      VLOG(4) << "\tSetting state to " << m_putInfo.putState;
  
      available = evbuffer_get_length(m_input);
      VLOG(4) << "\tAvailable in buffer: " << available << "\n\n";
      if (available == 0) return;
    } // end putstate 0 || 2

    if (m_putInfo.putState == 1 || m_putInfo.putState == 3)
    {
      VLOG(4) << "In put state " << m_putInfo.putState;
      available = evbuffer_get_length(m_input);
      LOG(WARNING) << "\tAvailable in buffer: " << available;
      if (available < m_putInfo.wantedSize)
      {
        VLOG(1) << "\tWaiting for more data to come to the buffer";
      }
      else if (available >= m_putInfo.wantedSize)
      {
        VLOG(4) << "\tHave enough data for part " << m_putInfo.partnum;
        char *data = new char[m_putInfo.wantedSize];
  
        size_t copied = evbuffer_remove(m_input, data, m_putInfo.wantedSize);
        if (copied != m_putInfo.wantedSize)
        {
          LOG(WARNING) << "\tCopied different bytes than wanted: " << copied << "/" << m_putInfo.wantedSize;
        }
  
        std::ofstream fileDest;
        std::string putDest = filename + "." + std::to_string(m_putInfo.partnum);
        fileDest.open(putDest, std::ios_base::out);
        if (!fileDest.is_open())
        {
          LOG(FATAL) << "\tError opening file: " << putDest;
          ResetCallbacks();
          delete data;
          return;
        }
        VLOG(1) << "\tWriting " << copied << "bytes to " << putDest;
        fileDest.write(data, copied);

        fileDest.close();
        delete data;
      }
      
  
      m_putInfo.putState++;
      // LOG(INFO) << "\tSetting state to " << putState;

      if (m_putInfo.putState == 4)
      {
        available = evbuffer_get_length(m_input);
        VLOG(1) << "\tAccepted all data! leftover=" << available;
        // all done...
        ResetCallbacks();
        memset(&m_putInfo, 0, sizeof m_putInfo); 
        // event_base_loopexit(bufferevent_get_base(bev), NULL);
        return;
      }
  
      available = evbuffer_get_length(m_input);
      // LOG(INFO) << "\tAvailable in buffer: " << available << "\n\n";
      if (available == 0) return;
  
    } // end putstate = 1||3
  } // end while loop
}










