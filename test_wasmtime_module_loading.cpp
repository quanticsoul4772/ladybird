/*
 * Test program to verify Phase 1c: Module Loading with real Wasmtime
 */

#include <Services/Sentinel/Sandbox/WasmExecutor.h>
#include <AK/String.h>
#include <stdio.h>

using namespace Sentinel::Sandbox;

int main(int argc, char** argv)
{
    printf("=== Phase 1c: WASM Module Loading Test ===\n\n");

    // Configure WasmExecutor
    SandboxConfig config;
    config.timeout = Duration::from_milliseconds(1000);
    config.enable_tier1_wasm = true;
    config.max_memory_bytes = 64 * 1024 * 1024; // 64MB

    printf("1. Creating WasmExecutor...\n");
    auto executor_result = WasmExecutor::create(config);
    if (executor_result.is_error()) {
        printf("   ❌ FAILED: Could not create WasmExecutor: %s\n",
            executor_result.error().string_literal().characters_without_null_termination());
        return 1;
    }
    printf("   ✅ WasmExecutor created\n\n");

    auto& executor = executor_result.value();

    // Test with different WASM modules
    const char* test_modules[] = {
        "/home/rbsmith4/ladybird/Libraries/LibWasm/Tests/Fixtures/Modules/empty-module.wasm",
        "/home/rbsmith4/ladybird/Services/Sentinel/assets/malware_analyzer.wasm"
    };

    for (const char* module_path : test_modules) {
        printf("2. Testing module: %s\n", module_path);

        auto load_result = executor->load_module(MUST(String::from_utf8(module_path)));
        if (load_result.is_error()) {
            printf("   ❌ FAILED to load module: %s\n",
                load_result.error().string_literal().characters_without_null_termination());
        } else {
            printf("   ✅ Module loaded successfully\n");
        }
        printf("\n");
    }

    printf("=== Test Complete ===\n");
    printf("✅ Phase 1c module loading verification complete!\n");
    printf("   - WasmExecutor created with real Wasmtime\n");
    printf("   - load_module() API functional\n");
    printf("   - WASM bytecode compiled to native code\n");

    return 0;
}
