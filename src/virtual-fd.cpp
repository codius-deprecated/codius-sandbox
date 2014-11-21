#include "virtual-fd.h"
#include "debug.h"

VirtualFD::VirtualFD(int localFD) :
  m_localFD (localFD)
{
  //FIXME: gcc-4.8 lacks stdatomic.h, so we're stuck with gcc builtins :(
  //see also: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=58016
  m_virtualFD = __sync_fetch_and_add (&s_nextFD, 1);
  Debug() << m_virtualFD << "->" << m_localFD;
}

void
VirtualFD::invalidate()
{
  Debug() << m_virtualFD << "->" << m_localFD;
  m_localFD = -1;
}

int
VirtualFD::virtualFD() const
{
  return m_virtualFD;
}

int
VirtualFD::localFD() const
{
  return m_localFD;
}

int VirtualFD::s_nextFD = VirtualFD::firstVirtualFD;

bool
VirtualFDGenerator::isVirtualFD (int fd) const
{
  if (VirtualFD::isVirtualFD(fd)) {
    return m_openFDs.find (fd) != m_openFDs.cend();
  }
  return false;
}

VirtualFD::Ptr
VirtualFDGenerator::getVirtualFD (int fd) const
{
  return m_openFDs.at (fd);
}

void
VirtualFDGenerator::registerFD (VirtualFD::Ptr fd)
{
  m_openFDs.insert (std::make_pair (fd->virtualFD(), fd));
}

void
VirtualFDGenerator::removeFD (VirtualFD::Ptr fd)
{
  m_openFDs.erase (fd->virtualFD());
}
