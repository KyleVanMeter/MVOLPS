#include "message.h"
#include "spdlog/spdlog.h"
#include <string>
#include <variant>

using namespace MVOLP;

void LogDispatch::write() const { spdlog::info(_msg); }

/*
template<class... Args>
LogDispatch* LogDispatch::message(Args&& ...args) {
  _msg = sstr(args...);
  return this;
}
*/
LogDispatch *LogDispatch::message(std::string in) {
  _msg = in;
  return this;
}

void DebugDispatch::write() const { spdlog::debug(_msg); }

/*
template<class... Args>
DebugDispatch* DebugDispatch::message(Args&& ...args) {
  _msg = sstr(args...);
  return this;
}
*/
DebugDispatch *DebugDispatch::message(std::string in) {
  _msg = in;
  return this;
}

void IPCDispatch::write() const {
  using MessageVariant =
      std::variant<BranchMessagePOD, CandMessagePOD, FathMessagePOD,
                   HeurMessagePOD, InfeasMessagePOD, InteMessagePOD>;

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
        // EDIT: got rid of inheritance structure; nested structs instead.
        BranchMessagePOD wtf{.baseFields = {this->baseFields.value().timeSpan,
                                            this->baseFields.value().nodeType,
                                            this->baseFields.value().oid,
                                            this->baseFields.value().pid,
                                            this->baseFields.value().direction},
                             .LPBound = field6.value(),
                             .infeasCount = field7.value(),
                             .violatedCount = field8.value(),
                             .bCond = field9.value(),
                             .eCond = field10.value()};

        ret = wtf;
        return ret;
      }
      break;
    case EventType::candidate:
      // Field 6 must be present
      if (field6.has_value() && !field7.has_value() && !field8.has_value() &&
          !field9.has_value() && !field10.has_value() &&
          baseFields.has_value()) {
        CandMessagePOD wtf{.baseFields = {this->baseFields.value().timeSpan,
                                          this->baseFields.value().nodeType,
                                          this->baseFields.value().oid,
                                          this->baseFields.value().pid,
                                          this->baseFields.value().direction},
                           .LPBound = field6.value()};

        ret = wtf;
        return ret;
      }
      break;
    case EventType::fathomed:
      // No additional fields
      if (!field6.has_value() && !field7.has_value() && !field8.has_value() &&
          !field9.has_value() && !field10.has_value() &&
          baseFields.has_value()) {
        FathMessagePOD wtf{.baseFields = {this->baseFields.value().timeSpan,
                                          this->baseFields.value().nodeType,
                                          this->baseFields.value().oid,
                                          this->baseFields.value().pid,
                                          this->baseFields.value().direction}};

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
        InfeasMessagePOD wtf{.baseFields = {this->baseFields.value().timeSpan,
                                            this->baseFields.value().nodeType,
                                            this->baseFields.value().oid,
                                            this->baseFields.value().pid,
                                            this->baseFields.value().direction},
                             .bCond = field9.value(),
                             .eCond = field10.value()};

        ret = wtf;
        return ret;
      }
      break;
    case EventType::integer:
      // Field 6, field 9, and field 10 must be present
      if (field6.has_value() && !field7.has_value() && !field8.has_value() &&
          field9.has_value() && field10.has_value() && baseFields.has_value()) {
        InteMessagePOD wtf{.baseFields = {this->baseFields.value().timeSpan,
                                          this->baseFields.value().nodeType,
                                          this->baseFields.value().oid,
                                          this->baseFields.value().pid,
                                          this->baseFields.value().direction},
                           .LPBound = field6.value(),
                           .bCond = field9.value(),
                           .eCond = field10.value()};

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

  std::visit([](auto &&arg) { std::cout << arg; }, result);

  // Send zmq message to client
}
