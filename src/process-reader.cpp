#include "process-reader.h"

#include <cassert>

#include <sys/ptrace.h>

ProcessReader::ProcessReader(pid_t pid)
  : m_pid (pid)
{}

Sandbox::Word
ProcessReader::peekData(Sandbox::Address addr) const
{
  Sandbox::Word w = ptrace (PTRACE_PEEKDATA, m_pid, addr, NULL);
  assert (errno == 0);
  return w;
}

bool
ProcessReader::copyData(Sandbox::Address addr, std::vector<char>& buf) const
{
  size_t i;
  for (i = 0; buf.size() - i > sizeof (Sandbox::Word); i += sizeof (Sandbox::Word)) {
    Sandbox::Word d = peekData (addr+i);
    for (size_t j = 0; j < sizeof (Sandbox::Word) && i + j < buf.size(); j++) {
      buf[i + j] = ((char*)(&d))[j];
    }
  }

  if (i != buf.size()) {
    Sandbox::Word d = peekData (addr + i);
    for (size_t j = 0; j < sizeof (Sandbox::Word) && i + j < buf.size(); j++) {
      buf[i] = ((char*)(&d))[j];
    }
  }

  if (errno)
    return false;
  return true;
}

bool
ProcessReader::readString (Sandbox::Address addr, std::vector<char>& buf) const
{
  bool endString = false;
  size_t i;
  for (i = 0; !endString && buf.size() - i > sizeof (Sandbox::Word); i += sizeof (Sandbox::Word)) {
    Sandbox::Word d = peekData(addr + i);
    for (size_t j = 0; j < sizeof (Sandbox::Word) && i + j < buf.size(); j++) {
      buf[i + j] = ((char*)(&d))[j];
      if (buf[i + j] == 0) {
        endString = true;
        break;
      }
    }
  }

  if (i != buf.size() && !endString) {
    Sandbox::Word d = peekData (addr + i);
    for (size_t j = 0; j < sizeof (Sandbox::Word) && i + j < buf.size(); j++) {
      buf[i + j] = ((char*)(&d))[j];
    }
  }

  if (errno)
    return false;
  return true;
}

bool
ProcessReader::pokeData(Sandbox::Address addr, Sandbox::Word word) const
{
  return ptrace (PTRACE_POKEDATA, m_pid, addr, word);
}

bool
ProcessReader::writeData (Sandbox::Address addr, const char* buf, size_t length) const
{
  size_t i;
  for (i = 0; length - i > sizeof (Sandbox::Word); i += sizeof (Sandbox::Word)) {
    Sandbox::Word d;
    for (size_t j = 0; j < sizeof (Sandbox::Word) && i+j < length; j++) {
      ((char*)(&d))[j] = buf[i+j];
    }
    pokeData (addr + i, d);
  }
  if (i != length) {
    Sandbox::Word d = peekData (addr + i);
    for (size_t j = 0; j < sizeof (Sandbox::Word) && i + j < length; j++) {
      ((char*)(&d))[j] = buf[i+j];
    }
    pokeData (addr + i, d);
  }
  if (errno)
    return false;
  return true;
}

