/*
 * Copyright 2016 Facebook, Inc.
 *
 * @author Eric Niebler (eniebler@fb.com), Sven Over (over@fb.com)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Acknowledgements: Giuseppe Ottaviano (ott@fb.com)
 */

/**
 * @class Function
 *
 * @brief A polymorphic function wrapper that is not copyable and does not
 *    require the wrapped function to be copy constructible.
 *
 * `folly::Function` is a polymorphic function wrapper, similar to
 * `std::function`. The template parameters of the `folly::Function` define
 * the parameter signature of the wrapped callable, but not the specific
 * type of the embedded callable. E.g. a `folly::Function<int(int)>`
 * can wrap callables that return an `int` when passed an `int`. This can be a
 * function pointer or any class object implementing one or both of
 *
 *     int operator(int);
 *     int operator(int) const;
 *
 * If both are defined, the non-const one takes precedence.
 *
 * Unlike `std::function`, a `folly::Function` can wrap objects that are not
 * copy constructible. As a consequence of this, `folly::Function` itself
 * is not copyable, either.
 *
 * Another difference is that, unlike `std::function`, `folly::Function` treats
 * const-ness of methods correctly. While a `std::function` allows to wrap
 * an object that only implements a non-const `operator()` and invoke
 * a const-reference of the `std::function`, `folly::Function` requires you to
 * declare a function type as const in order to be able to execute it on a
 * const-reference.
 *
 * For example:
 *
 *     class Foo {
 *      public:
 *       void operator()() {
 *         // mutates the Foo object
 *       }
 *     };
 *
 *     class Bar {
 *       std::function<void(void)> foo_; // wraps a Foo object
 *      public:
 *       void mutateFoo() const
 *       {
 *         foo_();
 *       }
 *     };
 *
 * Even though `mutateFoo` is a const-method, so it can only reference `foo_`
 * as const, it is able to call the non-const `operator()` of the Foo
 * object that is embedded in the foo_ function.
 *
 * `folly::Function` will not allow you to do that. You will have to decide
 * whether you need to invoke your wrapped callable from a const reference
 * (like in the example above), in which case it will only wrap a
 * `operator() const`. If your functor does not implement that,
 * compilation will fail. If you do not require to be able to invoke the
 * wrapped function in a const context, you can wrap any functor that
 * implements either or both of const and non-const `operator()`.
 *
 * The template parameter of `folly::Function`, the `FunctionType`, can be
 * const-qualified. Be aware that the const is part of the function signature.
 * It does not mean that the function type is a const type.
 *
 *   using FunctionType = R(Args...);
 *   using ConstFunctionType = R(Args...) const;
 *
 * In this example, `FunctionType` and `ConstFunctionType` are different
 * types. `ConstFunctionType` is not the same as `const FunctionType`.
 * As a matter of fact, trying to use the latter should emit a compiler
 * warning or error, because it has no defined meaning.
 *
 *     // This will not compile:
 *     folly::Function<void(void) const> func = Foo();
 *     // because Foo does not have a member function of the form:
 *     //   void operator()() const;
 *
 *     // This will compile just fine:
 *     folly::Function<void(void)> func = Foo();
 *     // and it will wrap the existing member function:
 *     //   void operator()();
 *
 * When should a const function type be used? As a matter of fact, you will
 * probably not need to use const function types very often. See the following
 * example:
 *
 *     class Bar {
 *       folly::Function<void()> func_;
 *       folly::Function<void() const> constFunc_;
 *
 *       void someMethod() {
 *         // Can call func_.
 *         func_();
 *         // Can call constFunc_.
 *         constFunc_();
 *       }
 *
 *       void someConstMethod() const {
 *         // Can call constFunc_.
 *         constFunc_();
 *         // However, cannot call func_ because a non-const method cannot
 *         // be called from a const one.
 *       }
 *     };
 *
 * As you can see, whether the `folly::Function`'s function type should
 * be declared const or not is identical to whether a corresponding method
 * would be declared const or not.
 *
 * You only require a `folly::Function` to hold a const function type, if you
 * intend to invoke it from within a const context. This is to ensure that
 * you cannot mutate its inner state when calling in a const context.
 *
 * This is how the const/non-const choice relates to lambda functions:
 *
 *     // Non-mutable lambdas: can be stored in a non-const...
 *     folly::Function<void(int)> print_number =
 *       [] (int number) { std::cout << number << std::endl; };
 *
 *     // ...as well as in a const folly::Function
 *     folly::Function<void(int) const> print_number_const =
 *       [] (int number) { std::cout << number << std::endl; };
 *
 *     // Mutable lambda: can only be stored in a non-const folly::Function:
 *     int number = 0;
 *     folly::Function<void()> print_number =
 *       [number] () mutable { std::cout << ++number << std::endl; };
 *     // Trying to store the above mutable lambda in a
 *     // `folly::Function<void() const>` would lead to a compiler error:
 *     // error: no viable conversion from '(lambda at ...)' to
 *     // 'folly::Function<void () const>'
 *
 * Casting between const and non-const `folly::Function`s:
 * conversion from const to non-const signatures happens implicitly. Any
 * function that takes a `folly::Function<R(Args...)>` can be passed
 * a `folly::Function<R(Args...) const>` without explicit conversion.
 * This is safe, because casting from const to non-const only entails giving
 * up the ability to invoke the function from a const context.
 * Casting from a non-const to a const signature is potentially dangerous,
 * as it means that a function that may change its inner state when invoked
 * is made possible to call from a const context. Therefore this cast does
 * not happen implicitly. The function `folly::constCastFunction` can
 * be used to perform the cast.
 *
 *     // Mutable lambda: can only be stored in a non-const folly::Function:
 *     int number = 0;
 *     folly::Function<void()> print_number =
 *       [number] () mutable { std::cout << ++number << std::endl; };
 *
 *     // const-cast to a const folly::Function:
 *     folly::Function<void() const> print_number_const =
 *       constCastFunction(std::move(print_number));
 *
 * When to use const function types?
 * Generally, only when you need them. When you use a `folly::Function` as a
 * member of a struct or class, only use a const function signature when you
 * need to invoke the function from const context.
 * When passing a `folly::Function` to a function, the function should accept
 * a non-const `folly::Function` whenever possible, i.e. when it does not
 * need to pass on or store a const `folly::Function`. This is the least
 * possible constraint: you can always pass a const `folly::Function` when
 * the function accepts a non-const one.
 *
 * How does the const behaviour compare to `std::function`?
 * `std::function` can wrap object with non-const invokation behaviour but
 * exposes them as const. The equivalent behaviour can be achieved with
 * `folly::Function` like so:
 *
 *     std::function<void(void)> stdfunc = someCallable;
 *
 *     folly::Function<void(void) const> uniqfunc = constCastFunction(
 *       folly::Function<void(void)>(someCallable)
 *     );
 *
 * You need to wrap the callable first in a non-const `folly::Function` to
 * select a non-const invoke operator (or the const one if no non-const one is
 * present), and then move it into a const `folly::Function` using
 * `constCastFunction`.
 * The name of `constCastFunction` should warn you that something
 * potentially dangerous is happening. As a matter of fact, using
 * `std::function` always involves this potentially dangerous aspect, which
 * is why it is not considered fully const-safe or even const-correct.
 * However, in most of the cases you will not need the dangerous aspect at all.
 * Either you do not require invokation of the function from a const context,
 * in which case you do not need to use `constCastFunction` and just
 * use the inner `folly::Function` in the example above, i.e. just use a
 * non-const `folly::Function`. Or, you may need invokation from const, but
 * the callable you are wrapping does not mutate its state (e.g. it is a class
 * object and implements `operator() const`, or it is a normal,
 * non-mutable lambda), in which case you can wrap the callable in a const
 * `folly::Function` directly, without using `constCastFunction`.
 * Only if you require invokation from a const context of a callable that
 * may mutate itself when invoked you have to go through the above procedure.
 * However, in that case what you do is potentially dangerous and requires
 * the equivalent of a `const_cast`, hence you need to call
 * `constCastFunction`.
 */

#pragma once

#include <functional>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

#include <folly/CppAttributes.h>

namespace folly {

namespace impl {
template <typename FunctionType, bool Const = false>
class Function;

template <typename ReturnType, typename... Args>
Function<ReturnType(Args...), true> constCastFunction(
    Function<ReturnType(Args...), false>&&) noexcept;
}

namespace detail {
namespace function {

enum class Op { MOVE, NUKE, FULL, HEAP };

union Data {
  void* big;
  typename std::aligned_storage<6 * sizeof(void*)>::type small;
};

struct Tag {};

template <bool If, typename T>
using ConstIf = typename std::conditional<If, const T, T>::type;

template <typename Fun, typename FunT = typename std::decay<Fun>::type>
using IsSmall = std::integral_constant<
    bool,
    (sizeof(FunT) <= sizeof(Data::small) &&
#if defined(__GNUC__) && !defined(__clang__)
     // GCC has a name mangling bug that causes hard errors if we use noexcept
     // directly here. Last tested at gcc 5.3.0.
     // See: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=70790
     std::is_nothrow_move_constructible<FunT>::value
#else
     // Same as is_nothrow_move_constructible, but w/ no template instantiation.
     noexcept(FunT(std::declval<FunT&&>()))
#endif
     )>;

template <typename T>
bool isNullPtrFn(T* p) {
  return p == nullptr;
}
template <typename T>
std::false_type isNullPtrFn(T&&) {
  return {};
}

template <typename ReturnType, typename... Args>
ReturnType uninitCall(Data&, Args&&...) {
  throw std::bad_function_call();
}
inline bool uninitNoop(Op, Data*, Data*) {
  return false;
}
} // namespace function
} // namespace detail

namespace impl {

template <typename ReturnType, typename... Args, bool Const>
class Function<ReturnType(Args...), Const> final {
  using Data = detail::function::Data;
  using Op = detail::function::Op;
  using Tag = detail::function::Tag;
  using Call = ReturnType (*)(Data&, Args&&...);
  using Exec = bool (*)(Op, Data*, Data*);

  template <typename T>
  using ConstIf = detail::function::ConstIf<Const, T>;
  template <typename Fun>
  using IsSmall = detail::function::IsSmall<Fun>;

  Data data_;
  Call call_;
  Exec exec_;

  friend Function<ReturnType(Args...), true> constCastFunction<>(
      Function<ReturnType(Args...), false>&&) noexcept;
  friend class Function<ReturnType(Args...), !Const>;

  template <typename Fun, typename FunT = typename std::decay<Fun>::type>
  Function(
      Fun&& fun,
      typename std::enable_if<IsSmall<Fun>::value, Tag>::
          type) noexcept(noexcept(FunT(std::declval<Fun>())))
      : Function() {
    struct Ops {
      static ReturnType call(Data& p, Args&&... args) {
        return static_cast<ReturnType>((*static_cast<ConstIf<FunT>*>(
            (void*)&p.small))(static_cast<Args&&>(args)...));
      }
      static bool exec(Op o, Data* src, Data* dst) {
        switch (o) {
          case Op::MOVE:
            ::new ((void*)&dst->small)
                FunT(std::move(*static_cast<FunT*>((void*)&src->small)));
            FOLLY_FALLTHROUGH;
          case Op::NUKE:
            static_cast<FunT*>((void*)&src->small)->~FunT();
            break;
          case Op::FULL:
            return true;
          case Op::HEAP:
            break;
        }
        return false;
      }
    };
    if (!detail::function::isNullPtrFn(fun)) {
      ::new (&data_.small) FunT(static_cast<Fun&&>(fun));
      exec_ = &Ops::exec;
      call_ = &Ops::call;
    }
  }

  template <typename Fun, typename FunT = typename std::decay<Fun>::type>
  Function(Fun&& fun, typename std::enable_if<!IsSmall<Fun>::value, Tag>::type)
      : Function() {
    struct Ops {
      static ReturnType call(Data& p, Args&&... args) {
        return static_cast<ReturnType>((*static_cast<ConstIf<FunT>*>(p.big))(
            static_cast<Args&&>(args)...));
      }
      static bool exec(Op o, Data* src, Data* dst) {
        switch (o) {
          case Op::MOVE:
            dst->big = src->big;
            src->big = nullptr;
            break;
          case Op::NUKE:
            delete static_cast<FunT*>(src->big);
            break;
          case Op::FULL:
          case Op::HEAP:
            break;
        }
        return true;
      }
    };
    data_.big = new FunT(static_cast<Fun&&>(fun));
    call_ = &Ops::call;
    exec_ = &Ops::exec;
  }
  template <typename F, typename G = typename std::decay<F>::type>
  using ResultOf = decltype(static_cast<ReturnType>(
      std::declval<ConstIf<G>&>()(std::declval<Args>()...)));

 public:
  /**
   * Default constructor. Constructs an empty Function.
   */
  Function() noexcept
      : call_(&detail::function::uninitCall<ReturnType, Args...>),
        exec_(&detail::function::uninitNoop) {}

  // not copyable
  // NOTE: Deleting the non-const copy constructor is unusual but necessary to
  // prevent copies from non-const `Function` object from selecting the
  // perfect forwarding implicit converting constructor below
  // (i.e., `template <typename Fun> Function(Fun&&)`).
  Function(Function&) = delete;
  Function(const Function&) = delete;

  /**
   * Move constructor
   */
  Function(Function&& that) noexcept : Function() {
    that.exec_(Op::MOVE, &that.data_, &data_);
    std::swap(call_, that.call_);
    std::swap(exec_, that.exec_);
  }

  /**
   * Constructs an empty `Function`.
   */
  /* implicit */ Function(std::nullptr_t) noexcept : Function() {}

  /**
   * Constructs a new `Function` from any callable object. This
   * handles function pointers, pointers to static member functions,
   * `std::reference_wrapper` objects, `std::function` objects, and arbitrary
   * objects that implement `operator()` if the parameter signature
   * matches (i.e. it returns R when called with Args...).
   * For a `Function` with a const function type, the object must be
   * callable from a const-reference, i.e. implement `operator() const`.
   * For a `Function` with a non-const function type, the object will
   * be called from a non-const reference, which means that it will execute
   * a non-const `operator()` if it is defined, and falls back to
   * `operator() const` otherwise.
   *
   * \note `typename = ResultOf<Fun>` prevents this overload from being
   * selected by overload resolution when `fun` is not a compatible function.
   */
  template <class Fun, typename = ResultOf<Fun>>
  /* implicit */ Function(Fun&& fun) noexcept(
      noexcept(Function(std::declval<Fun>(), Tag{})))
      : Function(static_cast<Fun&&>(fun), Tag{}) {}

  /**
   * For moving a `Function<X(Ys..) const>` into a `Function<X(Ys...)>`.
   */
  template <
      bool OtherConst,
      typename std::enable_if<!Const && OtherConst, int>::type = 0>
  Function(Function<ReturnType(Args...), OtherConst>&& that) noexcept
      : Function() {
    that.exec_(Op::MOVE, &that.data_, &data_);
    std::swap(call_, that.call_);
    std::swap(exec_, that.exec_);
  }

  /**
   * If `ptr` is null, constructs an empty `Function`. Otherwise,
   * this constructor is equivalent to `Function(std::mem_fn(ptr))`.
   */
  template <
      typename Member,
      typename Class,
      // Prevent this overload from being selected when `ptr` is not a
      // compatible member function pointer.
      typename = decltype(Function(std::mem_fn((Member Class::*)0)))>
  /* implicit */ Function(Member Class::*ptr) noexcept : Function() {
    if (ptr) {
      *this = std::mem_fn(ptr);
    }
  }

  ~Function() {
    exec_(Op::NUKE, &data_, nullptr);
  }

  Function& operator=(Function&) = delete;
  Function& operator=(const Function&) = delete;

  /**
   * Move assignment operator
   */
  Function& operator=(Function&& that) noexcept {
    if (&that != this) {
      // Q: Why is is safe to destroy and reconstruct this object in place?
      // A: Two reasons: First, `Function` is a final class, so in doing this
      //    we aren't slicing off any derived parts. And second, the move
      //    operation is guaranteed not to throw so we always leave the object
      //    in a valid state.
      this->~Function();
      ::new (this) Function(std::move(that));
    }
    return *this;
  }

  /**
   * Assigns a callable object to this `Function`. If the operation fails,
   * `*this` is left unmodified.
   *
   * \note `typename = ResultOf<Fun>` prevents this overload from being
   * selected by overload resolution when `fun` is not a compatible function.
   */
  template <class Fun, typename = ResultOf<Fun>>
  Function& operator=(Fun&& fun) noexcept(
      noexcept(/* implicit */ Function(std::declval<Fun>()))) {
    // Doing this in place is more efficient when we can do so safely.
    if (noexcept(/* implicit */ Function(std::declval<Fun>()))) {
      // Q: Why is is safe to destroy and reconstruct this object in place?
      // A: See the explanation in the move assignment operator.
      this->~Function();
      ::new (this) Function(static_cast<Fun&&>(fun));
    } else {
      // Construct a temporary and (nothrow) swap.
      Function(static_cast<Fun&&>(fun)).swap(*this);
    }
    return *this;
  }

  /**
   * Clears this `Function`.
   */
  Function& operator=(std::nullptr_t) noexcept {
    return (*this = Function());
  }

  /**
   * If `ptr` is null, clears this `Function`. Otherwise, this assignment
   * operator is equivalent to `*this = std::mem_fn(ptr)`.
   */
  template <typename Member, typename Class>
  auto operator=(Member Class::*ptr) noexcept
      // Prevent this overload from being selected when `ptr` is not a
      // compatible member function pointer.
      -> decltype(operator=(std::mem_fn(ptr))) {
    return ptr ? (*this = std::mem_fn(ptr)) : (*this = Function());
  }

  /**
   * Call the wrapped callable object with the specified arguments.
   * If this `Function` object is a const `folly::Function` object,
   * this overload shall not participate in overload resolution.
   */
  template <
      // `True` makes `operator()` a template so we can SFINAE on `Const`,
      // which is non-deduced here.
      bool True = true,
      typename std::enable_if<True && !Const, int>::type = 0>
  ReturnType operator()(Args... args) {
    return call_(data_, static_cast<Args&&>(args)...);
  }

  /**
   * Call the wrapped callable object with the specified arguments.
   * If this `Function` object is not a const `folly::Function` object,
   * this overload shall not participate in overload resolution.
   */
  template <
      // `True` makes `operator()` a template so we can SFINAE on `Const`,
      // which is non-deduced here.
      bool True = true,
      typename std::enable_if<True && Const, int>::type = 0>
  ReturnType operator()(Args... args) const {
    return call_(const_cast<Data&>(data_), static_cast<Args&&>(args)...);
  }

  /**
   * Exchanges the callable objects of `*this` and `that`.
   */
  void swap(Function& that) noexcept {
    std::swap(*this, that);
  }

  /**
   * Returns `true` if this `Function` contains a callable, i.e. is
   * non-empty.
   */
  explicit operator bool() const noexcept {
    return exec_(Op::FULL, nullptr, nullptr);
  }

  /**
   * Returns `true` if this `Function` stores the callable on the
   * heap. If `false` is returned, there has been no additional memory
   * allocation and the callable is stored inside the `Function`
   * object itself.
   */
  bool hasAllocatedMemory() const noexcept {
    return exec_(Op::HEAP, nullptr, nullptr);
  }

  /**
   * Construct a `std::function` by moving in the contents of this `Function`.
   * Note that the returned `std::function` will share its state (i.e. captured
   * data) across all copies you make of it, so be very careful when copying.
   */
  std::function<ReturnType(Args...)> asStdFunction() && {
    struct Impl {
      std::shared_ptr<Function> sp_;
      ReturnType operator()(Args&&... args) const {
        return (*sp_)(static_cast<Args&&>(args)...);
      }
    };
    return Impl{std::make_shared<Function>(std::move(*this))};
  }
};

template <typename FunctionType, bool Const>
void swap(
    Function<FunctionType, Const>& lhs,
    Function<FunctionType, Const>& rhs) noexcept {
  lhs.swap(rhs);
}

template <typename FunctionType, bool Const>
bool operator==(const Function<FunctionType, Const>& fn, std::nullptr_t) {
  return !fn;
}

template <typename FunctionType, bool Const>
bool operator==(std::nullptr_t, const Function<FunctionType, Const>& fn) {
  return !fn;
}

template <typename FunctionType, bool Const>
bool operator!=(const Function<FunctionType, Const>& fn, std::nullptr_t) {
  return !(fn == nullptr);
}

template <typename FunctionType, bool Const>
bool operator!=(std::nullptr_t, const Function<FunctionType, Const>& fn) {
  return !(nullptr == fn);
}

template <typename ReturnType, typename... Args>
Function<ReturnType(Args...), true> constCastFunction(
    Function<ReturnType(Args...), false>&& that) noexcept {
  Function<ReturnType(Args...), true> fn{};
  that.exec_(detail::function::Op::MOVE, &that.data_, &fn.data_);
  std::swap(fn.call_, that.call_);
  std::swap(fn.exec_, that.exec_);
  return fn;
}

template <typename FunctionType>
Function<FunctionType, true> constCastFunction(
    Function<FunctionType, true>&& that) noexcept {
  return std::move(that);
}

template <typename FunctionType>
struct MakeFunction {};

template <typename ReturnType, typename... Args>
struct MakeFunction<ReturnType(Args...)> {
  using type = Function<ReturnType(Args...), false>;
};

template <typename ReturnType, typename... Args>
struct MakeFunction<ReturnType(Args...) const> {
  using type = Function<ReturnType(Args...), true>;
};
} // namespace impl

/* using override */ using impl::constCastFunction;

template <typename FunctionType>
using Function = typename impl::MakeFunction<FunctionType>::type;
}