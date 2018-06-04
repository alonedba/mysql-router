/*
  Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <event2/event.h>
#include <event2/http.h>

#include <string>
#include <iostream>

#include "mysqlrouter/http_client.h"
#include "http_request_impl.h"

class IOContext::impl {
public:
  std::unique_ptr<event_base, std::function<void(event_base *)>> ev_base { nullptr, event_base_free };

  impl() {
    ev_base.reset(event_base_new());
  }
};

IOContext::IOContext():
  pImpl{new impl()}
{
}

void IOContext::dispatch() {
  event_base_dispatch(pImpl->ev_base.get());
}

IOContext::~IOContext() = default;


class HttpClient::impl {
public:
  std::unique_ptr<evhttp_connection, std::function<void(evhttp_connection *)>> conn { nullptr, evhttp_connection_free };

  impl() = default;
};

HttpClient::HttpClient(IOContext &io_ctx, const std::string &address, uint16_t port):
  pImpl{new impl()},

  // gcc-4.8 requires a () here, instead of {}
  //
  // invalid initialization of non-const reference of type ‘IOContext&’ from an rvalue of type ‘<brace-enclosed initializer list>’
  io_ctx_(io_ctx)
{
  auto *ev_base = io_ctx_.pImpl->ev_base.get();
  pImpl->conn.reset(evhttp_connection_base_new(ev_base, NULL, address.c_str(), port));
}

void HttpClient::make_request(HttpRequest *req, HttpMethod::type method, const std::string &uri) {
  auto *ev_req = req->pImpl->req.get();

  evhttp_make_request(pImpl->conn.get(), ev_req,
      static_cast<enum evhttp_cmd_type>(method), uri.c_str());

  // don't free the evhttp_request() when HttpRequest gets destructed
  // as the eventloop will do it
  req->pImpl->disown();
};

void HttpClient::make_request_sync(HttpRequest *req, HttpMethod::type method, const std::string &uri) {
  make_request(req, method, uri);

  io_ctx_.dispatch();
};


HttpClient::~HttpClient() = default;
