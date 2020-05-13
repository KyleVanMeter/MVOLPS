#ifndef MESSAGE_H
#define MESSAGE_H

#include "util.h"

#include <chrono>
#include <map>
#include <memory>
#include <optional>
#include <string>
// For automatic operator<< to message structs
#include <iostream>
#include <tuple>
#include <type_traits>

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

/*
 * The purpose of these templates is to at least somewhat automatically generate
 * operators for sum-types (structs) whose data members support such an
 * operation.  In this specific case this is to facilitate operator<< for each
 * *MessagePOD data-type.
 */
template <size_t... Ns> struct indices {};

template <size_t N, size_t... Ns>
struct build_indices : build_indices<N - 1, N - 1, Ns...> {};

template <size_t... Ns> struct build_indices<0, Ns...> : indices<Ns...> {};

template <typename T, typename Tuple, size_t... Indices>
void memberOut(std::ostream &stream, T &in, const Tuple &typeInfo,
               indices<Indices...>) {
  ((stream << in.*std::get<Indices>(typeInfo) << " "), ...);
}

template <typename T> struct Streamer {
  friend std::ostream &operator<<(std::ostream &stream, Streamer<T> &in) {
    memberOut(stream, *static_cast<T *>(&in), T::typeInfo,
              build_indices<std::tuple_size<decltype(T::typeInfo)>::value>{});

    return stream;
  }
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

struct BaseMessagePOD : public Streamer<BaseMessagePOD> {
  double timeSpan = std::chrono::duration<double, std::milli>(
                        std::chrono::high_resolution_clock::now() - startTime)
                        .count();
  EventType nodeType;
  int oid;
  int pid;
  BranchDirection direction;

  const static std::tuple<
      decltype(&BaseMessagePOD::timeSpan), decltype(&BaseMessagePOD::nodeType),
      decltype(&BaseMessagePOD::oid), decltype(&BaseMessagePOD::pid),
      decltype(&BaseMessagePOD::direction)>
      typeInfo;
};

struct HeurMessagePOD : public Streamer<HeurMessagePOD> {
  BaseMessagePOD baseFields;
  double value;

  const static std::tuple<decltype(&HeurMessagePOD::baseFields),
                          decltype(&HeurMessagePOD::value)>
      typeInfo;
};

struct InfeasMessagePOD : public Streamer<InfeasMessagePOD> {
  BaseMessagePOD baseFields;
  int bCond;
  int eCond;

  const static std::tuple<decltype(&InfeasMessagePOD::baseFields),
                          decltype(&InfeasMessagePOD::bCond),
                          decltype(&InfeasMessagePOD::eCond)>
      typeInfo;
};

struct BranchMessagePOD : public Streamer<BranchMessagePOD> {
  BaseMessagePOD baseFields;
  double LPBound;
  double infeasCount;
  int violatedCount;
  int bCond;
  int eCond;

  const static std::tuple<decltype(&BranchMessagePOD::baseFields),
                          decltype(&BranchMessagePOD::LPBound),
                          decltype(&BranchMessagePOD::infeasCount),
                          decltype(&BranchMessagePOD::violatedCount),
                          decltype(&BranchMessagePOD::bCond),
                          decltype(&BranchMessagePOD::eCond)>
      typeInfo;
};

struct CandMessagePOD : public Streamer<CandMessagePOD> {
  BaseMessagePOD baseFields;
  double LPBound;

  const static std::tuple<decltype(&CandMessagePOD::baseFields),
                          decltype(&CandMessagePOD::LPBound)>
      typeInfo;
};


struct InteMessagePOD : public Streamer<InteMessagePOD> {
  BaseMessagePOD baseFields;
  double LPBound;
  int bCond;
  int eCond;

  const static std::tuple<
      decltype(&InteMessagePOD::baseFields), decltype(&InteMessagePOD::LPBound),
      decltype(&InteMessagePOD::bCond), decltype(&InteMessagePOD::eCond)>
      typeInfo;
};

struct FathMessagePOD : public Streamer<FathMessagePOD> {
  BaseMessagePOD baseFields;
  const static std::tuple<decltype(&FathMessagePOD::baseFields)> typeInfo;
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
