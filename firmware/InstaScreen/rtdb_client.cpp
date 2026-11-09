#include "rtdb_client.h"

#include "config.h"
#include "root_ca.h"

RtdbClient::RtdbClient(const char* host, const char* auth) : host_(host), auth_(auth) {
  while (host_.endsWith("/")) host_.remove(host_.length() - 1);
}

void RtdbClient::begin(bool verifyTls) {
  if (verifyTls) {
    tls_.setCACert(kFirebaseRootCAs);
  } else {
    Serial.println(F("[tls] WARNING: certificate verification disabled"));
    tls_.setInsecure();
  }
  http_.setReuse(true);
  http_.setConnectTimeout(cfg::kHttpConnectTimeoutMs);
  http_.setTimeout(cfg::kHttpReadTimeoutMs);
}

String RtdbClient::url(const String& path) const {
  String u;
  u.reserve(host_.length() + path.length() + auth_.length() + 16);
  u += host_;
  u += '/';
  u += path;
  u += ".json";
  if (auth_.length()) {
    u += "?auth=";
    u += auth_;
  }
  return u;
}

RtdbClient::Status RtdbClient::get(const String& path, String& body, size_t maxBodyBytes) {
  body = String();
  lastHttpCode_ = 0;

  if (!http_.begin(tls_, url(path))) {
    reset();
    return Status::Transport;
  }

  const int code = http_.GET();
  lastHttpCode_ = code;

  Status status;
  if (code < 0) {
    status = Status::Transport;
  } else if (code != HTTP_CODE_OK) {
    status = Status::HttpError;
  } else {
    const int len = http_.getSize();  // -1 when the server streams without Content-Length
    if (len > 0 && static_cast<size_t>(len) > maxBodyBytes) {
      status = Status::TooLarge;
    } else {
      body = http_.getString();
      if (body.length() == 0 && len != 0) {
        status = Status::Transport;  // read timed out / connection dropped mid-body
      } else if (body.length() > maxBodyBytes) {
        status = Status::TooLarge;
        body = String();
      } else {
        status = (body == "null") ? Status::Null : Status::Ok;
      }
    }
  }

  // end() keeps the socket open for reuse when the response was fully read;
  // on errors, drop it so the next request starts from a clean connection.
  http_.end();
  if (status == Status::Transport || status == Status::TooLarge) reset();
  return status;
}

void RtdbClient::reset() {
  http_.end();
  tls_.stop();
}

const char* toString(RtdbClient::Status s) {
  switch (s) {
    case RtdbClient::Status::Ok: return "ok";
    case RtdbClient::Status::Null: return "null";
    case RtdbClient::Status::TooLarge: return "too-large";
    case RtdbClient::Status::HttpError: return "http-error";
    case RtdbClient::Status::Transport: return "transport";
  }
  return "?";
}
