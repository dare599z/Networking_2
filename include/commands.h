#ifndef MCBRIDE_INC_COMMANDS_H
#define MCBRIDE_INC_COMMANDS_H
struct Command 
{ 
  enum Type {Auth, Get, Put, List};
  bool valid;
  virtual enum Type Type() const = 0;

  Command() { valid = false; }
};
struct Command_Auth : public Command
{
  std::string user;
  std::string password;
  virtual enum Type Type() const { return Type::Auth; }
};
struct Command_Get : public Command
{
  std::string info;
  virtual enum Type Type() const { return Type::Get; }
};
struct Command_Put : public Command
{
  std::string filename;
  size_t size;
  virtual enum Type Type() const { return Type::Put; }
};
struct Command_List : public Command
{
  virtual enum Type Type() const { return Type::List; }
};

#endif
