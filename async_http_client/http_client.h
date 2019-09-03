#pragma once

#include <cstring>
#include <curl/curl.h>
#include <ev.h>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>
#include <algorithm>
#include <map>

#include "conlib/concurrent/executor.h"

namespace async_http_client {

class ClientInfo {
public:
  struct ev_loop *loop = nullptr;
  struct ev_timer timer_event {};
  std::shared_ptr<conlib::Executor> executor = nullptr;
  CURLM *multi = nullptr;
};

enum HTTPErrorCode { OK, HTTPRequestError };

enum HTTPRequestMethod {
  GET,
  POST,
  HEAD,
  DELETE,
};

struct HeaderComparator {
    bool operator() (const std::string& s1, const std::string& s2) const {
      std::string str1(s1.length(),' ');
      std::string str2(s2.length(),' ');
      std::transform(s1.begin(), s1.end(), str1.begin(), tolower);
      std::transform(s2.begin(), s2.end(), str2.begin(), tolower);
      return  str1 < str2;
    }
};

class AsyncHTTPClient : public std::enable_shared_from_this<AsyncHTTPClient> {
public:
  class Setting {
  public:
    size_t max_connections = 5;
    size_t max_conncetions_per_host = 0;
    size_t connection_timeout = 500;
    size_t request_timeout = 3000;

    std::shared_ptr<conlib::Executor> executor = nullptr;

    bool debug = false;
  };

  class Response {
  public:
    long code{};
    std::map<std::string, std::string, HeaderComparator> headers{};
    std::vector<char> body{};
  };

  class Request {
  public:
    friend class AsyncHTTPClient;

    Request(const std::string &url,
            const std::function<void(HTTPErrorCode, const std::string &, const Response &)> &cb);

    HTTPErrorCode add_header(const std::string &key, const std::string &value);

  protected:
    HTTPRequestMethod m_method{};
    std::string m_url{};
    std::map<std::string, std::string, HeaderComparator> m_headers{};
    std::unordered_map<std::string, std::string> m_url_params{};
    std::vector<char> m_body{};
    std::unordered_map<std::string, std::string> m_form_params{};
    std::function<void(HTTPErrorCode err, const std::string &, const Response &)> m_cb = nullptr;
  };

  class GetRequest : public Request {
  public:
    using Request::Request;

    HTTPErrorCode add_url_param(const std::string &key, const std::string &value);
  };

  class PostRequest : public Request {
  public:
    using Request::Request;

    HTTPErrorCode add_form_param(const std::string &key, const std::string &value);

    HTTPErrorCode set_body(std::vector<char> &&body);
  };

  explicit AsyncHTTPClient(const Setting &setting);

  virtual ~AsyncHTTPClient();

  void get(GetRequest &request);

  void post(PostRequest &request);

private:
  Setting m_setting{};
  ClientInfo m_info{};
  std::thread m_worker{};
  bool m_running{true};

  void do_request(Request &request);

  void ev_work();
};

} // namespace async_http_client
