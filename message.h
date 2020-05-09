#ifndef MESSAGE_H
#define MESSAGE_H

#include <map>
#include <memory>
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

  //template <class... Args> LogDispatch *message(Args &&... args);
  LogDispatch *message(std::string in);

private:
  std::string _msg;
};

class DebugDispatch : public BaseMessageDispatch {
public:
  virtual void write() const;

  //template <class... Args> DebugDispatch *message(Args &&... args);
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
  double timeSpan;
  EventType type;
  int oid;
  int pid;
  BranchDirection direction;
};

struct HeurMessagePOD : public BaseMessagePOD {
  double value;
};

struct InfeasMessagePOD : public BaseMessagePOD {
  int bCond;
  int eCond;
};

struct BranchMessagePOD : public BaseMessagePOD {
  double LPBound;
  double infeasCount;
  int violatedCount;
  int bCond;
  int eCond;
};

struct CandMessagePOD : public BaseMessagePOD {
  int LPBound;
};

struct InteMessagePOD : public BaseMessagePOD {
  int LPBound;
  int bCond;
  int eCond;
};

struct FathMessagePOD : public BaseMessagePOD {};

class IPCDispatch : public BaseMessageDispatch {
public:
  virtual void write() const;
  IPCDispatch *message(std::string in);

private:
  std::string _msg;
  std::unique_ptr<BaseMessagePOD> _data;
};

} // namespace MVOLP

#endif
