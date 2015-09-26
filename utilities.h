#ifndef UTILS_INC
#define UTILS_INC

namespace utils
{
  
std::string
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

bool
file_exists(const std::string& name) {
  struct stat buffer;
  return (stat (name.c_str(), &buffer) == 0); 
}

std::string
getCmdOption(const char ** begin, const char ** end, const std::string & option)
{
  const char ** itr = std::find(begin, end, option);
  if (itr != end && ++itr != end)
  {
      return std::string(*itr);
  }
  return std::string();
}

bool
cmdOptionExists(const char** begin, const char** end, const std::string& option)
{
  return std::find(begin, end, option) != end;
}


} // end util namespace

#endif
