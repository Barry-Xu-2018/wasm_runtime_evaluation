#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

/* WAMR C API */
#include "wasm_export.h"

/*
 * Read aot wasm file into a byte buffer
 */
static uint8_t *read_wasm_file(const char *filename, size_t *out_size)
{
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "[ERROR] Failed to open file: %s\n", filename);
        exit(1);
    }

    /* Seek to end to get file size */
    if (fseek(fp, 0, SEEK_END) != 0) {
        fprintf(stderr, "[ERROR] Failed to seek file: %s\n", filename);
        fclose(fp);
        exit(1);
    }
    long size = ftell(fp);
    if (size < 0) {
        fprintf(stderr, "[ERROR] Failed to get file size: %s\n", filename);
        fclose(fp);
        exit(1);
    }
    rewind(fp);

    uint8_t *buf = (uint8_t *)malloc((size_t)size);
    if (!buf) {
        fprintf(stderr, "[ERROR] Failed to allocate memory for file: %s\n", filename);
        fclose(fp);
        exit(1);
    }

    if (fread(buf, 1, (size_t)size, fp) != (size_t)size) {
        fprintf(stderr, "[ERROR] Failed to read file: %s\n", filename);
        free(buf);
        fclose(fp);
        exit(1);
    }
    fclose(fp);

    *out_size = (size_t)size;
    fprintf(stdout, "[INFO] Successfully read %s, size: %zu bytes\n",
            filename, (size_t)size);
    return buf;
}

/* Helper: return microseconds elapsed between two struct timespec values */
static long timespec_diff_us(struct timespec *start, struct timespec *end)
{
    return (long)((end->tv_sec  - start->tv_sec)  * 1000000L +
                  (end->tv_nsec - start->tv_nsec) / 1000L);
}

int main(int argc, char *argv[])
{
    /* Parse command-line arguments */
    if (argc < 2) {
        fprintf(stderr, "[ERROR] Usage: %s <aot_wasm_file>\n", argv[0]);
        return 1;
    }
    const char *WASM_FILE = argv[1];
    static char global_heap_buf[256 * 1024];

    /* Print WAMR version banner */
    uint32_t major, minor, patch;
    wasm_runtime_get_version(&major, &minor, &patch);
    fprintf(stdout, "================================================\n");
    fprintf(stdout, "[INFO] WAMR Version: %u.%u.%u\n", major, minor, patch);
    fprintf(stdout, "       WAMR C Runner - %s\n", WASM_FILE);
    fprintf(stdout, "================================================\n\n");

    /* Read the file into memory */
    size_t   wasm_size  = 0;
    uint8_t *wasm_bytes = read_wasm_file(WASM_FILE, &wasm_size);

    /* Error buffer for WAMR API calls */
    char error_buf[512];
    memset(error_buf, 0, sizeof(error_buf));

    struct timespec start_time, end_time;
    long duration;

    /*
     * Step 1. Initialize WAMR Runtime
     *
     *   - The binary already contains native machine code compiled
     *     by wamrc; no JIT compilation is needed at runtime.
     *   - Use Mode_Interp so that WAMR initializes only the
     *     lightweight AOT loader/executor, skipping the LLVM JIT
     *     engine entirely.  This reduces startup time and memory.
     */
    fprintf(stdout, "\n--- Step 1: Initialize Runtime ---\n");

    RuntimeInitArgs init_args;
    memset(&init_args, 0, sizeof(init_args));

    init_args.running_mode = Mode_Interp;
    init_args.mem_alloc_type = Alloc_With_Pool;
    init_args.mem_alloc_option.pool.heap_buf  = global_heap_buf;
    init_args.mem_alloc_option.pool.heap_size = sizeof(global_heap_buf);
    fprintf(stdout, "[INFO] Running mode: AOT (native pre-compiled)\n");

    /* Use the system allocator for simplicity */
    init_args.mem_alloc_type = Alloc_With_System_Allocator;

    clock_gettime(CLOCK_MONOTONIC, &start_time);
    bool ret = wasm_runtime_full_init(&init_args);
    clock_gettime(CLOCK_MONOTONIC, &end_time);

    if (!ret) {
        fprintf(stderr, "[ERROR] wasm_runtime_full_init() failed\n");
        free(wasm_bytes);
        return 1;
    }

    duration = timespec_diff_us(&start_time, &end_time);
    fprintf(stdout,
            "[INFO] Runtime initialized successfully in %ld microseconds\n",
            duration);

    /*
     * Step 2. LOAD
     *
     * wasm_runtime_load() inspects the first 4 magic bytes of the
     * buffer and automatically dispatches to:
     *   - The WASM bytecode loader  if magic == "\0asm"
     *   - The AOT binary loader     if magic == "\0aot"
     *
     * No code change is required here; the same API handles both.
     */
    fprintf(stdout, "\n--- Step 2: Load ---\n");

    clock_gettime(CLOCK_MONOTONIC, &start_time);
    wasm_module_t module = wasm_runtime_load(
        wasm_bytes,
        (uint32_t)wasm_size,
        error_buf,
        sizeof(error_buf)
    );
    clock_gettime(CLOCK_MONOTONIC, &end_time);

    if (!module) {
        fprintf(stderr, "[ERROR] wasm_runtime_load() failed: %s\n", error_buf);
        free(wasm_bytes);
        wasm_runtime_destroy();
        return 1;
    }

    duration = timespec_diff_us(&start_time, &end_time);
    fprintf(stdout, "[INFO] Load successful in %ld microseconds\n", duration);

    /*
     * Step 3. INSTANTIATE
     *   Create a module instance (wasm_module_inst_t).
     *   WAMR validates the module during instantiation.
     *
     *   stack_size : Wasm operand/call stack size (bytes)
     *   heap_size  : Wasm linear-memory managed heap size (bytes)
     */
    fprintf(stdout, "\n--- Step 3: Instantiate ---\n");

    const uint32_t STACK_SIZE = 256 * 1024;  /* 256 KB */
    const uint32_t HEAP_SIZE  = 256 * 1024;  /* 256 KB */

    clock_gettime(CLOCK_MONOTONIC, &start_time);
    wasm_module_inst_t module_inst = wasm_runtime_instantiate(
        module,
        STACK_SIZE,
        HEAP_SIZE,
        error_buf,
        sizeof(error_buf)
    );
    clock_gettime(CLOCK_MONOTONIC, &end_time);

    if (!module_inst) {
        fprintf(stderr,
                "[ERROR] wasm_runtime_instantiate() failed: %s\n", error_buf);
        free(wasm_bytes);
        wasm_runtime_unload(module);
        wasm_runtime_destroy();
        return 1;
    }

    duration = timespec_diff_us(&start_time, &end_time);
    fprintf(stdout,
            "[INFO] Instantiate successful in %ld microseconds\n", duration);
    fprintf(stdout, "       stack_size = %u bytes\n", STACK_SIZE);
    fprintf(stdout, "       heap_size  = %u bytes\n", HEAP_SIZE);

    /*
     * Step 4. Find _start function
     *   For WASI programs the canonical entry point is _start.
     *   wasm_runtime_lookup_wasi_start_function() is preferred
     *   because it also handles WASI reactor modules correctly.
     */
    fprintf(stdout, "\n--- Step 4: Find _start ---\n");

    wasm_function_inst_t start_func =
        wasm_runtime_lookup_wasi_start_function(module_inst);

    if (!start_func) {
        /* Fallback: look up the export by name directly */
        start_func = wasm_runtime_lookup_function(module_inst, "_start");
    }

    if (!start_func) {
        fprintf(stderr, "[ERROR] _start function not found\n");
        free(wasm_bytes);
        wasm_runtime_deinstantiate(module_inst);
        wasm_runtime_unload(module);
        wasm_runtime_destroy();
        return 1;
    }
    fprintf(stdout, "[INFO] _start function found\n");

    /*
     * Step 5. Create Execution Environment and EXECUTE
     *   wasm_exec_env_t is the per-thread execution context.
     *   The stack_size here is the Wasm operand stack size.
     */
    fprintf(stdout, "\n--- Step 5: Execute ---\n");
    fprintf(stdout, "[INFO] Calling _start() ...\n");
    fprintf(stdout, "================================================\n\n");

    wasm_exec_env_t exec_env =
        wasm_runtime_create_exec_env(module_inst, STACK_SIZE);

    if (!exec_env) {
        fprintf(stderr, "[ERROR] wasm_runtime_create_exec_env() failed\n");
        free(wasm_bytes);
        wasm_runtime_deinstantiate(module_inst);
        wasm_runtime_unload(module);
        wasm_runtime_destroy();
        return 1;
    }

    /* Call _start(): no arguments, no return value */
    bool ok = wasm_runtime_call_wasm(exec_env, start_func, 0, NULL);

    fprintf(stdout, "\n================================================\n");

    /*
     * Step 6. Handle execution result
     */
    int exit_code = 0;

    if (ok) {
        fprintf(stdout, "[INFO] _start() executed successfully\n");
    } else {
        const char *exception = wasm_runtime_get_exception(module_inst);

        if (exception) {
            /* WASI programs call proc_exit(N) which surfaces as:
             * "Exception: wasi proc exit(N)"
             * Parse the exit code from the exception string. */
            int wasi_exit_code = 0;
            if (sscanf(exception,
                       "Exception: wasi proc exit(%d)",
                       &wasi_exit_code) == 1)
            {
                fprintf(stdout,
                        "[INFO] WASI exited normally, exit code = %d\n",
                        wasi_exit_code);
                exit_code = wasi_exit_code;
            } else {
                fprintf(stderr,
                        "[ERROR] Execution exception: %s\n", exception);
                exit_code = 1;
            }
        } else {
            fprintf(stderr, "[ERROR] Execution failed (unknown error)\n");
            exit_code = 1;
        }
    }

    /*
     * Step 7. Clean up resources (reverse order of acquisition)
     */
    fprintf(stdout, "\n--- Step 7: Clean up resources ---\n");

    wasm_runtime_destroy_exec_env(exec_env);
    fprintf(stdout, "[INFO] exec_env    released\n");

    wasm_runtime_deinstantiate(module_inst);
    fprintf(stdout, "[INFO] module_inst released\n");

    wasm_runtime_unload(module);
    fprintf(stdout, "[INFO] module      released\n");

    wasm_runtime_destroy();
    fprintf(stdout, "[INFO] runtime     destroyed\n");

    free(wasm_bytes);

    fprintf(stdout, "\n[INFO] Done, exit code = %d\n", exit_code);
    return exit_code;
}
