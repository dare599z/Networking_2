#ifndef MCBRIDE_INC_UTILS_H
#define MCBRIDE_INC_UTILS_H

#include <sys/stat.h>
#include <locale>
#include <iostream>
#include <algorithm>
#include <iterator>
#include <vector>
#include <sstream>

namespace utils
{

static std::string
file_extension(const std::string& filename)
{
  std::string::size_type idx = filename.rfind('.');

  if (idx != std::string::npos)
  {
    return filename.substr(idx);
  }
  else
  {
    // No extension found
    return "";
  }
}

static size_t
get_size_by_fd(int fd) {
    struct stat statbuf;
    if(fstat(fd, &statbuf) < 0) exit(-1);
    return statbuf.st_size;
}

static bool
file_exists(const std::string& name) 
{
  struct stat buffer;
  return (stat (name.c_str(), &buffer) == 0); 
}

static bool 
DirectoryExists(const std::string& path)
{
  struct stat sb;
  if ( stat(path.c_str(), &sb) == 0 && S_ISDIR(sb.st_mode) ) return true;
  else return false;
}

static std::string
getCmdOption(const char ** begin, const char ** end, const std::string & option)
{
  const char ** itr = std::find(begin, end, option);
  if (itr != end && ++itr != end)
  {
      return std::string(*itr);
  }
  return std::string();
}

static bool
cmdOptionExists(const char** begin, const char** end, const std::string& option)
{
  return std::find(begin, end, option) != end;
}

// The following structure was learned from 
// http://stackoverflow.com/a/10376445
class colon_seperator : public std::ctype<char>
{
  mask my_table[table_size];
public:
  colon_seperator(size_t refs = 0)  
    : std::ctype<char>(&my_table[0], false, refs)
  {
    std::copy_n(classic_table(), table_size, my_table);
    my_table[':'] = (mask)space;
  }
};


} // end util namespace

#endif //MCBRIDE_INC_UTILS_H
