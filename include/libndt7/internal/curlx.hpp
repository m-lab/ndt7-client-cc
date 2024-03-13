// Part of Measurement Lab <https://www.measurementlab.net/>.
// Measurement Lab libndt7 is free software under the BSD license. See AUTHORS
// and LICENSE for more information on the copying conditions.
#ifndef MEASUREMENTLAB_LIBNDT7_INTERNAL_CURLX_HPP
#define MEASUREMENTLAB_LIBNDT7_INTERNAL_CURLX_HPP

// libndt7/internal/curlx.hpp - libcurl wrappers

#include <curl/curl.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <sstream>

#ifndef LIBNDT7_SINGLE_INCLUDE
#include "libndt7/internal/assert.hpp"
#include "libndt7/internal/logger.hpp"
#endif

namespace measurementlab {
namespace libndt7 {
namespace internal {

// CurlDeleter is a deleter for a libcurl handle.
class CurlDeleter {
 public:
  void operator()(CURL *handle) noexcept;
};

// UniqueCurl is a unique libcurl handle.
using UniqueCurl = std::unique_ptr<CURL, CurlDeleter>;

// CurlWriteCb is the signature of the callback used by curl.
using CurlWriteCb = size_t (*)(char *ptr, size_t size, size_t nmemb,
                               void *userdata);

// Curlx allows to emulate failures in libcurl code.
class Curlx {
 public:
  explicit Curlx(const Logger &logger) noexcept;

  explicit Curlx(const Logger &logger, const std::string &agent) noexcept;

  virtual bool GetMaybeSOCKS5(const std::string &proxy_port,
                              const std::string &url, long timeout,
                              std::string *body) noexcept;

  virtual bool Get(UniqueCurl &handle, const std::string &url, long timeout,
                   std::string *body) noexcept;

  virtual CURLcode SetoptURL(UniqueCurl &handle,
                             const std::string &url) noexcept;

  virtual CURLcode SetoptProxy(UniqueCurl &handle,
                               const std::string &url) noexcept;

  virtual CURLcode SetoptWriteFunction(UniqueCurl &handle,
                                       CurlWriteCb callback) noexcept;

  virtual CURLcode SetoptUserAgent(UniqueCurl &handle,
                                   const std::string &agent) noexcept;

  virtual CURLcode SetoptWriteData(UniqueCurl &handle, void *pointer) noexcept;

  virtual CURLcode SetoptTimeout(UniqueCurl &handle, long timeout) noexcept;

  virtual CURLcode SetoptFailonerr(UniqueCurl &handle) noexcept;

  virtual CURLcode Perform(UniqueCurl &handle) noexcept;

  virtual UniqueCurl NewUniqueCurl() noexcept;

  virtual CURLcode GetinfoResponseCode(UniqueCurl &handle,
                                       long *response_code) noexcept;

  virtual ~Curlx() noexcept;

 private:
  const Logger &logger_;
  const std::string agent_;
};

}  // namespace internal
}  // namespace libndt7
}  // namespace measurementlab
extern "C" {

static size_t libndt7_curl_callback(char *ptr, size_t size, size_t nmemb,
                                    void *userdata) {
  // Note: I have this habit of using `<= 0` rather than `== 0` even for
  // unsigned numbers because that makes the check robust when there is a
  // refactoring in which the number later becomes signed. In this case
  // it's probably a bit redundant because it's a cURL API but I still like
  // to continue to use it to avoid losing the habit. Spelling this out
  // explicitly here such that it's clear why I am doing it.
  if (nmemb <= 0) {
    return 0;  // This means "no body"
  }
  if (size > SIZE_MAX / nmemb) {
    // Note: if size is zero we end up here because we already excluded with
    // the above check the case where nmemb is zero.
    LIBNDT7_ASSERT(false);
    return 0;
  }
  auto realsiz = size * nmemb;  // Overflow not possible (see above)
  auto ss = static_cast<std::stringstream *>(userdata);
  (*ss) << std::string{ptr, realsiz};
  // From fwrite(3): "[the return value] equals the number of bytes
  // written _only_ when `size` equals `1`".
  return nmemb;
}

}  // extern "C"
namespace measurementlab {
namespace libndt7 {
namespace internal {

void CurlDeleter::operator()(CURL *handle) noexcept {
  if (handle != nullptr) {
    curl_easy_cleanup(handle);
  }
}

Curlx::Curlx(const Logger &logger) noexcept : logger_{logger}, agent_{"default-ndt7-client-cc-agent"} {}

Curlx::Curlx(const Logger &logger, const std::string &agent) noexcept : logger_{logger}, agent_{agent} {}

bool Curlx::GetMaybeSOCKS5(const std::string &proxy_port,
                           const std::string &url, long timeout,
                           std::string *body) noexcept {
  auto handle = this->NewUniqueCurl();
  if (!handle) {
    LIBNDT7_LOGGER_WARNING(logger_, "curlx: cannot initialize cURL");
    return false;
  }
  if (!proxy_port.empty()) {
    std::stringstream ss;
    ss << "socks5h://127.0.0.1:" << proxy_port;
    if (this->SetoptProxy(handle, ss.str()) != CURLE_OK) {
      LIBNDT7_LOGGER_WARNING(logger_,
                             "curlx: cannot configure proxy: " << ss.str());
      return false;
    }
  }
  return this->Get(handle, url, timeout, body);
}

bool Curlx::Get(UniqueCurl &handle, const std::string &url, long timeout,
                std::string *body) noexcept {
  if (body == nullptr) {
    LIBNDT7_LOGGER_WARNING(logger_, "curlx: passed a nullptr body");
    return false;
  }
  std::stringstream ss;
  if (this->SetoptURL(handle, url) != CURLE_OK) {
    LIBNDT7_LOGGER_WARNING(logger_, "curlx: cannot set URL: " << url);
    return false;
  }
  if (this->SetoptWriteFunction(handle, libndt7_curl_callback) != CURLE_OK) {
    LIBNDT7_LOGGER_WARNING(logger_, "curlx: cannot set callback function");
    return false;
  }
  if (this->SetoptWriteData(handle, &ss) != CURLE_OK) {
    LIBNDT7_LOGGER_WARNING(logger_,
                           "curlx: cannot set callback function context");
    return false;
  }
  if (this->SetoptUserAgent(handle, this->agent_) != CURLE_OK) {
    LIBNDT7_LOGGER_WARNING(logger_, "curlx: cannot set user agent");
    return false;
  }
  if (this->SetoptTimeout(handle, timeout) != CURLE_OK) {
    LIBNDT7_LOGGER_WARNING(logger_, "curlx: cannot set timeout");
    return false;
  }
  if (this->SetoptFailonerr(handle) != CURLE_OK) {
    LIBNDT7_LOGGER_WARNING(logger_, "curlx: cannot set fail-on-error option");
    return false;
  }
  LIBNDT7_LOGGER_DEBUG(logger_, "curlx: performing request");
  auto rv = this->Perform(handle);
  if (rv != CURLE_OK) {
    LIBNDT7_LOGGER_WARNING(logger_,
                           "curlx: cURL failed: " << curl_easy_strerror(rv));
    return false;
  }
  long response_code = 0L;
  if (this->GetinfoResponseCode(handle, &response_code) != 0) {
    LIBNDT7_LOGGER_WARNING(logger_, "curlx: cannot get the response code");
    return false;
  }
  if (response_code == 204) {
    LIBNDT7_LOGGER_WARNING(logger_, "curlx: mlab-ns is out of capacity");
    return false;
  }
  if (response_code != 200) {
    LIBNDT7_LOGGER_WARNING(logger_, "curlx: unexpected mlab-ns response");
    return false;
  }
  LIBNDT7_LOGGER_DEBUG(logger_, "curlx: request complete");
  *body = ss.str();
  return true;
}

CURLcode Curlx::SetoptURL(UniqueCurl &handle, const std::string &url) noexcept {
  LIBNDT7_ASSERT(handle);
  return ::curl_easy_setopt(handle.get(), CURLOPT_URL, url.c_str());
}

CURLcode Curlx::SetoptProxy(UniqueCurl &handle,
                            const std::string &url) noexcept {
  LIBNDT7_ASSERT(handle);
  return ::curl_easy_setopt(handle.get(), CURLOPT_PROXY, url.c_str());
}

CURLcode Curlx::SetoptWriteFunction(UniqueCurl &handle,
                                    CurlWriteCb callback) noexcept {
  LIBNDT7_ASSERT(handle);
  return ::curl_easy_setopt(handle.get(), CURLOPT_WRITEFUNCTION, callback);
}

CURLcode Curlx::SetoptUserAgent(UniqueCurl &handle,
                                const std::string &agent) noexcept {
  LIBNDT7_ASSERT(handle);
  return ::curl_easy_setopt(handle.get(), CURLOPT_USERAGENT, agent.c_str());
}

CURLcode Curlx::SetoptWriteData(UniqueCurl &handle, void *pointer) noexcept {
  LIBNDT7_ASSERT(handle);
  return ::curl_easy_setopt(handle.get(), CURLOPT_WRITEDATA, pointer);
}

CURLcode Curlx::SetoptTimeout(UniqueCurl &handle, long timeout) noexcept {
  LIBNDT7_ASSERT(handle);
  return ::curl_easy_setopt(handle.get(), CURLOPT_TIMEOUT, timeout);
}

CURLcode Curlx::SetoptFailonerr(UniqueCurl &handle) noexcept {
  LIBNDT7_ASSERT(handle);
  return ::curl_easy_setopt(handle.get(), CURLOPT_FAILONERROR, 1L);
}

CURLcode Curlx::Perform(UniqueCurl &handle) noexcept {
  LIBNDT7_ASSERT(handle);
  return ::curl_easy_perform(handle.get());
}

UniqueCurl Curlx::NewUniqueCurl() noexcept {
  return UniqueCurl{::curl_easy_init()};
}

CURLcode Curlx::GetinfoResponseCode(UniqueCurl &handle,
                                    long *response_code) noexcept {
  LIBNDT7_ASSERT(handle);
  LIBNDT7_ASSERT(response_code);
  return ::curl_easy_getinfo(handle.get(), CURLINFO_RESPONSE_CODE,
                             response_code);
}

Curlx::~Curlx() noexcept {}

}  // namespace internal
}  // namespace libndt7
}  // namespace measurementlab
#endif
