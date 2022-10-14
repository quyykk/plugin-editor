// SPDX-License-Identifier: GPL-3.0

#ifndef MFUNCTION_H_
#define MFUNCTION_H_

#include <cassert>
#include <cstddef>
#include <utility>

template <typename>
class mfunction;

// A simple move-only std::function.
template <typename R, typename... Ps>
class mfunction<R(Ps...)> {
 public:
  constexpr mfunction() noexcept = default;
  template <typename T>
  mfunction(T &&func) noexcept
      : func(reinterpret_cast<std::byte *>(
            new std::decay_t<T>(std::forward<T>(func)))),
        caller([](std::byte *Ptr, Ps &&...Args) -> decltype(auto) {
          return (*reinterpret_cast<std::decay_t<T> *>(Ptr))(
              std::forward<Ps>(Args)...);
        }),
        deleter([](std::byte *Ptr) noexcept {
          delete reinterpret_cast<std::decay_t<T> *>(Ptr);
        }) {}
  constexpr mfunction(mfunction &&Other) noexcept
      : func(std::exchange(Other.func, nullptr)),
        caller(std::exchange(Other.caller, nullptr)),
        deleter(std::exchange(Other.deleter, nullptr)) {}
  constexpr mfunction &operator=(mfunction &&Other) noexcept
  {
    func = std::exchange(Other.func, nullptr);
    caller = std::exchange(Other.caller, nullptr);
    deleter = std::exchange(Other.deleter, nullptr);
    return *this;
  }
  ~mfunction()
  {
    if(deleter)
		deleter(func);
  }

  template <typename... Ts>
  constexpr decltype(auto) operator()(Ts &&...Args) noexcept
  {
    assert(caller && "can't call null function");
    return caller(func, std::forward<Ts>(Args)...);
  }

  constexpr operator bool() noexcept { return func; }

 private:
  std::byte *func = nullptr;
  R (*caller)(std::byte *, Ps &&...) = nullptr;
  void (*deleter)(std::byte *) = nullptr;
};

template <typename R, typename ...Ts>
mfunction(R(*Func)(Ts&&...)) -> mfunction<R(Ts...)>;

#endif
