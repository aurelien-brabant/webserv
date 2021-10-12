#pragma once
#include <vector>
#include <map>
#include <string>
#include "webserv/config-parser/Lexer.hpp"

typedef std::map<std::string, std::string> DirectiveMap;

enum BlockType {
    BLOCK_GLOBAL = 0,
    BLOCK_LOCATION = 1 << 0,
    BLOCK_SERVER = 1 << 1
};

class ConfigBlock  {
  BlockType _type;
  DirectiveMap _directives;
  ConfigBlock* _parent;

  public:
    std::vector<ConfigBlock*> _blocks;
    ConfigBlock(BlockType type, ConfigBlock* parent = 0);
    ~ConfigBlock(void);

    BlockType getType(void) const;
    DirectiveMap& getDirectiveMap(void);
    ConfigBlock* getParent(void) const;
};

std::ostream& operator<<(std::ostream& os, ConfigBlock* block);

class ConfigParser {
    std::vector<Lexer::Token> lex(const std::string& data);
    ConfigBlock* parse(const std::vector<Lexer::Token>& tv);

  public:
    ConfigParser(void);
    ConfigParser(const ConfigParser& other);

    ~ConfigParser(void);

    ConfigParser& operator=(const ConfigParser& rhs);

    ConfigBlock* loadConfig(const char* configPath);

    std::ostream& printConfig(std::ostream& os, ConfigBlock* main, size_t depth = 0);
};
