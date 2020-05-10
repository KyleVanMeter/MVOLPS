#ifndef MESSAGE_H
#define MESSAGE_H

#include "util.h"

#include <chrono>
#include <map>
#include <memory>
#include <optional>
#include <string>

namespace MVOLP {
template <class Interface, class KeyT = std::string> class Factory {
public:
  using Key = KeyT;
  using Type = std::shared_ptr<Interface>;
  using Creator = Type (*)();

  bool define(Key const &key, Creator v) {
    return _registry.insert(typename Registry::value_type(key, v)).second;
  }

  Type create(Key const &key) {
    typename Registry::const_iterator i = _registry.find(key);
    if (i == _registry.end()) {
      throw std::invalid_argument(std::string(__PRETTY_FUNCTION__) +
                                  ": key not registered");
    } else {
      return i->second();
    }
  }

  template <class Base, class Actual>
  static std::shared_ptr<Base> create_func() {
    return std::shared_ptr<Base>(new Actual());
  }

private:
  typedef std::map<Key, Creator> Registry;
  Registry _registry;
};

namespace {
class BaseMessageDispatch {
public:
  typedef MVOLP::Factory<BaseMessageDispatch> Factory;
  virtual ~BaseMessageDispatch(){};

  virtual void write() const = 0;

  static Factory::Type create(Factory::Key const &name) {
    return _factory.create(name);
  }

  template <class Derived> static void define(Factory::Key const &name) {
    bool new_key = _factory.define(
        name, &Factory::template create_func<BaseMessageDispatch, Derived>);
    if (!new_key) {
      throw std::logic_error(std::string(__PRETTY_FUNCTION__) +
                             ": name already registered");
    }
  }

private:
  static Factory _factory;
};
BaseMessageDispatch::Factory BaseMessageDispatch::_factory;
} // namespace

class LogDispatch : public BaseMessageDispatch {
public:
  virtual void write() const;

  // template <class... Args> LogDispatch *message(Args &&... args);
  LogDispatch *message(std::string in);

private:
  std::string _msg;
};

class DebugDispatch : public BaseMessageDispatch {
public:
  virtual void write() const;

  // template <class... Args> DebugDispatch *message(Args &&... args);
  DebugDispatch *message(std::string in);

private:
  std::string _msg;
};

enum EventType {
  heuristic,
  infeasible,
  branched,
  candidate,
  integer,
  fathomed
};

enum BranchDirection { L, R, M };

struct BaseMessagePOD {
  double timeSpan = std::chrono::duration<double, std::milli>(
                        std::chrono::high_resolution_clock::now() - startTime)
                        .count();
  EventType nodeType;
  int oid;
  int pid;
  BranchDirection direction;

  friend std::ostream &operator<<(std::ostream &os, const BaseMessagePOD &in) {
    os << in.timeSpan << " " << in.nodeType << " " << in.oid << " " << in.pid
       << " " << in.direction << "\n";

    return os;
  }
};

struct HeurMessagePOD {
  BaseMessagePOD baseFields;
  double value;

  friend std::ostream &operator<<(std::ostream &os, const HeurMessagePOD &in) {
    os << in.baseFields << " " << in.value << "\n";

    return os;
  }
};

struct InfeasMessagePOD {
  BaseMessagePOD baseFields;
  int bCond;
  int eCond;

  friend std::ostream &operator<<(std::ostream &os,
                                  const InfeasMessagePOD &in) {
    os << in.baseFields << " " << in.bCond << " " << in.eCond << "\n";

    return os;
  }
};

struct BranchMessagePOD {
  BaseMessagePOD baseFields;
  double LPBound;
  double infeasCount;
  int violatedCount;
  int bCond;
  int eCond;

  friend std::ostream &operator<<(std::ostream &os,
                                  const BranchMessagePOD &in) {
    os << in.baseFields << " " << in.LPBound << " " << in.infeasCount << " "
       << in.violatedCount << " " << in.bCond << " " << in.eCond << "\n";

    return os;
  }
};

struct CandMessagePOD {
  BaseMessagePOD baseFields;
  double LPBound;

  friend std::ostream &operator<<(std::ostream &os,
                                  const CandMessagePOD&in) {
    os << in.baseFields << " " << in.LPBound << "\n";

    return os;
  }
};

struct InteMessagePOD {
  BaseMessagePOD baseFields;
  double LPBound;
  int bCond;
  int eCond;

  friend std::ostream &operator<<(std::ostream &os, const InteMessagePOD &in) {
    os << in.baseFields << " " << in.LPBound << " " << in.bCond << " " << in.eCond << "\n";

    return os;
  }
};

struct FathMessagePOD {
  BaseMessagePOD baseFields;

  friend std::ostream &operator<<(std::ostream &os, const FathMessagePOD &in) {
    os << in.baseFields << "\n";

    return os;
  }
};

class IPCDispatch : public BaseMessageDispatch {
public:
  virtual void write() const;
  // IPCDispatch *message(std::string in);

  /*
  double timeStamp;
  EventType type;
  int oid;
  int pid;
  BranchDirection direction;
  */
  std::optional<BaseMessagePOD> baseFields;
  std::optional<double> field6;
  std::optional<double> field7;
  std::optional<int> field8;
  std::optional<int> field9;
  std::optional<int> field10;

private:
  std::string _msg;
  std::unique_ptr<BaseMessagePOD> _data;
};

} // namespace MVOLP

#endif
