#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>
#include <string>

// WAMR C API
#include "wasm_export.h"

// ============================================================
// Read wasm file
// ============================================================
static std::vector<uint8_t> read_wasm_file(const std::string& filename)
{
  std::ifstream file(filename, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    fprintf(stderr, "[ERROR] Failed to open file: %s\n", filename.c_str());
    exit(1);
  }

  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);

  std::vector<uint8_t> buf(static_cast<size_t>(size));
  if (!file.read(reinterpret_cast<char*>(buf.data()), size)) {
    fprintf(stderr, "[ERROR] Failed to read file: %s\n", filename.c_str());
    exit(1);
  }

  fprintf(stdout, "[INFO] Successfully read %s, size: %zu bytes\n",
    filename.c_str(), buf.size());
  return buf;
}

// ============================================================
// main
// ============================================================
int main(int argc, char* argv[])
{
  // Get file from input parameter
  if (argc < 2) {
    fprintf(stderr, "[ERROR] Usage: %s <wasm_file>\n", argv[0]);
    return 1;
  }
  const char* WASM_FILE = argv[1];

  fprintf(stdout, "================================================\n");
  fprintf(stdout, "  WAMR C++ Runner - %s\n", WASM_FILE);
  fprintf(stdout, "================================================\n\n");

  // ----------------------------------------------------------
  // Read wasm file
  // ----------------------------------------------------------
  std::vector<uint8_t> wasm_bytes = read_wasm_file(WASM_FILE);

  // Error buffer for WAMR API calls
  char error_buf[512] = { 0 };

  // ==========================================================
  // Step 1. Initialize WAMR Runtime
  // ==========================================================
  fprintf(stdout, "\n--- Step 1: Initialize Runtime ---\n");

  // RuntimeInitArgs is used to configure runtime parameters
  RuntimeInitArgs init_args;
  memset(&init_args, 0, sizeof(init_args));

  // Choose execution mode:
  //   Interpreter  = "interp"
  //   Fast JIT     = "fast-jit"
  //   LLVM JIT     = "llvm-jit"
  //   Multi-tier   = "multi-tier-jit"
  init_args.running_mode = Mode_LLVM_JIT;

  // Use system malloc for memory management (simpler)
  init_args.mem_alloc_type = Alloc_With_System_Allocator;

  auto start_time = std::chrono::high_resolution_clock::now();
  auto ret = wasm_runtime_full_init(&init_args);
  auto end_time = std::chrono::high_resolution_clock::now();
  if (!ret) {
    fprintf(stderr, "[ERROR] wasm_runtime_full_init() failed\n");
    return 1;
  }
  auto duration =
    std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
  fprintf(stdout, "[INFO] Runtime initialized successfully in %ld microseconds\n", duration);

  // ==========================================================
  // Step 2. LOAD (Do JIT)
  //   Parse wasm binary -> wasm_module_t
  // ==========================================================
  fprintf(stdout, "\n--- Step 2: Load ---\n");

  start_time = std::chrono::high_resolution_clock::now();
  wasm_module_t module = wasm_runtime_load(
      wasm_bytes.data(),
      static_cast<uint32_t>(wasm_bytes.size()),
      error_buf,
      sizeof(error_buf)
  );
  end_time = std::chrono::high_resolution_clock::now();

  if (!module) {
    fprintf(stderr, "[ERROR] wasm_runtime_load() failed: %s\n", error_buf);
    wasm_runtime_destroy();
    return 1;
  }
  duration =
    std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
  fprintf(stdout, "[INFO] Load successful in %ld microseconds\n", duration);

    // ==========================================================
    // Step 3. INSTANTIATE
    //   Create module instance -> wasm_module_inst_t
    //   WAMR automatically performs validation during instantiation
    //
    //   Parameters:
    //     stack_size : wasm call stack size (bytes)
    //     heap_size  : wasm linear memory heap size (bytes)
    // ==========================================================
    fprintf(stdout, "\n--- Step 3: Instantiate ---\n");

    const uint32_t STACK_SIZE = 256 * 1024;   //  256 KB
    const uint32_t HEAP_SIZE  = 256 * 1024;   //  256 KB

    start_time = std::chrono::high_resolution_clock::now();
    wasm_module_inst_t module_inst = wasm_runtime_instantiate(
        module,
        STACK_SIZE,
        HEAP_SIZE,
        error_buf,
        sizeof(error_buf)
    );
    end_time = std::chrono::high_resolution_clock::now();

    if (!module_inst) {
        fprintf(stderr, "[ERROR] wasm_runtime_instantiate() failed: %s\n",
                error_buf);
        wasm_runtime_unload(module);
        wasm_runtime_destroy();
        return 1;
    }
    duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
    fprintf(stdout, "[INFO] Instantiate successful in %ld microseconds\n", duration);

    fprintf(stdout, "       stack_size = %u bytes\n", STACK_SIZE);
    fprintf(stdout, "       heap_size  = %u bytes\n", HEAP_SIZE);

    // ==========================================================
    // Step 4. Find _start function
    //   Entry point for WASI programs is _start
    // ==========================================================
    fprintf(stdout, "\n--- Step 4: Find _start ---\n");

    wasm_function_inst_t start_func =
        wasm_runtime_lookup_wasi_start_function(module_inst);

    if (!start_func) {
        // Fallback: try to lookup by name directly
        start_func = wasm_runtime_lookup_function(module_inst, "_start");
    }

    if (!start_func) {
        fprintf(stderr, "[ERROR] _start function not found\n");
        wasm_runtime_deinstantiate(module_inst);
        wasm_runtime_unload(module);
        wasm_runtime_destroy();
        return 1;
    }
    fprintf(stdout, "[INFO] _start function found\n");

    // ==========================================================
    // Step 5. Create Execution Environment and EXECUTE
    //   wasm_exec_env_t is the execution context, one per thread
    // ==========================================================
    fprintf(stdout, "\n--- Step 5: Execute ---\n");
    fprintf(stdout, "[INFO] Calling _start() ...\n");
    fprintf(stdout, "================================================\n\n");

    // Create execution environment (stack_size is the native call stack size)
    wasm_exec_env_t exec_env =
        wasm_runtime_create_exec_env(module_inst, STACK_SIZE);

    if (!exec_env) {
        fprintf(stderr, "[ERROR] wasm_runtime_create_exec_env() failed\n");
        wasm_runtime_deinstantiate(module_inst);
        wasm_runtime_unload(module);
        wasm_runtime_destroy();
        return 1;
    }

    // Call _start(): no arguments, no return value
    bool ok = wasm_runtime_call_wasm(exec_env, start_func, 0, nullptr);

    fprintf(stdout, "\n================================================\n");

    // ==========================================================
    // Step 6. Handle execution result
    // ==========================================================
    int exit_code = 0;

    if (ok) {
        fprintf(stdout, "[INFO] _start() executed successfully\n");
    } else {
        // Get exception information
        const char* exception = wasm_runtime_get_exception(module_inst);

        if (exception) {
            // WASI exit() will throw an exception in the form of "Exception: wasi proc exit(N)"
            // Need to parse the exit code from it
            int wasi_exit_code = 0;
            if (sscanf(exception, "Exception: wasi proc exit(%d)",
                       &wasi_exit_code) == 1)
            {
                fprintf(stdout, "[INFO] WASI exited normally, exit code = %d\n",
                        wasi_exit_code);
                exit_code = wasi_exit_code;
            } else {
                fprintf(stderr, "[ERROR] Execution exception: %s\n", exception);
                exit_code = 1;
            }
        } else {
            fprintf(stderr, "[ERROR] Execution failed (unknown error)\n");
            exit_code = 1;
        }
    }

    // ==========================================================
    // Step 7. Clean up resources
    // ==========================================================
    fprintf(stdout, "\n--- Step 7: Clean up resources ---\n");

    wasm_runtime_destroy_exec_env(exec_env);
    fprintf(stdout, "[INFO] exec_env    released\n");

    wasm_runtime_deinstantiate(module_inst);
    fprintf(stdout, "[INFO] module_inst released\n");

    wasm_runtime_unload(module);
    fprintf(stdout, "[INFO] module      released\n");

    wasm_runtime_destroy();
    fprintf(stdout, "[INFO] runtime     destroyed\n");

    fprintf(stdout, "\n[INFO] Done, exit code = %d\n", exit_code);
    return exit_code;
}