#include <fstream>
#include <string>
#include <chrono>
#include <cstdio>
#include <cstring>

// WasmEdge C API
#include <wasmedge/wasmedge.h>

// ============================================================
// Print WasmEdge Result error
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
  // Get file from input parameter
  if (argc < 2) {
    fprintf(stderr, "[ERROR] Usage: %s <wasm_file>\n", argv[0]);
    return 1;
  }

  const char* WASM_FILE = argv[1];

  // Check if file exists
  std::ifstream infile(WASM_FILE);
  if (!infile.good()) {
    fprintf(stderr, "[ERROR] File not found: %s\n", WASM_FILE);
    return 1;
  }

  fprintf(stdout, "================================================\n");
  fprintf(stdout, "  WasmEdge C++ Runner - %s\n", WASM_FILE);
  fprintf(stdout, "  WasmEdge version: %s\n", WasmEdge_VersionGet());
  fprintf(stdout, "================================================\n\n");

  // ----------------------------------------------------------
  // 1. Create Configure (enable WASI)
  // ----------------------------------------------------------
  WasmEdge_ConfigureContext* cfg = WasmEdge_ConfigureCreate();
  if (!cfg) {
    fprintf(stderr, "[ERROR] WasmEdge_ConfigureCreate() failed\n");
    return 1;
  }

  // Enable WASI proposal
  WasmEdge_ConfigureAddHostRegistration(cfg, WasmEdge_HostRegistration_Wasi);
  fprintf(stdout, "[INFO] Configure created successfully, WASI enabled\n");

  // ----------------------------------------------------------
  // 2. Create VM
  //    VM internally manages Store, Loader, Validator, Executor
  // ----------------------------------------------------------
  auto start_time = std::chrono::high_resolution_clock::now();
  WasmEdge_VMContext* vm = WasmEdge_VMCreate(cfg, nullptr);
  auto end_time = std::chrono::high_resolution_clock::now();
  if (!vm) {
      fprintf(stderr, "[ERROR] WasmEdge_VMCreate() failed\n");
      WasmEdge_ConfigureDelete(cfg);
      return 1;
  }
  auto duration =
    std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
  fprintf(stdout, "[INFO] VM created successfully in %ld µs\n", duration);

  // ----------------------------------------------------------
  // 3. Configure WASI environment
  //    Get the WASI Import Object from the VM and configure it
  // ----------------------------------------------------------
  WasmEdge_ModuleInstanceContext* wasi_module =
    WasmEdge_VMGetImportModuleContext(vm, WasmEdge_HostRegistration_Wasi);

  if (!wasi_module) {
      fprintf(stderr, "[ERROR] Failed to get WASI module\n");
      WasmEdge_VMDelete(vm);
      WasmEdge_ConfigureDelete(cfg);
      return 1;
  }

  // Set argv
  // const char* wasi_argv[] = { nullptr };
  // Set environment variables (optional, here we pass empty)
  // const char* wasi_envs[] = { nullptr };
  // Set preopened directories (optional, here we pass empty)
  // const char* wasi_dirs[] = { nullptr };
  // Set preopened sockets (optional, here we pass empty)
  // const char* wasi_preopens[] = { nullptr };

  WasmEdge_ModuleInstanceInitWASI(
    wasi_module,
    nullptr, 0,       // argv, argc
    nullptr, 0,       // envs,  env count  (no environment variables)
    nullptr,   0        // dirs,  dir count   (no preopened directories)
  );

  fprintf(stdout, "[INFO] WASI environment configured successfully\n");

  // ----------------------------------------------------------
  // 4. Load, validate, instantiate, and run
  // ----------------------------------------------------------
  fprintf(stdout, "[INFO] Starting to load and execute %s ...\n", WASM_FILE);
  fprintf(stdout, "------------------------------------------------\n\n");

  // _start function name in WASI modules is conventionally "_start"
  WasmEdge_String start_func = WasmEdge_StringCreateByCString("_start");

  // Step 1: Load WASM file (include compile)
  fprintf(stdout, "[INFO] Step 1: Loading WASM file...\n");
  start_time = std::chrono::high_resolution_clock::now();
  WasmEdge_Result result = WasmEdge_VMLoadWasmFromFile(vm, WASM_FILE);
  end_time = std::chrono::high_resolution_clock::now();
  if (!WasmEdge_ResultOK(result)) {
      fprintf(stderr, "[ERROR] Failed to load WASM file\n");
      print_wasmedge_error(result);
      WasmEdge_StringDelete(start_func);
      WasmEdge_VMDelete(vm);
      WasmEdge_ConfigureDelete(cfg);
      return 1;
  }
  duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
  fprintf(stdout, "[INFO] WASM file loaded and compiled successfully in %ld µs\n", duration);

  // Step 2: Validate WASM module
  fprintf(stdout, "[INFO] Step 2: Validating WASM module...\n");
  result = WasmEdge_VMValidate(vm);
  if (!WasmEdge_ResultOK(result)) {
      fprintf(stderr, "[ERROR] Failed to validate WASM module\n");
      print_wasmedge_error(result);
      WasmEdge_StringDelete(start_func);
      WasmEdge_VMDelete(vm);
      WasmEdge_ConfigureDelete(cfg);
      return 1;
  }
  fprintf(stdout, "[INFO] WASM module validated successfully\n");

  // Step 3: Instantiate WASM module
  fprintf(stdout, "[INFO] Step 3: Instantiating WASM module...\n");
  start_time = std::chrono::high_resolution_clock::now();
  result = WasmEdge_VMInstantiate(vm);
  end_time = std::chrono::high_resolution_clock::now();
  if (!WasmEdge_ResultOK(result)) {
      fprintf(stderr, "[ERROR] Failed to instantiate WASM module\n");
      print_wasmedge_error(result);
      WasmEdge_StringDelete(start_func);
      WasmEdge_VMDelete(vm);
      WasmEdge_ConfigureDelete(cfg);
      return 1;
  }
  duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
  fprintf(stdout, "[INFO] WASM module instantiated successfully in %ld µs\n", duration);

  // Step 4: Execute _start function
  fprintf(stdout, "[INFO] Step 4: Executing _start function...\n");
  result = WasmEdge_VMExecute(
      vm,
      start_func,
      nullptr, 0,    // arguments list, argument count
      nullptr, 0     // return values list, return values count
  );

  WasmEdge_StringDelete(start_func);

  fprintf(stdout, "\n------------------------------------------------\n");

  // ----------------------------------------------------------
  // 5. Handle execution result
  // ----------------------------------------------------------
  int exit_code = 0;

  if (WasmEdge_ResultOK(result)) {
      fprintf(stdout, "[INFO] benchmark.wasm executed successfully\n");
  } else {
      // Check if it is a WASI exit() normal exit
      // In WasmEdge, the error code for WASI exit is WasmEdge_ErrCode_Terminated
      uint32_t err_code = WasmEdge_ResultGetCode(result);

      if (WasmEdge_ResultGetCategory(result) == WasmEdge_ErrCategory_WASM &&
          err_code == 0x07 /* Terminated */) {
          // Get WASI exit code
          uint32_t wasi_exit = WasmEdge_ModuleInstanceWASIGetExitCode(wasi_module);
          fprintf(stdout, "[INFO] Program exited normally (WASI exit code = %u)\n", wasi_exit);
          exit_code = (int)wasi_exit;
      } else {
          fprintf(stderr, "[ERROR] Execution failed\n");
          print_wasmedge_error(result);
          exit_code = 1;
      }
  }

  // ----------------------------------------------------------
  // 6. Clean up resources
  // ----------------------------------------------------------
  fprintf(stdout, "[INFO] Cleaning up resources...\n");
  WasmEdge_VMDelete(vm);
  WasmEdge_ConfigureDelete(cfg);

  fprintf(stdout, "[INFO] Done, exit code = %d\n", exit_code);
  return exit_code;
}