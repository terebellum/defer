# defer

️**NOTE:** This is bad! It's highly UB: it can delete your system, call your grandma and make demons fly out of your nose 

## What's This?

It's  Go's `defer`. Real Non-RAII function-scope defer.

This header-only hack adds `defer` to C++ using stack manipulation. It schedules cleanup actions to run when your function exits:

```cpp
#include "defer.h"

void example() {
    FILE* f;
    {
        f = fopen("file.txt", "r");
        defer [&] { fclose(f); }; // Runs when function exits
    }
    
    // ... your code ...
} // <- `fclose` happens here
```

## How to Use
1. Grab `defer.h`  and use it

### How to build tests 

1. Compile tests with:
```bash
mkdir build
cmake ..
make
```
2. Run `./tests` to see the tests

## How It Works

- **Stack Hijacking:** It override  function's return address to jump into our `defer` trampoline
- **Task Queue:** Thread-local stack stores lambdas to execute on exit.
- **Execution**: It executes all defers and than jumps to real return address

## Known Limitations 

### 1. Sorry, no exceptions 
```cpp
TEST_CASE("LIMITATION: Defer DO NOT work with stack unwinding") {
    REQUIRE_THROWS_AS(test_exception_logic(log), std::runtime_error);
}
```
**Why?** Exceptions use OS-level unwind tables that bypass our return hijack. Moreover, because of defer exception now learn how to bypass try-catch =).

### 2. Requires compiles flags
You **SHOULD** compile with:
- `-fno-omit-frame-pointer` (for stack walking)
- `-O0` or carefully managed optimizations (or the stack layout breaks)

### 3.  No windows 
Only works on:
- x86-64 Linux/macOS (GCC/Clang)
- AArch64 
- **No Windows support** (mainly because there are no inline assembly, and I wanted this to be header-only)

### 4. No inlines
- Lambdas **must not be optimized out**
- Inlining can break stack assumptions
- `__attribute__((noinline))` is helpful for functions

### 5. Lambda pains
* No stacked lambdas. It just not works.
* Be careful with lambda captures

## Should I Use This?

| Situation | Verdict |
|-----------|---------|
| Production code | ❌ ABSOLUTELY NOT |
| Learning/experimenting | ✅ Yes! |
| I am crazy and UB is my friend | ✅ HELL YES |
| Need reliable cleanup | 💊 Use RAII like a normal person |

## Result 

This is **dangerous, unportable, with-assembly, bad, bad** code. It's a fun experiment, but real projects should stick to destructors and RAII. Things **will** break and you would not

## Contributing
If you can make it work in more ways - bring PRs in, let's make it morre reliable.