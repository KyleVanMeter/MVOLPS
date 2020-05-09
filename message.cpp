#include "message.h"
#include "spdlog/spdlog.h"
#include <string>

using namespace MVOLP;

void LogDispatch::write() const {
  spdlog::info(_msg);
}

/*
template<class... Args>
LogDispatch* LogDispatch::message(Args&& ...args) {
  _msg = sstr(args...);
  return this;
}
*/
LogDispatch* LogDispatch::message(std::string in) {
  _msg = in;
  return this;
}

void DebugDispatch::write() const {
  spdlog::debug(_msg);
}

/*
template<class... Args>
DebugDispatch* DebugDispatch::message(Args&& ...args) {
  _msg = sstr(args...);
  return this;
}
*/
DebugDispatch* DebugDispatch::message(std::string in) {
  _msg = in;
  return this;
}
