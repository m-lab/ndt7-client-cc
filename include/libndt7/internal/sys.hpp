// Part of Measurement Lab <https://www.measurementlab.net/>.
// Measurement Lab libndt7 is free software under the BSD license. See AUTHORS
// and LICENSE for more information on the copying conditions.
#ifndef MEASUREMENTLAB_LIBNDT7_INTERNAL_SYS_HPP
#define MEASUREMENTLAB_LIBNDT7_INTERNAL_SYS_HPP

// libndt7/internal/sys.hpp - system dependent routines

#ifdef _WIN32
#include <winsock2.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

namespace measurementlab {
namespace libndt7 {
namespace internal {

// Size is our definition of size
using Size = uint64_t;

// SizeMax is the maximum value that Size could have
constexpr Size SizeMax = UINT64_MAX;

// Ssize is the signed size (like ssize_t)
using Ssize = int64_t;

// Socket is the definition of socket
#ifdef _WIN32
using Socket = SOCKET;
#else
using Socket = int;
#endif

// IsSocketValid tells you whether a socket is valid
constexpr bool IsSocketValid(Socket s) noexcept {
#ifdef _WIN32
  return s != INVALID_SOCKET;
#else
  return s >= 0;
#endif
}

// LIBNDT7_OS_EINVAL is a portable EINVAL
#ifdef _WIN32
#define LIBNDT7_OS_EINVAL WSAEINVAL
#else
#define LIBNDT7_OS_EINVAL EINVAL
#endif

// Sys allow to simulate failures in system dependencies.
class Sys {
 public:
  virtual int GetLastError() const noexcept;

  virtual void SetLastError(int err) const noexcept;

  virtual int Getaddrinfo(
      const char *domain, const char *port, const addrinfo *hints,
      addrinfo **res) const noexcept;

  virtual int Getnameinfo(
      const sockaddr *sa, socklen_t salen, char *host, socklen_t hostlen,
      char *serv, socklen_t servlen, int flags) const noexcept;

  virtual void Freeaddrinfo(addrinfo *aip) const noexcept;

  virtual Socket NewSocket(int domain, int type, int protocol) const noexcept;

  virtual int Connect(
      Socket fd, const sockaddr *sa, socklen_t n) const noexcept;

  virtual Ssize Recv(Socket fd, void *base, Size count) const noexcept;

  virtual Ssize Send(
      Socket fd, const void *base, Size count) const noexcept;

  virtual int Shutdown(Socket fd, int shutdown_how) const noexcept;

  virtual int Closesocket(Socket fd) const noexcept;

#ifdef _WIN32
  virtual int Poll(LPWSAPOLLFD fds, ULONG nfds, INT timeout) const noexcept;
#else
  virtual int Poll(pollfd *fds, nfds_t nfds, int timeout) const noexcept;
#endif

  virtual long long Strtonum(
      const char *s, long long minval, long long maxval,
      const char **err) const noexcept;

#ifdef _WIN32
  virtual int Ioctlsocket(Socket s, long cmd, u_long *argp) const noexcept;
#else
  virtual int Fcntl(Socket s, int cmd) const noexcept;
  virtual int Fcntl(Socket s, int cmd, int arg) const noexcept;
#endif

  virtual int Getsockopt(
      Socket socket, int level, int name, void *value,
      socklen_t *len) const noexcept;

  virtual ~Sys() noexcept;
};

// LIBNDT7_HAVE_STRTONUM tells us whether we have strtonum in libc
#ifndef LIBNDT7_HAVE_STRTONUM
// clang-format off
/*
 * Copyright (c) 2004 Ted Unangst and Todd Miller
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#define	INVALID		1
#define	TOOSMALL	2
#define	TOOLARGE	3

static long long
strtonum(const char *numstr, long long minval, long long maxval,
    const char **errstrp)
{
	long long ll = 0;
	int error = 0;
	char *ep;
	struct errval {
		const char *errstr;
		int err;
	} ev[4] = {
		{ NULL,		0 },
		{ "invalid",	EINVAL },
		{ "too small",	ERANGE },
		{ "too large",	ERANGE },
	};

	ev[0].err = errno;
	errno = 0;
	if (minval > maxval) {
		error = INVALID;
	} else {
		ll = strtoll(numstr, &ep, 10);
		if (numstr == ep || *ep != '\0')
			error = INVALID;
		else if ((ll == LLONG_MIN && errno == ERANGE) || ll < minval)
			error = TOOSMALL;
		else if ((ll == LLONG_MAX && errno == ERANGE) || ll > maxval)
			error = TOOLARGE;
	}
	if (errstrp != NULL)
		*errstrp = ev[error].errstr;
	errno = ev[error].err;
	if (error)
		ll = 0;

	return (ll);
}
// clang-format on
#endif  // LIBNDT7_HAVE_STRTONUM

#ifdef _WIN32
#define LIBNDT7_AS_OS_BUFFER(b) ((char *)b)
#define LIBNDT7_AS_OS_BUFFER_LEN(n) ((int)n)
#define LIBNDT7_OS_SSIZE_MAX INT_MAX
#define LIBNDT7_AS_OS_OPTION_VALUE(x) ((char *)x)
#else
#define LIBNDT7_AS_OS_BUFFER(b) ((char *)b)
#define LIBNDT7_AS_OS_BUFFER_LEN(n) ((size_t)n)
#define LIBNDT7_OS_SSIZE_MAX SSIZE_MAX
#define LIBNDT7_AS_OS_OPTION_VALUE(x) ((void *)x)
#endif

int Sys::GetLastError() const noexcept {
#ifdef _WIN32
  return ::GetLastError();
#else
  return errno;
#endif
}

void Sys::SetLastError(int err) const noexcept {
#ifdef _WIN32
  ::SetLastError(err);
#else
  errno = err;
#endif
}

int Sys::Getaddrinfo(const char *domain, const char *port,
                     const addrinfo *hints, addrinfo **res) const noexcept {
  return ::getaddrinfo(domain, port, hints, res);
}

int Sys::Getnameinfo(const sockaddr *sa, socklen_t salen, char *host,
                     socklen_t hostlen, char *serv, socklen_t servlen,
                     int flags) const noexcept {
  return ::getnameinfo(sa, salen, host, hostlen, serv, servlen, flags);
}

void Sys::Freeaddrinfo(addrinfo *aip) const noexcept { ::freeaddrinfo(aip); }

Socket Sys::NewSocket(int domain, int type, int protocol) const noexcept {
  return (Socket)::socket(domain, type, protocol);
}

int Sys::Connect(Socket fd, const sockaddr *sa, socklen_t len) const noexcept {
  return ::connect(fd, sa, len);
}

Ssize Sys::Recv(Socket fd, void *base, Size count) const noexcept {
  if (count > LIBNDT7_OS_SSIZE_MAX) {
    this->SetLastError(LIBNDT7_OS_EINVAL);
    return -1;
  }
  int flags = 0;
#ifdef MSG_NOSIGNAL
  // On Linux systems this flag prevents socket ops from raising SIGPIPE.
  flags |= MSG_NOSIGNAL;
#endif
  return (Ssize)::recv(
      fd, LIBNDT7_AS_OS_BUFFER(base), LIBNDT7_AS_OS_BUFFER_LEN(count), flags);
}

Ssize Sys::Send(Socket fd, const void *base, Size count) const noexcept {
  if (count > LIBNDT7_OS_SSIZE_MAX) {
    this->SetLastError(LIBNDT7_OS_EINVAL);
    return -1;
  }
  int flags = 0;
#ifdef MSG_NOSIGNAL
  // On Linux systems this flag prevents socket ops from raising SIGPIPE.
  flags |= MSG_NOSIGNAL;
#endif
  return (Ssize)::send(
      fd, LIBNDT7_AS_OS_BUFFER(base), LIBNDT7_AS_OS_BUFFER_LEN(count), flags);
}

int Sys::Shutdown(Socket fd, int shutdown_how) const noexcept {
  return ::shutdown(fd, shutdown_how);
}

int Sys::Closesocket(Socket fd) const noexcept {
#ifdef _WIN32
  return ::closesocket(fd);
#else
  return ::close(fd);
#endif
}

#ifdef _WIN32
int Sys::Poll(LPWSAPOLLFD fds, ULONG nfds, INT timeout) const noexcept {
  return ::WSAPoll(fds, nfds, timeout);
}
#else
int Sys::Poll(pollfd *fds, nfds_t nfds, int timeout) const noexcept {
  return ::poll(fds, nfds, timeout);
}
#endif

long long Sys::Strtonum(const char *s, long long minval,
                        long long maxval, const char **errp) const noexcept {
  return strtonum(s, minval, maxval, errp);
}

#ifdef _WIN32
int Sys::Ioctlsocket(Socket s, long cmd, u_long *argp) const noexcept {
  return ::ioctlsocket(s, cmd, argp);
}
#else
int Sys::Fcntl(Socket s, int cmd) const noexcept { return ::fcntl(s, cmd); }
int Sys::Fcntl(Socket s, int cmd, int arg) const noexcept {
  return ::fcntl(s, cmd, arg);
}
#endif

int Sys::Getsockopt(Socket socket, int level, int name, void *value,
                    socklen_t *len) const noexcept {
  return ::getsockopt(
      socket, level, name, LIBNDT7_AS_OS_OPTION_VALUE(value), len);
}

Sys::~Sys() noexcept {}

}  // namespace internal
}  // namespace libndt7
}  // namespace measurementlab
#endif  // MEASUREMENTLAB_LIBNDT7_INTERNAL_SYS_HPP
