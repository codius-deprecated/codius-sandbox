#ifndef DEBUG_H
#define DEBUG_H

#include <string>
#include <iostream>

class DebugOutput {
public:
  DebugOutput(const std::string& tag = "Unknown") :
    m_tag(tag)
  {
    if (enabled())
      std::cout << m_tag;
  }

  ~DebugOutput() {
    if (enabled())
      std::cout << std::endl;
  }

  template<typename V> DebugOutput& operator<<(const V& str) {
    if (enabled())
      std::cout << " " << str;
    return *this;
  }

  static bool enabled() {
    return getenv("CODIUS_DEBUG") != NULL;
  }

private:
  const std::string m_tag;
};

#define Debug() DebugOutput(__PRETTY_FUNCTION__)

#endif // DEBUG_H
