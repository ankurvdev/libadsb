#pragma once
#if !(defined SUPPRESS_WARNINGS_START)
#define SUPPRESS_WARNINGS_START                                                                 \
    _Pragma("warning(push, 3)") _Pragma("clang diagnostic push") _Pragma("GCC diagnostic push") \
        _Pragma("clang diagnostic ignored \"-Weverything\"")

#define SUPPRESS_WARNINGS_END _Pragma("GCC diagnostic pop") _Pragma("clang diagnostic pop") _Pragma("warning(pop)")

#define SUPPRESS_STL_WARNINGS                                                                                                 \
    _Pragma("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")                                                               \
        _Pragma("warning(disable : 4244)") /*'argument' : conversion from 'unsigned __int64' to 'size_t'*/                    \
        _Pragma("warning(disable : 4265)") /* class has virtual functions, but its non - trivial destructor is not virtual*/  \
        _Pragma("warning(disable : 4355)") /* 'this': used in base member initializer list*/                                  \
        _Pragma("warning(disable : 4365)") /* signed / unsigned mismatch*/                                                    \
        _Pragma("warning(disable : 4623)") /* default constructor was implicitly defined as deleted*/                         \
        _Pragma("warning(disable : 4625)") /* copy constructor was implicitly defined as deleted*/                            \
        _Pragma("warning(disable : 4626)") /* assignment operator was implicitly defined as deleted*/                         \
        _Pragma("warning(disable : 4668)") /* not defined as a preprocessor macro, replacing with '0' f*/                     \
        _Pragma("warning(disable : 4686)") /* chrono::operator -' : change behavior change in UDT return calling convention*/ \
        _Pragma("warning(disable : 4840)") /* non-portable use of class as an argument to a variadic function*/               \
        _Pragma("warning(disable : 4868)") /* compiler may not enforce left-to-right eval-order in braced initializer list*/  \
        _Pragma("warning(disable : 5027)") /* move assignment operator was implicitly defined as deleted*/                    \
        _Pragma("warning(disable : 5026)") /* move constructor was implicitly defined as deleted*/                            \
        _Pragma("warning(disable : 5204)") /* class has virtual functions, but its trivial destructor is not virtual;*/       \
        _Pragma("warning(disable : 5246)") /* initialization of a subobject should be wrapped in braces */                    \
        _Pragma("warning(disable : 5220)") /* a non-static data member with a volatile qualified type no longer implies*/     \
        _Pragma("warning(disable : 5262)") /* xlocale(2010, 13):implicit fall - through occurs here; */

#define SUPPRESS_FMT_WARNINGS                                                                                                \
    _Pragma("warning(disable : 4061)")     /* Not all labels are EXPLICITLY handled in switch */                             \
        _Pragma("warning(disable : 4127)") /* conditional expression is constant*/                                           \
        _Pragma("warning(disable : 4365)") /* signed / unsigned mismatch*/                                                   \
        _Pragma("warning(disable : 4582)") /* constructor is not implicitly called */                                        \
        _Pragma("warning(disable : 4623)") /* default constructor was implicitly defined as deleted*/                        \
        _Pragma("warning(disable : 4626)") /* assignment operator was implicitly defined as deleted*/                        \
        _Pragma("warning(disable : 4868)") /* compiler may not enforce left-to-right eval-order in braced initializer list*/ \
        _Pragma("warning(disable : 4738)") /* float-rounding. possible loss of performance. use /fp:fast*/                   \
        _Pragma("warning(disable : 5027)") /* move assignment operator was implicitly defined as deleted*/                   \
        _Pragma("warning(disable : 5204)") /* class has virtual functions, but its trivial destructor is not virtual;*/      \
        _Pragma("warning(disable : 4668)") /* not defined as a preprocessor macro, replacing with '0' f*/
#endif

#if (!defined CLASS_DEFAULT_COPY_AND_MOVE)
#define CLASS_DEFAULT_COPY_AND_MOVE(name)   \
    name(name const&)            = default; \
    name(name&&)                 = default; \
    name& operator=(name const&) = default; \
    name& operator=(name&&)      = delete

#define CLASS_DELETE_MOVE_ASSIGNMENT(name)  \
    name(name const&)            = default; \
    name(name&&)                 = default; \
    name& operator=(name const&) = default; \
    name& operator=(name&&)      = delete

#define CLASS_DELETE_MOVE_AND_COPY_ASSIGNMENT(name) \
    name(name const&)            = default;         \
    name(name&&)                 = default;         \
    name& operator=(name const&) = delete;          \
    name& operator=(name&&)      = delete

#define CLASS_DELETE_COPY_AND_MOVE(name)   \
    name(name const&)            = delete; \
    name(name&&)                 = delete; \
    name& operator=(name const&) = delete; \
    name& operator=(name&&)      = delete

#define CLASS_DELETE_COPY_DEFAULT_MOVE(name) \
    name(name const&)            = delete;   \
    name(name&&)                 = default;  \
    name& operator=(name const&) = delete;   \
    name& operator=(name&&)      = default

#define CLASS_ONLY_MOVE_CONSTRUCT(name)     \
    name(name const&)            = delete;  \
    name(name&&)                 = default; \
    name& operator=(name const&) = delete;  \
    name& operator=(name&&)      = delete
#endif

#if !defined             TODO
[[noreturn]] inline void TodoFunc()
{
    throw "Not Implemented:";
}
#define TODO(...) TodoFunc()
#endif
