#include "Client.h"

Client::Client()
  : m_manager()
{

}

Client::~Client()
{

}

void
Client::Put(Command_Put *c)
{
  VLOG(2) << __PRETTY_FUNCTION__;
  unsigned char hash[MD5_DIGEST_LENGTH];

  int file = open(c->filename.c_str(), O_RDONLY);
  if (file < 0)
  {
    LOG(ERROR) << "Can't open file " << c->filename;
    return;
  }

  size_t file_size = utils::get_size_by_fd(file);
  VLOG(2) << "File size: " << file_size;
  char* file_buffer = (char*)mmap(0, file_size, PROT_READ, MAP_SHARED, file, 0);
  MD5((unsigned char*) file_buffer, file_size, hash);
  munmap(file_buffer, file_size);

  int x = (int)hash[MD5_DIGEST_LENGTH-1] % 4;
  VLOG(3) << "x = " << x;


  // Because there will always be 4 parts, there may be up
  // up to a 3 byte remainder. So, just make the first three
  // the same size, and the last one have the extra bytes
  size_t part_size_first;
  size_t part_size_last;
  if (file_size % 4 == 0)
  {
    part_size_first = part_size_last = file_size / 4;
  }
  else
  {
    part_size_first = file_size / 4;
    part_size_last  = file_size - (part_size_first*3);
  }
  VLOG(2) << "First size: " << part_size_first;
  VLOG(2) << "Last  size: " << part_size_last;

  close(file);
  
  VLOG(9) << "Starting to distribute the files";

  std::thread t1, t2, t3, t4;
  
  char pieces = FilePieces(1, x);
  for (int i = 1; i <= 4; i++)
  {
    Connection *s = m_manager.Get(i);
    Connection::PutInfo pi;
    memset(&pi, 0, sizeof pi);
    pi.name = c->filename;
    switch (FilePieces(i, x)) {
      case FILE_PAIR_1:
        VLOG(2) << "PUT pair 1 to server " << i;
        pi.part1_number = 1;
        pi.length1 = part_size_first;
        pi.offset1 = 0;

        pi.part2_number = 2;
        pi.length2 = part_size_first;
        pi.offset2 = part_size_first;
        t1 = std::thread(std::bind(&Connection::Put, s, c->filename, pi));
        break;
      case FILE_PAIR_2:
        VLOG(2) << "PUT pair 2 to server " << i;
        pi.part1_number = 2;
        pi.length1 = part_size_first;
        pi.offset1 = part_size_first;

        pi.part2_number = 3;
        pi.length2 = part_size_first;
        pi.offset2 = part_size_first * 2;
        t2 = std::thread(std::bind(&Connection::Put, s, c->filename, pi));
        break;
      case FILE_PAIR_3:
        VLOG(2) << "PUT pair 3 to server " << i;
        pi.part1_number = 3;
        pi.length1 = part_size_first;
        pi.offset1 = part_size_first * 2;

        pi.part2_number = 4;
        pi.length2 = part_size_last;
        pi.offset2 = part_size_first * 3;
        t3 = std::thread(std::bind(&Connection::Put, s, c->filename, pi));
        break;
      case FILE_PAIR_4:
        VLOG(2) << "PUT pair 4 to server " << i;
        pi.part1_number = 4;
        pi.length1 = part_size_last;
        pi.offset1 = part_size_first * 3;

        pi.part2_number = 1;
        pi.length2 = part_size_first;
        pi.offset2 = 0;
        t4 = std::thread(std::bind(&Connection::Put, s, c->filename, pi));
        break;
      default:
        LOG(ERROR) << "Unknown file part";
        break;
    }
  }

  LOG(INFO) << "Waiting for threads...";
  t1.join(); LOG(INFO) << "thread 1 joined!";
  t2.join(); LOG(INFO) << "thread 2 joined!";
  t3.join(); LOG(INFO) << "thread 3 joined!";
  t4.join(); LOG(INFO) << "thread 4 joined!";

  delete c;
}

void
Client::List(Command_List *c)
{
  VLOG(2) << __PRETTY_FUNCTION__;

  delete c;
}


void
Client::Get(Command_Get *c)
{
  VLOG(2) << __PRETTY_FUNCTION__;

  delete c;
}

bool
Client::Initialize()
{
  return m_manager.ConnectAll();
}

char
Client::FilePieces(int server_number, int x_val)
{
  switch (x_val)
  {
    case 0:
      if      (server_number == 1) return FILE_PAIR_1;
      else if (server_number == 2) return FILE_PAIR_2;
      else if (server_number == 3) return FILE_PAIR_3;
      else if (server_number == 4) return FILE_PAIR_4;
    case 1:
      if      (server_number == 1) return FILE_PAIR_4;
      else if (server_number == 2) return FILE_PAIR_1;
      else if (server_number == 3) return FILE_PAIR_2;
      else if (server_number == 4) return FILE_PAIR_3;
    case 2:
      if      (server_number == 1) return FILE_PAIR_3;
      else if (server_number == 2) return FILE_PAIR_4;
      else if (server_number == 3) return FILE_PAIR_1;
      else if (server_number == 4) return FILE_PAIR_2;
    case 3:
      if      (server_number == 1) return FILE_PAIR_2;
      else if (server_number == 2) return FILE_PAIR_3;
      else if (server_number == 3) return FILE_PAIR_4;
      else if (server_number == 4) return FILE_PAIR_1;
    default:
      return 0;
  }
}




bool
Client::ParseConfFile(const std::string& filename)
{
  int foundServers = 0;

  std::ifstream file;
  file.open(filename);

  if ( !file.is_open() )
  {
    LOG(FATAL) << "Couldn't open file: " << filename;
    return false;
  }

  std::string line;
  int lineCount = 0;
  while ( getline(file, line) )
  {
    lineCount++;
    std::string directive;
    if ( line.at(0) == '#' ) continue;
    std::istringstream ss(line);
    std::locale x(std::locale::classic(), new utils::colon_seperator);
    ss.imbue(x);

    if ( !(ss >> directive) )
    {
      LOG(ERROR) << "Conf parsing error: read line " << lineCount;
      return false;
    }

    if ( directive.compare("Server") == 0 )
    {
      std::string serverName;
      if ( !(ss >> serverName) )
      {
        LOG(ERROR) << "Conf parsing error: server name. Line " << lineCount;
        return false;
      }

      std::string ip;
      if ( !(ss >> ip) )
      {
        LOG(ERROR) << "Conf parsing error: server ip. Line " << lineCount;
        return false;
      }

      short port;
      if ( !(ss >> port) )
      {
        LOG(ERROR) << "Conf parsing error: server port. Line " << lineCount;
        return false;
      }
      // Connection *s = new Connection(++foundServers, ip, port, &m_manager);
      Connection *s = m_manager.CreateNewConnection(++foundServers, ip, port);
      if ( !(s->Initialize()) ) return false;
      LOG(INFO) << "Using server " << s->IP() << " [" << s->Port() << "]";
      
    }
    else if ( directive.compare("Username") == 0 )
    {
      std::string username;
      if ( !(ss >> username) )
      {
        LOG(ERROR) << "Conf parsing error: username. Line " << lineCount;
        return false;
      }
      VLOG(1) << "Using user: " << username;
      m_manager.SetUser(username);
    }
    else if ( directive.compare("Password") == 0 )
    {
      std::string pass;
      if ( !(ss >> pass) )
      {
        LOG(ERROR) << "Conf parsing error: password. Line " << lineCount;
        return false;
      }
      VLOG(1) << "Using pass: " << pass;
      m_manager.SetPassword(pass);
    }
    else
    {
      LOG(WARNING) << "Unknown conf option: " << line;
    }
  }

  file.close();

  if (foundServers == 4) return true;
  else {
    LOG(FATAL) << "Only found " << foundServers << " servers.";
    return false;
  }
}