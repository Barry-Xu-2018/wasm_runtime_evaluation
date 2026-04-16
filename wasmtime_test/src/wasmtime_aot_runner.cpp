#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <chrono>
#include <cstdio>
#include <cstring>

#include <wasmtime.h>
#include <wasm.h>

// ============================================================
// Print wasmtime error / trap information
// ============================================================
static void print_error(wasmtime_error_t* error, wasm_trap_t* trap)
{
  wasm_byte_vec_t msg;
  wasm_byte_vec_new_empty(&msg);

  if (error) {
    wasmtime_error_message(error, &msg);
    fprintf(stderr, "[ERROR] %.*s\n", (int)msg.size, msg.data);
    wasmtime_error_delete(error);
  }
  if (trap) {
    wasm_trap_message(trap, &msg);
    fprintf(stderr, "[TRAP]  %.*s\n", (int)msg.size, msg.data);
    wasm_trap_delete(trap);
  }

  wasm_byte_vec_delete(&msg);
}

// ============================================================
// main
// ============================================================
int main(int argc, char* argv[])
{
    // Get file from input parameter
    if (argc < 2) {
        fprintf(stderr, "[ERROR] Usage: %s <wasm_file_or_cwasm_file>\n", argv[0]);
        return 1;
    }
    const std::string WASM_FILE = argv[1];

    // Output the version of wasmtime
    fprintf(stdout, "[INFO] Wasmtime version: %s\n", WASMTIME_VERSION);

    // ----------------------------------------------------------
    // 1. Create Engine
    // ----------------------------------------------------------
    auto start_time = std::chrono::high_resolution_clock::now();
    wasm_engine_t* engine = wasm_engine_new();
    auto end_time = std::chrono::high_resolution_clock::now();
    if (!engine) {
        fprintf(stderr, "[ERROR] wasm_engine_new() failed\n");
        return 1;
    }

    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
    fprintf(stdout, "[INFO] Engine created successfully in %ld µs\n", duration);

    // ----------------------------------------------------------
    // 2. Create Store
    // ----------------------------------------------------------
    wasmtime_store_t*   store   = wasmtime_store_new(engine, nullptr, nullptr);
    wasmtime_context_t* context = wasmtime_store_context(store);

    // ----------------------------------------------------------
    // 3. Configure WASI
    //    - Inherit host's stdout / stderr / stdin
    // ----------------------------------------------------------
    wasi_config_t* wasi_cfg = wasi_config_new();
    wasi_config_inherit_stdout(wasi_cfg);
    wasi_config_inherit_stderr(wasi_cfg);
    wasi_config_inherit_stdin(wasi_cfg);

    // Transfer ownership of wasi_config to the store
    wasmtime_error_t* error = wasmtime_context_set_wasi(context, wasi_cfg);
    if (error) {
        fprintf(stderr, "[ERROR] wasmtime_context_set_wasi() failed\n");
        print_error(error, nullptr);
        wasmtime_store_delete(store);
        wasm_engine_delete(engine);
        return 1;
    }
    fprintf(stdout, "[INFO] WASI environment configured successfully\n");

    // ----------------------------------------------------------
    // 4. Load module — AOT Wasm
    // ----------------------------------------------------------
    wasmtime_module_t* module = nullptr;
    start_time = std::chrono::high_resolution_clock::now();

    // Directly from file path (no need to read bytes first):
    error = wasmtime_module_deserialize_file(engine,WASM_FILE.c_str(), &module);

    end_time = std::chrono::high_resolution_clock::now();

    if (error || !module) {
        fprintf(stderr, "[ERROR] Failed to load module (AOT deserialize)\n");
        print_error(error, nullptr);
        wasmtime_store_delete(store);
        wasm_engine_delete(engine);
        return 1;
    }

    duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
    fprintf(stdout, "[INFO] Module loaded successfully in %ld µs (AOT)\n", duration);

    // ----------------------------------------------------------
    // 5. Create Linker and register WASI
    // ----------------------------------------------------------
    wasmtime_linker_t* linker = wasmtime_linker_new(engine);

    error = wasmtime_linker_define_wasi(linker);
    if (error) {
        fprintf(stderr, "[ERROR] wasmtime_linker_define_wasi() failed\n");
        print_error(error, nullptr);
        wasmtime_linker_delete(linker);
        wasmtime_module_delete(module);
        wasmtime_store_delete(store);
        wasm_engine_delete(engine);
        return 1;
    }
    fprintf(stdout, "[INFO] WASI registered to Linker successfully\n");

    // ----------------------------------------------------------
    // 6. Instantiate module
    // ----------------------------------------------------------
    wasmtime_instance_t instance;
    wasm_trap_t*        trap = nullptr;

    start_time = std::chrono::high_resolution_clock::now();
    error = wasmtime_linker_instantiate(linker, context, module,
                                        &instance, &trap);
    end_time = std::chrono::high_resolution_clock::now();
    if (error || trap) {
        fprintf(stderr, "[ERROR] Failed to instantiate module\n");
        print_error(error, trap);
        wasmtime_linker_delete(linker);
        wasmtime_module_delete(module);
        wasmtime_store_delete(store);
        wasm_engine_delete(engine);
        return 1;
    }
    duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
    fprintf(stdout, "[INFO] Module instantiated successfully in %ld µs\n", duration);

    // ----------------------------------------------------------
    // 7. Get and call _start (WASI entry point)
    // ----------------------------------------------------------
    wasmtime_extern_t start_extern;
    bool found = wasmtime_instance_export_get(
        context, &instance,
        "_start", strlen("_start"),
        &start_extern);

    if (!found || start_extern.kind != WASMTIME_EXTERN_FUNC) {
        fprintf(stderr, "[ERROR] Failed to find exported function _start\n");
        wasmtime_linker_delete(linker);
        wasmtime_module_delete(module);
        wasmtime_store_delete(store);
        wasm_engine_delete(engine);
        return 1;
    }

    fprintf(stdout, "[INFO] Found _start, starting execution...\n");
    fprintf(stdout, "================================================\n\n");

    error = wasmtime_func_call(context,
                               &start_extern.of.func,
                               nullptr, 0,
                               nullptr, 0,
                               &trap);

    fprintf(stdout, "\n================================================\n");

    // ----------------------------------------------------------
    // 8. Handle execution result
    // ----------------------------------------------------------
    int exit_code = 0;

    if (error) {
        int status = 0;
        bool is_exit = wasmtime_error_exit_status(error, &status);
        if (is_exit) {
            exit_code = status;
            fprintf(stdout, "[INFO] Program exited normally, exit code = %d\n", status);
            wasmtime_error_delete(error);
        } else {
            fprintf(stderr, "[ERROR] Runtime error occurred\n");
            print_error(error, nullptr);
            exit_code = 1;
        }
    } else if (trap) {
        fprintf(stderr, "[ERROR] Trap occurred\n");
        print_error(nullptr, trap);
        exit_code = 1;
    } else {
        fprintf(stdout, "[INFO] _start() returned normally\n");
    }

    // ----------------------------------------------------------
    // 9. Clean up resources
    // ----------------------------------------------------------
    wasmtime_linker_delete(linker);
    wasmtime_module_delete(module);
    wasmtime_store_delete(store);
    wasm_engine_delete(engine);

    return exit_code;
}
