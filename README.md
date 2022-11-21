A GCC plugin allowing consistent detection of exceptions in the terminate handler without unwinding noexcept functions

## The problem

GCC implements `noexcept` functions containing catch blocks by adding a cleanup handler that calls `std::terminate`.  This causes multiple problems:

1. The stack is not consistently unwound ([GCC bug 55918](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=55918)).  Because termination is implemented with a cleanup block rather than dedicated metadata, the runtime does not know that the function is `noexcept` and will continue past it when searching for a `catch` block.  If it reaches the end of the stack while searching, the stack will not be unwound, but if the exception could be caught by a handler farther up the stack, it will be unwound.
2. If the stack is unwound, the exception is not available via `std::current_exception` ([GCC bug 97720](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=97720)).  Exceptions are made available to `std::current_exception` via the `__cxa_begin_catch` function.  GCC inserts calls to `__cxa_begin_catch` before catch blocks but not before cleanup blocks, and since the call to `std::terminate` is in a cleanup block this means the exception is never made available.  This is in contrast to the [Exception Handling Itanium C++ ABI](https://itanium-cxx-abi.github.io/cxx-abi/abi-eh.html) section 2.5.3, which says that a `terminate()` call due to a throw is an exception handler, and all exception handlers must call `__cxa_begin_catch`.  This is also in contrast to the C++ Standard, which says in section **support.exception.propagation** that `std::current_exception` returns "an `exception_ptr` object that refers to the currently handled exception", and in section **except.handle** says "the exception with the most recently activated handler that is still active is called the *currently handled exception*" and "an implicit handler is considered active when the function `std::terminate` is entered due to a throw".

## Other implementations

* MSVC does not use Itanium C++ ABI, and its ABI has metadata indicating whether a function is `noexcept`.
* Clang effectively wraps the function with `try { function body } catch (...) { std::terminate(); }`[^clang_call_terminate].  This ensures that `std::terminate` is always called in a handler, but also requires the stack to always be unwound, which makes postmortem debugging harder.
[^clang_call_terminate]: Unlike if a user specified it, clang does not do this inline, but instead emits a call to `__clang_call_terminate`, which calls `__cxa_begin_catch` followed by `std::terminate`

## The workaround/"solution"

This approach was inspired by [a comment on GCC bug 55918](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=55918#c6).  It is divided into 2 parts: a user source file (compiled and linked with the final binary) that defines a new type and a personality function that catches that type, and a GCC plugin that modifies `noexcept` functions to use this new type and personality.

### The user source

The user source defines a type named `__noexcept_marker`.  It then defines the type info for that type, and overrides the libstdc++-internal function `__do_catch`.  This function is normally used to determine whether the thrown object is derived from the base type in the catch block, and to convert it to the base type.  In this case, it unconditionally indicates that it can handle whatever exception is thrown, and "converts" it to `__noexcept_marker` by ignoring the exception object's actual address and using the address of a global variable instead.

Additionally, the file defines a personality function, which normally checks for catch blocks and handles calling destructors during unwinding.  This personality function calls the default personality function, and then checks if the exception was "converted" to `__noexcept_marker`.  If it was, it immediately calls `__cxa_begin_catch` and `std::terminate`.

### The GCC plugin

The GCC plugin inserts a new compiler pass just before the exception handling code is lowered.  This pass performs 2 transformations on every function being compiled:

1. It replaces the default personality function with the one defined in the user source file.
2. If the function is `noexcept`, it looks for the cleanup block which calls `std::terminate` and replaces it with a catch block that catches `__noexcept_marker`.

### Additional notes

* The [aforementioned comment](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=55918#c6) suggested calling `std::terminate` directly from the `__do_catch` override.  However, `__do_catch` does not have access to the exception header, so it can't call `__cxa_begin_catch`, which means the terminate handler would not have access to the exception.
* Initially, the personality returned `_URC_END_OF_STACK` when it detected a `noexcept` function, which caused its callee (`_Unwind_RaiseException`) to return immediately, causing `__cxa_throw` to call `std::terminate` correctly.  However, this is not allowed by the ABI, which mandates that the personality function only return `_URC_HANDLER_FOUND` or `_URC_CONTINUE_UNWIND` when searching for a handler.

## CMake

The CMake project includes samples demonstrating the plugin's effect and a convenience target for using the plugin.  To run the samples, build the target "run-samples".  To use the convenience target, add the project using `add_subdirectory` and link your binary with `use_noexcept_personality`.
