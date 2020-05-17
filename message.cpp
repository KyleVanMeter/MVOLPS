#include "message.h"
#include "spdlog/spdlog.h"
#include <string>
#include <variant>
// std::stringstream
#include <sstream>

using namespace MVOLP;

void LogDispatch::write() const { spdlog::info(_msg); }

template <class... Args> void LogDispatch::write(Args &&... args) {
  spdlog::info(sstr(args...));
}

LogDispatch *LogDispatch::message(std::string in) {
  _msg = in;
  return this;
}

void DebugDispatch::write() const { spdlog::debug(_msg); }

template <class... Args> void DebugDispatch::write(Args &&... args) {
  spdlog::debug(sstr(args...));
}

DebugDispatch *DebugDispatch::message(std::string in) {
  _msg = in;
  return this;
}

void IPCDispatch::write() const {
  using MessageVariant =
      std::variant<BranchMessagePOD, CandMessagePOD, FathMessagePOD,
                   HeurMessagePOD, InfeasMessagePOD, PregMessagePOD,
                   InteMessagePOD>;
  if (!_startServer) {
    return;
  }

  if (!baseFields.has_value()) {
    throw std::invalid_argument("BaseMessagePOD has incomplete information");
  }

  // Collect data in correct type, DNF form logic
  auto result = [&]() -> MessageVariant {
    MessageVariant ret;
    switch (baseFields.value().nodeType) {
    case EventType::branched:
      // All fields must be present
      if (field6.has_value() && field7.has_value() && field8.has_value() &&
          field9.has_value() && field10.has_value() && baseFields.has_value()) {
        // Named initialization is not possible on non-aggregate types.  Any
        // object that has a base class (even if POD) is categorically excluded
        // from being an aggregate type.  WHY???
        BranchMessagePOD wtf;
        wtf.baseFields.timeSpan = this->baseFields.value().timeSpan;
        wtf.baseFields.nodeType = this->baseFields.value().nodeType;
        wtf.baseFields.oid = this->baseFields.value().oid;
        wtf.baseFields.pid = this->baseFields.value().pid;
        wtf.baseFields.direction = this->baseFields.value().direction;
        wtf.LPBound = field6.value();
        wtf.infeasCount = field7.value();
        wtf.violatedCount = field8.value();
        wtf.bCond = field9.value();
        wtf.eCond = field10.value();

        ret = wtf;
        return ret;
      }
      break;
    case EventType::candidate:
      // Field 6 must be present
      if (field6.has_value() && !field7.has_value() && !field8.has_value() &&
          !field9.has_value() && !field10.has_value() &&
          baseFields.has_value()) {

        CandMessagePOD wtf;
        wtf.baseFields.timeSpan = this->baseFields.value().timeSpan;
        wtf.baseFields.nodeType = this->baseFields.value().nodeType;
        wtf.baseFields.oid = this->baseFields.value().oid;
        wtf.baseFields.pid = this->baseFields.value().pid;
        wtf.baseFields.direction = this->baseFields.value().direction;
        wtf.LPBound = field6.value();

        ret = wtf;
        return ret;
      }
      break;
    case EventType::fathomed:
      // No additional fields
      if (!field6.has_value() && !field7.has_value() && !field8.has_value() &&
          !field9.has_value() && !field10.has_value() &&
          baseFields.has_value()) {
        FathMessagePOD wtf;
        wtf.baseFields.timeSpan = this->baseFields.value().timeSpan;
        wtf.baseFields.nodeType = this->baseFields.value().nodeType;
        wtf.baseFields.oid = this->baseFields.value().oid;
        wtf.baseFields.pid = this->baseFields.value().pid;
        wtf.baseFields.direction = this->baseFields.value().direction;

        ret = wtf;
        return ret;
      }
      break;
    case EventType::heuristic:
      // Not used yet!
      throw std::invalid_argument("Heuristic message not implemented");
    case EventType::infeasible:
      // Field 9 & field 10 must be present
      if (!field6.has_value() && !field7.has_value() && !field8.has_value() &&
          field9.has_value() && field10.has_value() && baseFields.has_value()) {
        InfeasMessagePOD wtf;

        wtf.baseFields.timeSpan = this->baseFields.value().timeSpan;
        wtf.baseFields.nodeType = this->baseFields.value().nodeType;
        wtf.baseFields.oid = this->baseFields.value().oid;
        wtf.baseFields.pid = this->baseFields.value().pid;
        wtf.baseFields.direction = this->baseFields.value().direction;
        wtf.bCond = field9.value();
        wtf.eCond = field10.value();

        ret = wtf;
        return ret;
      }
      break;
    case EventType::pregnant:
      // Field 6, field 9, and field 10 must be present
      if (field6.has_value() && !field7.has_value() && !field8.has_value() &&
          field9.has_value() && field10.has_value() && baseFields.has_value()) {
        PregMessagePOD wtf;

        wtf.baseFields.timeSpan = this->baseFields.value().timeSpan;
        wtf.baseFields.nodeType = this->baseFields.value().nodeType;
        wtf.baseFields.oid = this->baseFields.value().oid;
        wtf.baseFields.pid = this->baseFields.value().pid;
        wtf.baseFields.direction = this->baseFields.value().direction;
        wtf.LPBound = field6.value();
        wtf.bCond = field9.value();
        wtf.eCond = field10.value();

        ret = wtf;
        return ret;
      }
      break;
    case EventType::integer:
      // Field 6 must be present
      if (field6.has_value() && !field7.has_value() && !field8.has_value() &&
          !field9.has_value() && !field10.has_value() && baseFields.has_value()) {
        InteMessagePOD wtf;

        wtf.baseFields.timeSpan = this->baseFields.value().timeSpan;
        wtf.baseFields.nodeType = this->baseFields.value().nodeType;
        wtf.baseFields.oid = this->baseFields.value().oid;
        wtf.baseFields.pid = this->baseFields.value().pid;
        wtf.baseFields.direction = this->baseFields.value().direction;
        wtf.LPBound = field6.value();

        ret = wtf;
        return ret;
      }
      break;
    default:
      throw std::invalid_argument("IPCDispatch EventType invalid type");
    };
    throw std::logic_error(
        "IPCDispatch invalid arguments to specific message constructor");
  }();

  std::stringstream msg;
  std::visit([&msg](auto &&arg) { msg << arg << "\n"; }, result);

  /*
   * As we have overloaded operator<< for each messagePOD type having the base
   * message as a member of the derived types will cause operator<< to be called
   * twice.  Since there is a trailing " " this leads to an erroneous "  " being
   * placed in the middle of the message.  To avoid refactoring we can just to
   * string replacement from "  " to " "
   */
  std::string sMsg = msg.str();
  sMsg.replace(sMsg.find("  "), 1, "");

  // Send zmq message to client
  zmqpp::message message;
  _socket->receive(message);
  std::string txt;
  message >> txt;
  DebugDispatch::write("Received from client: ", txt);

  _socket->send(sMsg);
}

// Type definitions for field-helper tuples.  Needed for metaprogramming
decltype(BaseMessagePOD::typeInfo)
    BaseMessagePOD::typeInfo(&BaseMessagePOD::timeSpan,
                             &BaseMessagePOD::nodeType, &BaseMessagePOD::oid,
                             &BaseMessagePOD::pid, &BaseMessagePOD::direction);

decltype(HeurMessagePOD::typeInfo)
    HeurMessagePOD::typeInfo(&HeurMessagePOD::baseFields,
                             &HeurMessagePOD::value);

decltype(InfeasMessagePOD::typeInfo)
    InfeasMessagePOD::typeInfo(&InfeasMessagePOD::baseFields,
                               &InfeasMessagePOD::bCond,
                               &InfeasMessagePOD::eCond);

decltype(BranchMessagePOD::typeInfo) BranchMessagePOD::typeInfo(
    &BranchMessagePOD::baseFields, &BranchMessagePOD::LPBound,
    &BranchMessagePOD::infeasCount, &BranchMessagePOD::violatedCount,
    &BranchMessagePOD::bCond, &BranchMessagePOD::eCond);

decltype(CandMessagePOD::typeInfo)
    CandMessagePOD::typeInfo(&CandMessagePOD::baseFields,
                             &CandMessagePOD::LPBound);

decltype(PregMessagePOD::typeInfo)
    PregMessagePOD::typeInfo(&PregMessagePOD::baseFields,
                             &PregMessagePOD::LPBound, &PregMessagePOD::bCond,
                             &PregMessagePOD::eCond);

decltype(InteMessagePOD::typeInfo)
    InteMessagePOD::typeInfo(&InteMessagePOD::baseFields,
                             &InteMessagePOD::LPBound);

decltype(FathMessagePOD::typeInfo)
    FathMessagePOD::typeInfo(&FathMessagePOD::baseFields);
