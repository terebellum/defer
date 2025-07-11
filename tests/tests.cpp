// To compile: g++ -std=c++17 -Wall -g -pthread -fno-omit-frame-pointer test_defer.cpp -o test_defer

#define CATCH_CONFIG_MAIN
#include "catch2/catch_test_macros.hpp"
#include <defer.h>

#include <vector>
#include <string>
#include <stdexcept>
#include <thread>
#include <future>

// =====================================================================================
//   HELPER FUNCTIONS
//   These MUST be marked `noinline` to ensure they have their own stack frame.
//   This is a core requirement for the `defer` technique to work.
// =====================================================================================

__attribute__((noinline))
void test_basic_defer_logic(std::vector<std::string>& log) {
    log.push_back("enter_function");
    defer [&log] { log.push_back("deferred_action"); };
    log.push_back("exit_function");
}

__attribute__((noinline, optnone))
void test_lifo_order_logic(std::vector<std::string>& log) {
    defer [&log] { log.push_back("deferred_1"); };
    defer [&log] { log.push_back("deferred_2"); };
    defer [&log] { log.push_back("deferred_3"); };
}

__attribute__((noinline))
void test_exception_logic(std::vector<std::string>& log) {
    defer [&log] {log.push_back("deffered_1"); };
    throw std::runtime_error("defer bypasses try-catch");
}

__attribute__((noinline))
bool function_with_early_return(bool& side_effect_flag) {
    defer [&] { side_effect_flag = true; };

    if (true) {
        // The return value is determined *before* the defer runs.
        // At this point, side_effect_flag is still `false`.
        return side_effect_flag;
    }

    return true;
}


__attribute__((noinline))
void test_variable_capture_logic(int& value) {
    // The deferred lambda will run after this function returns,
    // modifying the variable in the calling scope.
    defer [&value] { value = 99; };
    value = 10; // Set initial value before returning
}

__attribute__((noinline))
void test_thread_local_logic(std::promise<bool>& p) {
    bool local_flag = false;
    defer [&]()  {
        local_flag = true;
        p.set_value(local_flag);
    };
    // Let the function return to trigger the defer
}


// =====================================================================================
//   TEST CASES
// =====================================================================================

TEST_CASE("Defer executes after function returns (Basic Correctness)") {
    std::vector<std::string> log;
    test_basic_defer_logic(log);

    // The deferred action should be the LAST thing to run.
    REQUIRE(log.size() == 3);
    REQUIRE(log[0] == "enter_function");
    REQUIRE(log[1] == "exit_function");
    REQUIRE(log[2] == "deferred_action");
}

TEST_CASE("Multiple defers execute in LIFO order") {
    std::vector<std::string> log;
    test_lifo_order_logic(log);

    // Defer 3 was last in, so it should be first out.
    REQUIRE(log.size() == 3);
    REQUIRE(log[0] == "deferred_3");
    REQUIRE(log[1] == "deferred_2");
    REQUIRE(log[2] == "deferred_1");
}


TEST_CASE("Defer executes on early return") {
    bool side_effect_happened = false;

    // Call the function. It captures the return value, which should be `false`.
    // After the function returns, the deferred task will run, setting
    // side_effect_happened to `true`.
    bool returned_value = function_with_early_return(side_effect_happened);

    // 1. Check that the return value was captured BEFORE the defer ran.
    REQUIRE(returned_value == false);

    // 2. Check that the side-effect from the deferred task IS visible now.
    REQUIRE(side_effect_happened == true);
}


TEST_CASE("Deferred lambda can capture and modify local variables") {
    int value = 0;
    test_variable_capture_logic(value);
    // The helper function sets value to 10, but the deferred action
    // overwrites it to 99 after the function returns.
    REQUIRE(value == 99);
}

TEST_CASE("LIMITATION: Defer DO NOT work with stack unwinding", "[!mayfail]") {
    std::vector<std::string> log;

    // C++ exception handling uses a separate mechanism (unwind tables) that
    // bypasses the standard function `ret` instruction. Our ajssembly hijack
    // relies on overwriting the return address used by `ret`, so it mainly UB
    REQUIRE_THROWS_AS(test_exception_logic(log), std::runtime_error);

    REQUIRE(log.size() == 1);
}

TEST_CASE("Defer is thread-local and does not interfere between threads") {
    std::promise<bool> promise1;
    std::promise<bool> promise2;
    auto future1 = promise1.get_future();
    auto future2 = promise2.get_future();

    std::thread t1(test_thread_local_logic, std::ref(promise1));
    std::thread t2(test_thread_local_logic, std::ref(promise2));

    t1.join();
    t2.join();

    // Check that both threads successfully completed their own deferred tasks
    // without interfering with each other's defer stack.
    REQUIRE(future1.get() == true);
    REQUIRE(future2.get() == true);
}