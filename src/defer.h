#ifndef FUNC_DEFER_H
#define FUNC_DEFER_H

#include <cassert>
#include <exception>
#include <thread>
#include <utility>
#include <vector>
#include <functional>
#include <dlfcn.h>

#if defined(__APPLE__)
#define C_SYMBOL_NAME(name) "_" #name
#else
#define C_SYMBOL_NAME(name) #name
#endif

namespace defer_private {

thread_local static std::vector<std::function<void()>> deferred_task_stack;
thread_local static void* final_return_address = nullptr;
 
extern "C" __attribute__((used)) inline void* execute_deferred_task() {
    auto return_address = final_return_address;
    final_return_address = nullptr;

    while (!deferred_task_stack.empty()) {
        auto task = std::move(deferred_task_stack.back());
        deferred_task_stack.pop_back();
        task();
    }
    return return_address;
}

 
extern "C" __attribute__((used, naked)) inline void defer_trampoline() {
#if (defined(__GNUC__) || defined(__clang__)) && defined(__x86_64__)
__asm__(
    "    subq $8, %rsp\n" // align stack to 16-byte boundary
    "    call " C_SYMBOL_NAME(execute_deferred_task) "\n"
    "    addq $8, %rsp\n" // restore stack
    "    jmp *%rax\n" // jump to original return address
);
#elif (defined(__GNUC__) || defined(__clang__)) && defined(__aarch64__)
__asm__(
    "    stp x29, x30, [sp, #-16]!\n" // Save FP and LR, 16-byte align
    "    bl " C_SYMBOL_NAME(execute_deferred_task) "\n" // Returns original ret addr in x0
    "    add sp, sp, 16\n" // restore stack
    "    br x0\n" // jump to the original return address
);
#endif
}

class defer_impl {
public:
    template <typename F>
    __attribute__((always_inline)) // required
    defer_impl(F&& f) {
#if (defined(__GNUC__) || defined(__clang__)) && (defined(__x86_64__) || defined(__aarch64__))
        // This code assumes it is INLINED into the function that uses `defer_function`.
        // Therefore, level 0 refers to the frame/return address of that function.

        if (!final_return_address) {
            // 1. Get the frame pointer of the CURRENT function (not this constuctor), which better
            //    be compiled with -fno-omit-frame-pointer.
            void* current_frame_address = __builtin_frame_address(0);
            assert(current_frame_address != nullptr);


            // 2. Get the return address of the CURRENT function. This is the value we need
            //    to save and restore later.
            //
            //    Original return adress returned would be our trampoline, if we already have
            //    defer statements
            void* original_return_address = __builtin_return_address(0);

            assert(original_return_address != nullptr);
            
            // 3. Find the location ON THE STACK where the return address is stored.
            //    For standard frame layouts on x86-64 and AArch64, this is at:
            //    [frame_pointer + sizeof(void*)]
            void** return_address_ptr_on_stack = (void**)((char*)current_frame_address + sizeof(void*));

            // 4. Sanity Check: Does the memory location we calculated actually contain the
            //    return address we expect? This confirms our stack layout assumption is correct.
            assert(*return_address_ptr_on_stack == original_return_address);
            
            // 5. Success! Push the task and the original address, then overwrite the
            //    return address on the stack with the address of our trampoline.
            final_return_address = original_return_address;
            *return_address_ptr_on_stack = (void*)&defer_trampoline;
        } 
        deferred_task_stack.push_back(std::forward<F>(f));

#else
        static_assert(false, "Function-level defer is not supported on this platform/compiler combination.");
#endif
    }
};

} // namespace defer_private

#define DEFER_FUNC_CONCAT_IMPL(a, b) a##b
#define DEFER_FUNC_CONCAT(a, b) DEFER_FUNC_CONCAT_IMPL(a, b)
#define DEFER_FUNC_UNIQUE_NAME(prefix) DEFER_FUNC_CONCAT(prefix, __LINE__)

#define defer \
    [[maybe_unused]] ::defer_private::defer_impl DEFER_FUNC_UNIQUE_NAME(DEFER_FUNC_DUMMY) =     

#endif // FUNC_DEFER_H
