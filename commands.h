struct Command 
{ 
  enum Type {User, Password, Get, Put, List};
  bool valid;
  virtual enum Type Type() const = 0;

  Command() { valid = false; }
};
struct Command_User : public Command
{
  std::string user;
  virtual enum Type Type() const { return Type::User; }
};
struct Command_Password : public Command
{
  std::string password;
  virtual enum Type Type() const { return Type::Password; }
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