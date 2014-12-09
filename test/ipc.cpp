#include "codius-util.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <cppunit/extensions/HelperMacros.h>
#include <string.h>

#define FD_SEND 0
#define FD_RECV 1

class IPCMessagingTest : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE (IPCMessagingTest);
  CPPUNIT_TEST (testReply);
  CPPUNIT_TEST (testSyncCall);
  CPPUNIT_TEST_SUITE_END ();

private:
  static void* listenThread(void* data) {
    IPCMessagingTest* self = static_cast<IPCMessagingTest*> (data);
    codius_request_t* req = codius_read_request (self->test_fd[FD_RECV]);
    codius_result_t* result = codius_result_new ();
    codius_send_reply (req, result);
    codius_request_free (req);
    codius_result_free (result);
    return NULL;
  }

public:
  void testSyncCall() {
    codius_request_t* req;
    codius_result_t* result;

    pthread_t thread;
    dup2 (test_fd[FD_SEND], 3);
    pthread_create (&thread, NULL, IPCMessagingTest::listenThread, this);

    req = codius_request_new ("test_api", "test_method");
    result = codius_sync_call (req);
    close (3);

    pthread_join (thread, NULL);

    CPPUNIT_ASSERT_EQUAL (req->_id, result->_id);
    CPPUNIT_ASSERT_EQUAL (req->data, result->data);
  }

  void testReply() {
    codius_request_t* req;
    codius_request_t* sent_req;
    codius_result_t* result;
    codius_result_t* sent_result;

    req = codius_request_new ("test_api", "test_method");
    codius_write_request (test_fd[FD_SEND], req);
    sent_req = codius_read_request (test_fd[FD_RECV]);
    CPPUNIT_ASSERT_EQUAL (req->_id, sent_req->_id);
    result = codius_result_new ();
    codius_send_reply (sent_req, result);
    sent_result = codius_read_result (test_fd[FD_SEND]);

    CPPUNIT_ASSERT_EQUAL (req->_id, sent_result->_id);

    codius_request_free (req);
    codius_request_free (sent_req);
    codius_result_free (result);
    codius_result_free (sent_result);
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

class IPCResultTest : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE (IPCResultTest);
  CPPUNIT_TEST (testSendRecv);
  CPPUNIT_TEST (testDecodeEncode);
  CPPUNIT_TEST (testDecodeEncodeWithPayload);
  CPPUNIT_TEST_SUITE_END ();

public:
  void testSendRecv() {
    codius_result_t* result = codius_result_new ();
    int ret = codius_write_result (test_fd[FD_SEND], result);
    CPPUNIT_ASSERT_EQUAL (0, ret);
    codius_result_t* sent_result = codius_read_result (test_fd[FD_RECV]);
    CPPUNIT_ASSERT (result != nullptr);
    CPPUNIT_ASSERT_EQUAL (result->data, sent_result->data);
    CPPUNIT_ASSERT_EQUAL (result->_id, sent_result->_id);
    codius_result_free (result);
  }

  void testDecodeEncodeWithPayload() {
    char* buf;
    codius_result_t* decoded;
    codius_result_t* result = codius_result_new ();
    result->_id = 42;
    result->data = json_mknumber (42);
    buf = codius_result_to_string (result);
    decoded = codius_result_from_string (buf);
    CPPUNIT_ASSERT_EQUAL (result->success, decoded->success);
    CPPUNIT_ASSERT_EQUAL (JSON_NUMBER, decoded->data->tag);
    CPPUNIT_ASSERT_EQUAL ((double)42, decoded->data->number_);

    codius_result_free (decoded);
    codius_result_free (result);
    free (buf);
  }


  void testDecodeEncode() {
    char* buf;
    codius_result_t* decoded;
    codius_result_t* result = codius_result_new ();
    result->_id = 42;
    buf = codius_result_to_string (result);
    decoded = codius_result_from_string (buf);
    CPPUNIT_ASSERT_EQUAL (result->success, decoded->success);
    CPPUNIT_ASSERT_EQUAL (result->data, decoded->data);

    codius_result_free (decoded);
    codius_result_free (result);
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

class IPCRequestTest : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE (IPCRequestTest);
  CPPUNIT_TEST (testSendRecv);
  CPPUNIT_TEST (testDecodeEncode);
  CPPUNIT_TEST (testDecodeEncodeWithPayload);
  CPPUNIT_TEST_SUITE_END ();

public:
  void testUniqueIDs() {
    codius_request_t* req = codius_request_new (NULL, NULL);
    codius_request_t* req2 = codius_request_new (NULL, NULL);

    CPPUNIT_ASSERT (req->_id != req2->_id);
    codius_request_free (req);
    codius_request_free (req2);
  }

  void testSendRecv() {
    codius_request_t* req = codius_request_new ("test_api", "test_method");
    int ret = codius_write_request (test_fd[FD_SEND], req);
    CPPUNIT_ASSERT_EQUAL (0, ret);
    codius_request_t* sent_req = codius_read_request (test_fd[FD_RECV]);
    CPPUNIT_ASSERT_EQUAL (strcmp (req->api_name, sent_req->api_name), 0);
    CPPUNIT_ASSERT_EQUAL (req->_id, sent_req->_id);
    codius_request_free (req);
  }

  void testDecodeEncodeWithPayload() {
    char* buf;
    codius_request_t* decoded;
    codius_request_t* req = codius_request_new ("test_api", "test_method");
    req->data = json_mknumber (42);
    buf = codius_request_to_string (req);
    decoded = codius_request_from_string (buf);
    CPPUNIT_ASSERT_EQUAL (std::string(req->api_name), std::string(decoded->api_name));
    CPPUNIT_ASSERT_EQUAL (std::string(req->method_name), std::string(decoded->method_name));
    CPPUNIT_ASSERT_EQUAL (JSON_NUMBER, decoded->data->tag);
    CPPUNIT_ASSERT_EQUAL ((double)42, decoded->data->number_);

    codius_request_free (decoded);
    codius_request_free (req);
    free (buf);
  }

  void testDecodeEncode() {
    char* buf;
    codius_request_t* decoded;
    codius_request_t* req = codius_request_new ("test_api", "test_method");
    buf = codius_request_to_string (req);
    decoded = codius_request_from_string (buf);
    CPPUNIT_ASSERT_EQUAL (std::string(req->api_name), std::string(decoded->api_name));
    CPPUNIT_ASSERT_EQUAL (std::string(req->method_name), std::string(decoded->method_name));
    CPPUNIT_ASSERT_EQUAL (decoded->data, (JsonNode*)NULL);

    codius_request_free (decoded);
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

CPPUNIT_TEST_SUITE_REGISTRATION (IPCRequestTest);
CPPUNIT_TEST_SUITE_REGISTRATION (IPCResultTest);
CPPUNIT_TEST_SUITE_REGISTRATION (IPCMessagingTest);
