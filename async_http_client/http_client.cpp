#include <cassert>
#include <cstring>
#include <vector>

#include "http_client.h"

namespace async_http_client {

class Sock {
public:
  curl_socket_t sockfd{};
  struct ev_io ev {};
  int ev_set{};
};

class CurlSession {
public:
  std::shared_ptr<AsyncHTTPClient> holder = nullptr;

  CURL *easy = nullptr;
  char *url = nullptr;
  char error[CURL_ERROR_SIZE]{};

  std::vector<char> request_body{};

  std::map<std::string, std::string, HeaderComparator> response_header{};
  size_t current_body_size = 0;
  std::vector<char> response_body{};

  std::function<void(HTTPErrorCode err, const std::string &err_msg,
                     const AsyncHTTPClient::Response &resp)>
      cb = nullptr;
};

static void timer_cb(struct ev_loop *l, struct ev_timer *w, int revents);

static void check_multi_info(ClientInfo *info);

static void add_sock(ClientInfo *info, curl_socket_t s, CURL *easy, int action);

static void set_sock(ClientInfo *info, Sock *sock, curl_socket_t s, CURL *e, int action);

static void rm_sock(ClientInfo *info, Sock *sock);

static int multi_timer_cb(CURLM *multi, long timeout_ms, ClientInfo *info) {
  ev_timer_stop(info->loop, &info->timer_event);
  if (timeout_ms >= 0) {
    /* -1 means delete, other values are timeout times in milliseconds */
    double t = (double)timeout_ms / 1000;
    ev_timer_init(&info->timer_event, timer_cb, t, 0.);
    ev_timer_start(info->loop, &info->timer_event);
  }
  return 0;
}

static void timer_cb(struct ev_loop *l, struct ev_timer *w, int revents) {
  auto *info = (ClientInfo *)w->data;
  int sr;
  curl_multi_socket_action(info->multi, CURL_SOCKET_TIMEOUT, 0, &sr);
  check_multi_info(info);
}

static void event_cb(struct ev_loop *l, struct ev_io *w, int revents) {
  auto *info = (ClientInfo *)w->data;
  int action = (revents & EV_READ ? CURL_POLL_IN : 0) | (revents & EV_WRITE ? CURL_POLL_OUT : 0);
  int sr;
  curl_multi_socket_action(info->multi, w->fd, action, &sr);
  check_multi_info(info);
  if (sr <= 0) {
    ev_timer_stop(info->loop, &info->timer_event);
  }
}

static int sock_cb(CURL *e, curl_socket_t s, int what, void *cbp, void *sockp) {
  auto *info = (ClientInfo *)cbp;
  auto *sock = (Sock *)sockp;
  static const char *whatstr[] = {"none", "IN", "OUT", "INOUT", "REMOVE"};
  if (what == CURL_POLL_REMOVE) {
    rm_sock(info, sock);
  } else {
    if (!sock) {
      add_sock(info, s, e, what);
    } else {
      set_sock(info, sock, s, e, what);
    }
  }
  return 0;
}

static size_t header_cb(void *ptr, size_t size, size_t nmemb, void *data) {
  size_t real_size = size * nmemb;
  if (real_size <= 2) {
    return real_size;
  }
  auto *session = (CurlSession *)data;
  auto c_ptr = (char *)ptr;
  std::string key, value;
  bool key_area = true;
  for (size_t i = 0; i < real_size; ++i) {
    auto c = c_ptr[i];
    if (key_area && c_ptr[i] == ':') {
      key_area = false;
      if (i++ >= real_size) {
        break;
      } else {
        c = c_ptr[i];
        while (c == ' ') {
          c = c_ptr[++i];
        }
      }
    }
    if (key_area) {
      key += c;
    } else {
      value += c;
    }
  }
  if (key_area) {
    return real_size;
  }
  size_t found = value.rfind("\r\n");
  value = value.substr(0, found);
  session->response_header[key] = value;
  return real_size;
}

static size_t write_cb(void *ptr, size_t size, size_t nmemb, void *data) {
  //    INFO("Body callback");
  size_t part_size = size * nmemb;
  if (part_size == 0) {
    return part_size;
  }
  auto *session = (CurlSession *)data;
  size_t new_size = session->current_body_size + part_size;
  if (session->response_body.size() < new_size) {
    session->response_body.resize(new_size * 1.5);
  }
  std::memcpy(&session->response_body[session->current_body_size], ptr, part_size);
  session->current_body_size += part_size;
  return part_size;
}

static void check_multi_info(ClientInfo *info) {
  CurlSession *session;
  int msgs_left;
  CURL *easy;
  CURLMsg *msg;
  CURLcode res;
  long response_code;
  long connect_time;
  long total_time;

  while ((msg = curl_multi_info_read(info->multi, &msgs_left))) {
    if (msg->msg == CURLMSG_DONE) {
      easy = msg->easy_handle;
      res = msg->data.result;
      curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &response_code);
      curl_easy_getinfo(easy, CURLINFO_CONNECT_TIME_T, &connect_time);
      curl_easy_getinfo(easy, CURLINFO_TOTAL_TIME_T, &total_time);
      curl_easy_getinfo(easy, CURLINFO_PRIVATE, &session);
      curl_multi_remove_handle(info->multi, easy);
      curl_easy_cleanup(easy);
      info->executor->submit([=]() {
        AsyncHTTPClient::Response resp{};
        if (res != CURLE_OK) {
          session->cb(HTTPRequestError, session->error, resp);
        } else {
          resp.code = response_code;
          session->response_body.resize(session->current_body_size);
          resp.body = std::move(session->response_body);
          resp.headers = std::move(session->response_header);
          session->cb(OK, "", resp);
        }
        free(session->url);
        delete session;
      });
    }
  }
}

static void set_sock(ClientInfo *info, Sock *sock, curl_socket_t s, CURL *e, int action) {
  int kind = (action & CURL_POLL_IN ? EV_READ : 0) | (action & CURL_POLL_OUT ? EV_WRITE : 0);
  sock->sockfd = s;
  if (sock->ev_set) {
    ev_io_stop(info->loop, &sock->ev);
  }
  ev_io_init(&sock->ev, event_cb, sock->sockfd, kind);
  sock->ev.data = info;
  sock->ev_set = 1;
  ev_io_start(info->loop, &sock->ev);
}

static void add_sock(ClientInfo *info, curl_socket_t s, CURL *easy, int action) {
  auto *sock = new Sock;
  set_sock(info, sock, s, easy, action);
  curl_multi_assign(info->multi, s, sock);
}

static void rm_sock(ClientInfo *info, Sock *sock) {
  if (sock) {
    if (sock->ev_set) {
      ev_io_stop(info->loop, &sock->ev);
    }
    delete sock;
  }
}

AsyncHTTPClient::Request::Request(
    const std::string &url,
    const std::function<void(HTTPErrorCode, const std::string &, const Response &)> &cb)
    : m_url(url), m_cb(cb) {
  assert(!m_url.empty());
  assert(m_cb != nullptr);
}

AsyncHTTPClient::AsyncHTTPClient(const AsyncHTTPClient::Setting &setting) : m_setting(setting) {
  assert(setting.executor != nullptr);
  m_info.loop = ev_loop_new(0);
  m_info.executor = m_setting.executor;
  ev_work_init();
  m_worker = std::thread(&AsyncHTTPClient::ev_work, this);
  ev_timer_init(&m_info.timer_event, timer_cb, 100, 0.);
  m_info.timer_event.data = &m_info;
  m_info.multi = curl_multi_init();
  curl_multi_setopt(m_info.multi, CURLMOPT_MAXCONNECTS, m_setting.max_connections);
  curl_multi_setopt(m_info.multi, CURLMOPT_MAX_HOST_CONNECTIONS,
                    m_setting.max_conncetions_per_host);
  curl_multi_setopt(m_info.multi, CURLMOPT_SOCKETFUNCTION, sock_cb);
  curl_multi_setopt(m_info.multi, CURLMOPT_SOCKETDATA, &m_info);
  curl_multi_setopt(m_info.multi, CURLMOPT_TIMERFUNCTION, multi_timer_cb);
  curl_multi_setopt(m_info.multi, CURLMOPT_TIMERDATA, &m_info);
}

AsyncHTTPClient::~AsyncHTTPClient() {
  curl_multi_cleanup(m_info.multi);
  m_running = false;
  m_worker.join();
}

void AsyncHTTPClient::get(AsyncHTTPClient::GetRequest &request) {
  request.m_method = GET;
  if (request.m_url[request.m_url.size() - 1] != '?') {
    request.m_url += '?';
  }
  bool first = true;
  for (auto &entry : request.m_url_params) {
    char *ek = curl_escape(entry.first.data(), entry.first.size());
    char *ev = curl_escape(entry.second.data(), entry.second.size());
    if (first) {
      first = false;
    } else {
      request.m_url += '&';
    }
    request.m_url += ek;
    request.m_url += '=';
    request.m_url += ev;
    curl_free(ek);
    curl_free(ev);
  }
  do_request(request);
}

void AsyncHTTPClient::post(AsyncHTTPClient::PostRequest &request) {
  request.m_method = POST;

  if (!request.m_body.empty()) {
    assert(request.m_form_params.empty());
  }

  if (!request.m_form_params.empty()) {
    assert(request.m_body.empty());
    bool first_pair = true;
    for (auto &form_param : request.m_form_params) {
      char *ek = curl_escape(form_param.first.data(), form_param.first.size());
      char *ev = curl_escape(form_param.second.data(), form_param.second.size());
      size_t ek_size = strlen(ek);
      size_t ev_size = strlen(ev);
      if (first_pair) {
        first_pair = false;
      } else {
        request.m_body.emplace_back('&');
      }
      std::copy(ek, ek + ek_size, std::back_inserter(request.m_body));
      request.m_body.emplace_back('=');
      std::copy(ev, ev + ev_size, std::back_inserter(request.m_body));
      curl_free(ek);
      curl_free(ev);
    }
  }
  do_request(request);
}

HTTPErrorCode AsyncHTTPClient::Request::add_header(const std::string &key,
                                                   const std::string &value) {
  m_headers[key] = value;
}

HTTPErrorCode AsyncHTTPClient::GetRequest::add_url_param(const std::string &key,
                                                         const std::string &value) {
  m_url_params[key] = value;
}

HTTPErrorCode AsyncHTTPClient::PostRequest::add_form_param(const std::string &key,
                                                           const std::string &value) {
  m_form_params[key] = value;
}

HTTPErrorCode AsyncHTTPClient::PostRequest::set_body(std::vector<char> &&body) {
  m_body = std::move(body);
}

void AsyncHTTPClient::do_request(Request &request) {
  auto *session = new CurlSession;
  session->url = strdup(request.m_url.data());
  session->holder = shared_from_this();
  session->cb = request.m_cb;
  session->error[0] = '\0';
  session->easy = curl_easy_init();
  session->request_body = std::move(request.m_body);
  assert(session->easy != nullptr);
  struct curl_slist *chunk = nullptr;
  chunk = curl_slist_append(chunk, "Expect:");
  for (auto &header : request.m_headers) {
    std::string header_str = header.first + ": " + header.second;
    chunk = curl_slist_append(chunk, header_str.data());
  }
  curl_easy_setopt(session->easy, CURLOPT_HTTPHEADER, chunk);
  curl_easy_setopt(session->easy, CURLOPT_URL, session->url);
  curl_easy_setopt(session->easy, CURLOPT_WRITEFUNCTION, write_cb);
  curl_easy_setopt(session->easy, CURLOPT_WRITEDATA, session);
  curl_easy_setopt(session->easy, CURLOPT_HEADERFUNCTION, header_cb);
  curl_easy_setopt(session->easy, CURLOPT_HEADERDATA, session);
  curl_easy_setopt(session->easy, CURLOPT_VERBOSE, m_setting.debug ? 1L : 0L);
  curl_easy_setopt(session->easy, CURLOPT_ERRORBUFFER, session->error);
  curl_easy_setopt(session->easy, CURLOPT_PRIVATE, session);
  curl_easy_setopt(session->easy, CURLOPT_NOPROGRESS, 1L);
  curl_easy_setopt(session->easy, CURLOPT_CONNECTTIMEOUT_MS, m_setting.connection_timeout);
  curl_easy_setopt(session->easy, CURLOPT_TIMEOUT_MS, m_setting.request_timeout);
  curl_easy_setopt(session->easy, CURLOPT_KEEP_SENDING_ON_ERROR, 1L);
  if (request.m_method == POST) {
    curl_easy_setopt(session->easy, CURLOPT_POSTFIELDSIZE, session->request_body.size());
    curl_easy_setopt(session->easy, CURLOPT_POSTFIELDS, session->request_body.data());
  }
  curl_multi_add_handle(m_info.multi, session->easy);
}

static void generator_cb(EV_P_ struct ev_timer *w, int revents) {
  bool running = *(bool *)w->data;
  if (!running) {
    ev_break(loop, EVBREAK_ALL);
  }
}

void AsyncHTTPClient::ev_work_init() {
  ev_init(&request_generator, generator_cb);
  request_generator.repeat = 1;
  request_generator.data = &m_running;
  ev_timer_again(m_info.loop, &request_generator);
}

void AsyncHTTPClient::ev_work() {
  ev_loop(m_info.loop, 0);
}

} // namespace async_http_client
