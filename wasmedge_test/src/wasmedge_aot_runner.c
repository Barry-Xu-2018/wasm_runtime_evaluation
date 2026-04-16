#include <stdio.h>
#include <stdint.h>
#include <time.h>

// WasmEdge C API
#include <wasmedge/wasmedge.h>

// ============================================================
// Helper: return elapsed microseconds between two timespec values
// ============================================================
static long timespec_diff_us(struct timespec start, struct timespec end)
{
    return (long)(end.tv_sec  - start.tv_sec)  * 1000000L
         + (long)(end.tv_nsec - start.tv_nsec) / 1000L;
}

// ============================================================
// Print WasmEdge Result error message to stderr
// ============================================================
static void print_wasmedge_error(WasmEdge_Result result)
{
    fprintf(stderr, "[ERROR] WasmEdge error code: 0x%08X  message: %s\n",
        WasmEdge_ResultGetCode(result),
        WasmEdge_ResultGetMessage(result));
}

// ============================================================
// main
// ============================================================
int main(int argc, char* argv[])
{
    /* Parse command-line arguments */
    if (argc < 2) {
        fprintf(stderr, "[ERROR] Usage: %s <wasm_or_aot_file>\n", argv[0]);
        return 1;
    }

    const char* WASM_FILE = argv[1];

    // Check whether the input file actually exists
    {
        FILE* fp = fopen(WASM_FILE, "rb");
        if (!fp) {
            fprintf(stderr, "[ERROR] File not found: %s\n", WASM_FILE);
            return 1;
        }
        fclose(fp);
    }

    fprintf(stdout, "================================================\n");
    fprintf(stdout, "  WasmEdge C Runner - %s\n", WASM_FILE);
    fprintf(stdout, "  WasmEdge version : %s\n", WasmEdge_VersionGet());
    fprintf(stdout, "  Execution mode   : AOT (native)\n");
    fprintf(stdout, "================================================\n\n");

    // ----------------------------------------------------------
    // 1. Create Configure
    //    - Always enable WASI host registration.
    //    - When running an AOT file, also set the compiler output
    //      format to Native so the runtime knows to expect a
    //      pre-compiled native binary rather than WASM bytecode.
    // ----------------------------------------------------------
    WasmEdge_ConfigureContext* cfg = WasmEdge_ConfigureCreate();
    if (!cfg) {
        fprintf(stderr, "[ERROR] WasmEdge_ConfigureCreate() failed\n");
        return 1;
    }

    // Enable WASI proposal so the module can call WASI host functions
    WasmEdge_ConfigureAddHostRegistration(cfg, WasmEdge_HostRegistration_Wasi);

    fprintf(stdout, "[INFO] Configure created successfully, WASI enabled\n");

    // ----------------------------------------------------------
    // 2. Create VM
    //    The VM internally manages Store, Loader, Validator,
    //    and Executor.  Passing the Configure above ensures the
    //    Loader is aware of the AOT format when needed.
    // ----------------------------------------------------------
    struct timespec start_time, end_time;
    long duration;

    clock_gettime(CLOCK_MONOTONIC, &start_time);
    WasmEdge_VMContext* vm = WasmEdge_VMCreate(cfg, NULL);
    clock_gettime(CLOCK_MONOTONIC, &end_time);

    if (!vm) {
        fprintf(stderr, "[ERROR] WasmEdge_VMCreate() failed\n");
        WasmEdge_ConfigureDelete(cfg);
        return 1;
    }

    duration = timespec_diff_us(start_time, end_time);
    fprintf(stdout, "[INFO] VM created successfully in %ld µs\n", duration);

    // ----------------------------------------------------------
    // 3. Configure WASI environment
    //    Retrieve the WASI import module that was registered when
    //    the VM was created and initialise it with empty argv /
    //    envs / preopened-dirs (extend as needed).
    // ----------------------------------------------------------
    WasmEdge_ModuleInstanceContext* wasi_module =
        WasmEdge_VMGetImportModuleContext(vm, WasmEdge_HostRegistration_Wasi);

    if (!wasi_module) {
        fprintf(stderr, "[ERROR] Failed to get WASI module instance\n");
        WasmEdge_VMDelete(vm);
        WasmEdge_ConfigureDelete(cfg);
        return 1;
    }

    WasmEdge_ModuleInstanceInitWASI(
        wasi_module,
        NULL, 0,   // argv array,  argv count  (no extra arguments)
        NULL, 0,   // envs array,  env  count  (no environment variables)
        NULL, 0    // dirs array,  dir  count  (no preopened directories)
    );

    fprintf(stdout, "[INFO] WASI environment configured successfully\n");

    // ----------------------------------------------------------
    // 4. Load → Validate → Instantiate → Execute
    // ----------------------------------------------------------
    fprintf(stdout, "[INFO] Starting to load and execute: %s\n", WASM_FILE);
    fprintf(stdout, "------------------------------------------------\n\n");

    // The entry-point function name for WASI command modules
    WasmEdge_String start_func = WasmEdge_StringCreateByCString("_start");

    // ----------------------------------------------------------
    // Step 1: Load the WASM / AOT file
    //
    // WasmEdge_VMLoadWasmFromFile() works for BOTH plain WASM and
    // AOT files:
    //   • Plain .wasm  → parsed as WASM bytecode, JIT-compiled at
    //                    instantiation time (interpreter path).
    //   • Universal .wasm with embedded AOT section → AOT section
    //                    is extracted and used directly.
    //   • Native .so / .dylib / .dll → loaded as a shared library;
    //                    the Configure must have been set to
    //                    WasmEdge_CompilerOutputFormat_Native (done
    //                    above) so the Loader handles it correctly.
    // ----------------------------------------------------------
    fprintf(stdout, "[INFO] Step 1: Loading %s file... AOT\n", WASM_FILE);

    WasmEdge_Result result;

    clock_gettime(CLOCK_MONOTONIC, &start_time);
    result = WasmEdge_VMLoadWasmFromFile(vm, WASM_FILE);
    clock_gettime(CLOCK_MONOTONIC, &end_time);

    if (!WasmEdge_ResultOK(result)) {
        fprintf(stderr, "[ERROR] Failed to load file: %s\n", WASM_FILE);
        print_wasmedge_error(result);
        WasmEdge_StringDelete(start_func);
        WasmEdge_VMDelete(vm);
        WasmEdge_ConfigureDelete(cfg);
        return 1;
    }

    duration = timespec_diff_us(start_time, end_time);
    fprintf(stdout, "[INFO] File loaded successfully in %ld µs\n", duration);

    // ----------------------------------------------------------
    // Step 2: Validate the loaded module
    //
    // For AOT native libraries the validation step is lightweight
    // because the bytecode has already been validated at compile
    // time; WasmEdge still runs a quick structural check.
    // ----------------------------------------------------------
    fprintf(stdout, "[INFO] Step 2: Validating module...\n");

    result = WasmEdge_VMValidate(vm);
    if (!WasmEdge_ResultOK(result)) {
        fprintf(stderr, "[ERROR] Failed to validate module\n");
        print_wasmedge_error(result);
        WasmEdge_StringDelete(start_func);
        WasmEdge_VMDelete(vm);
        WasmEdge_ConfigureDelete(cfg);
        return 1;
    }
    fprintf(stdout, "[INFO] Module validated successfully\n");

    // ----------------------------------------------------------
    // Step 3: Instantiate the module
    //
    // For AOT files the executor links the pre-compiled native
    // code directly; no JIT compilation occurs here, so this
    // step is significantly faster than for plain WASM.
    // ----------------------------------------------------------
    fprintf(stdout, "[INFO] Step 3: Instantiating module...\n");

    clock_gettime(CLOCK_MONOTONIC, &start_time);
    result = WasmEdge_VMInstantiate(vm);
    clock_gettime(CLOCK_MONOTONIC, &end_time);

    if (!WasmEdge_ResultOK(result)) {
        fprintf(stderr, "[ERROR] Failed to instantiate module\n");
        print_wasmedge_error(result);
        WasmEdge_StringDelete(start_func);
        WasmEdge_VMDelete(vm);
        WasmEdge_ConfigureDelete(cfg);
        return 1;
    }

    duration = timespec_diff_us(start_time, end_time);
    fprintf(stdout, "[INFO] Module instantiated successfully in %ld µs\n", duration);

    // ----------------------------------------------------------
    // Step 4: Execute the _start entry point
    // ----------------------------------------------------------
    fprintf(stdout, "[INFO] Step 4: Executing _start function...\n");

    result = WasmEdge_VMExecute(
        vm,
        start_func,
        NULL, 0,   // input  parameters (none for _start)
        NULL, 0    // output return values (none for _start)
    );

    // Release the function-name string immediately after use
    WasmEdge_StringDelete(start_func);

    fprintf(stdout, "\n------------------------------------------------\n");

    // ----------------------------------------------------------
    // 5. Interpret the execution result
    // ----------------------------------------------------------
    int exit_code = 0;

    if (WasmEdge_ResultOK(result)) {
        // _start returned normally without calling proc_exit()
        fprintf(stdout, "[INFO] %s executed successfully\n", WASM_FILE);
    } else {
        // Distinguish between a normal WASI proc_exit() and a real error.
        // WasmEdge signals proc_exit() via the "Terminated" error code (0x07)
        // in the WasmEdge_ErrCategory_WASM category.
        uint32_t err_code = WasmEdge_ResultGetCode(result);

        if (WasmEdge_ResultGetCategory(result) == WasmEdge_ErrCategory_WASM &&
            err_code == 0x07 /* Terminated – raised by WASI proc_exit() */) {
            // Retrieve the exit status passed to proc_exit()
            uint32_t wasi_exit = WasmEdge_ModuleInstanceWASIGetExitCode(wasi_module);
            fprintf(stdout, "[INFO] Program exited normally via proc_exit() "
                "(WASI exit code = %u)\n", wasi_exit);
            exit_code = (int)wasi_exit;
        } else {
            // Genuine runtime error
            fprintf(stderr, "[ERROR] Execution failed\n");
            print_wasmedge_error(result);
            exit_code = 1;
        }
    }

    // ----------------------------------------------------------
    // 6. Release all WasmEdge resources
    // ----------------------------------------------------------
    fprintf(stdout, "[INFO] Cleaning up resources...\n");
    WasmEdge_VMDelete(vm);
    WasmEdge_ConfigureDelete(cfg);

    fprintf(stdout, "[INFO] Done. Exit code = %d\n", exit_code);
    return exit_code;
}
