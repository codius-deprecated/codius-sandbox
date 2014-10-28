#include "codius-util.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <cppunit/extensions/HelperMacros.h>
#include <string.h>

#define FD_SEND 0
#define FD_RECV 1

class IPCTest : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE (IPCTest);
  CPPUNIT_TEST (testSendRequest);
  CPPUNIT_TEST_SUITE_END ();

public:
  void testSendRequest() {
    codius_request_t* req = codius_request_new ("test_api", "test_method");
    int ret = codius_write_request (test_fd[FD_SEND], req);
    CPPUNIT_ASSERT_EQUAL (ret, 0);
    codius_request_t* sent_req = codius_read_request (test_fd[FD_RECV]);
    CPPUNIT_ASSERT_EQUAL (strcmp (req->api_name, sent_req->api_name), 0);
    codius_request_free (req);
  }

  void setUp() {
    socketpair (AF_UNIX, SOCK_STREAM, 0, test_fd);
  }

  void tearDown() {
    close (test_fd[0]);
    close (test_fd[1]);
  }

private:
  int test_fd[2];
};

CPPUNIT_TEST_SUITE_REGISTRATION (IPCTest);
