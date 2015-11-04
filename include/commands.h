#ifndef MCBRIDE_INC_COMMANDS_H
#define MCBRIDE_INC_COMMANDS_H
struct Command 
{ 
  enum Type {Get, Put, List};
  bool valid;
  std::string user;
  std::string pass;
  virtual enum Type Type() const = 0;

  Command() : user(), pass() { valid = false; }
};
struct Command_Get : public Command
{
  std::string filename;
  int partnum;
  virtual enum Type Type() const { return Type::Get; }
};
struct Command_Put : public Command
{
  std::string filename;
  virtual enum Type Type() const { return Type::Put; }

  static std::string PUT_READY() { return "PUT_READY"; }
};
struct Command_List : public Command
{
  virtual enum Type Type() const { return Type::List; }
  static std::string LIST_TERMINAL() { return "<<<__LIST_DONE__>>>"; }
};

#endif
