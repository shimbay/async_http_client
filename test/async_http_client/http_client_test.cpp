#include <future>
#include <gtest/gtest.h>
#include <memory>

#include "async_http_client/http_client.h"

using namespace async_http_client;

class HTTPClientTest : public ::testing::Test {
public:
  static void SetUpTestSuite() {}

  static void TearDownTestSuite() {}

  static std::shared_ptr<AsyncHTTPClient> client;
  static std::shared_ptr<conlib::Executor> executor;
};

std::shared_ptr<AsyncHTTPClient> HTTPClientTest::client;
std::shared_ptr<conlib::Executor> HTTPClientTest::executor;

TEST_F(HTTPClientTest, Load) {
  executor = std::make_shared<conlib::Executor>(4);

  AsyncHTTPClient::Setting setting{};
  setting.connection_timeout = 3000;
  setting.request_timeout = 10000;
  setting.executor = executor;
  setting.debug = true;
  client = std::make_shared<AsyncHTTPClient>(setting);
}

TEST_F(HTTPClientTest, Get) {
  std::promise<void> p;
  std::future<void> f = p.get_future();
  auto http_cb = [&](HTTPErrorCode err, const std::string &err_msg,
                     const AsyncHTTPClient::Response &response) {
    if (err != OK) {
      std::cout << "HTTP request failed: " << err_msg << std::endl;
    } else {
      std::cout << "Code: " << response.code << std::endl;
      for (auto &header : response.headers) {
        std::cout << "Header: " << header.first << ": " << header.second << std::endl;
      }
      if (!response.body.empty()) {
        std::cout << "Body: " << response.body.data() << std::endl;
      }
    }
    p.set_value();
  };

  AsyncHTTPClient::GetRequest req{"10.199.1.14:8080/test/get", http_cb};
  req.add_url_param("a", "b");
  req.add_url_param("a+c", "b=d");
  req.add_header("test", "value");
  client->get(req);
  f.get();
}

TEST_F(HTTPClientTest, PostForm) {
  std::promise<void> p;
  std::future<void> f = p.get_future();
  auto http_cb = [&](HTTPErrorCode err, const std::string &err_msg,
                     const AsyncHTTPClient::Response &response) {
    if (err != OK) {
      std::cout << "HTTP request failed: " << err_msg << std::endl;
    } else {
      std::cout << "Code: " << response.code << std::endl;
      for (auto &header : response.headers) {
        std::cout << "Header: " << header.first << ": " << header.second << std::endl;
      }
      if (!response.body.empty()) {
        std::cout << "Body: " << response.body.data() << std::endl;
      }
    }
    p.set_value();
  };

  AsyncHTTPClient::PostRequest req{"10.199.1.14:8080/test/post", http_cb};
  req.add_form_param("a", "b");
  req.add_form_param("a+c", "b=d");
  req.add_header("test", "value");
  client->post(req);
  f.get();
  usleep(1000000);
}

TEST_F(HTTPClientTest, PostBody) {
  std::promise<void> p;
  std::future<void> f = p.get_future();
  auto http_cb = [&](HTTPErrorCode err, const std::string &err_msg,
                     const AsyncHTTPClient::Response &response) {
    if (err != OK) {
      std::cout << "HTTP request failed: " << err_msg << std::endl;
    } else {
      std::cout << "Code: " << response.code << std::endl;
      for (auto &header : response.headers) {
        std::cout << "Header: " << header.first << ": " << header.second << std::endl;
      }
      if (!response.body.empty()) {
        std::cout << "Body: " << response.body.data() << std::endl;
      }
    }
    p.set_value();
  };

  std::vector<char> body = {'a', 'b', 'c', 'd', 'e'};
  AsyncHTTPClient::PostRequest req{"10.199.1.14:8080/test/post", http_cb};
  req.set_body(std::move(body));
  req.add_header("test", "value");
  client->post(req);
  f.get();
  usleep(1000000);
}

TEST_F(HTTPClientTest, PostEmpty) {
  std::promise<void> p;
  std::future<void> f = p.get_future();
  auto http_cb = [&](HTTPErrorCode err, const std::string &err_msg,
                     const AsyncHTTPClient::Response &response) {
    if (err != OK) {
      std::cout << "HTTP request failed: " << err_msg << std::endl;
    } else {
      std::cout << "Code: " << response.code << std::endl;
      for (auto &header : response.headers) {
        std::cout << "Header: " << header.first << ": " << header.second << std::endl;
      }
      if (!response.body.empty()) {
        std::cout << "Body: " << response.body.data() << std::endl;
      }
    }
    p.set_value();
  };

  AsyncHTTPClient::PostRequest req{"10.199.1.14:8080/test/post", http_cb};
  req.add_header("test", "value");
  client->post(req);
  f.get();
  usleep(1000000);
}
