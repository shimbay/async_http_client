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
      std::cout << "Body: " << response.body.data() << std::endl;
    }
    p.set_value();
  };

  AsyncHTTPClient::GetRequest req{"ipinfo.io", http_cb};
  client->get(req);
  f.get();
}
