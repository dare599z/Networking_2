#ifndef MCBRIDE_INC_CLIENT_H
#define MCBRIDE_INC_CLIENT_H

#include <event2/listener.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/util.h>

#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <openssl/md5.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <mutex>
#include <functional>
#include <algorithm>
#include <iterator>

#include "easylogging++.h"
#include "utilities.h"
#include "commands.h"
#include "Connection.h"
#include "Client.h"

#define FILE_PAIR_1 1 // (1<<1) | (1<<2)
#define FILE_PAIR_2 2 // (1<<2) | (1<<3)
#define FILE_PAIR_3 3 // (1<<3) | (1<<4)
#define FILE_PAIR_4 4 // (1<<4) | (1<<1)



class Client {
public:
  Client();
  ~Client();
  bool ParseConfFile(const std::string& filename);

  void List(Command_List *c);
  void Get(Command_Get *c);
  void Put(Command_Put *c);

  bool Initialize();
  void AddFiles(const std::vector<std::string>& files);
  void FileBuilder(const std::string& filename, int partnum, char* data, size_t size);

  std::string m_auth_user;      // safer to do with mutators, but whatever for now
  std::string m_auth_password;  // safer to do with mutators, but whatever for now

private:
  ConnectionManager m_manager;
  std::vector<std::string> m_files;
  std::mutex m_mutex;

  struct file_parts {
    bool parts[4];

    bool Complete() const
    {
      if ( parts[1] && parts[2] && parts[3] && parts[0] ) return true;
      return false;
    }

    void HavePart(int n)
    {
      parts[n-1] = true; // totally unsafe...
    }
  };
  
  static char FilePieces(int server_number, int x_val);

};


#endif //MCBRIDE_INC_CLIENT_H
