https://github.com/bytecodealliance/wasmtime/releases/download/v43.0.1/wasmtime-v43.0.1-aarch64-linux-c-api.tar.xz# wasm_runtime_evaluation
Sample code for evaluating WASM runtimes.

My test environment is a Raspi 4 board with Ubuntu 22.04.  
The reason for using Ubuntu 22.04 is that the WAMR manual specifies using Ubuntu 22.04.

## Install WASM runtime environment

1. **wasmtime**

   ```bash
   $ curl https://wasmtime.dev/install.sh -sSf | bash
   ```
   Download the corresponding version of the C API SDK.
   e.g. https://github.com/bytecodealliance/wasmtime/releases/download/v43.0.1/wasmtime-v43.0.1-aarch64-linux-c-api.tar.xz.  
   Download it and unpack it to ${HOME}/.wasmtime

2. **wasmedge**

    ```bash
    $ curl -sSf https://raw.githubusercontent.com/WasmEdge/WasmEdge/master/utils/install.sh | bash
    ```
    Installed environment is located at ${HOME}/.wasmedge

3. **wamr**

   The official website does not provide an aarch64 SDK, so it is necessary to compile from the source code.  
   ```bash
   # 1. Download source codes
   $ git clone https://github.com/bytecodealliance/wasm-micro-runtime.git

   # 2. Checkout latest version
   $ cd wasm-micro-runtime
   $ git checkout -b WAMR-2.4.4 WAMR-2.4.4

   # 3. Install dependent packages
   $ sudo apt install -y build-essential cmake ccache python3-pip ninja-build

   # 4. build wamrc
   $ cd wamr-compiler
   $ ./build_llvm.sh # If compilation issues occur due to resource constraints, you can manually go to core/deps/llvm/build/ and run 'cmake --build . --target package -j 2'.
   $ mkdir build && cd build
   $ cmake ..
   $ cmake --build . -j 2

   # 5. build C API sdk and iwasm
   $ cd product-mini/platforms/linux
   $ mkdir build && cd build
   $ cmake .. \
    -DWAMR_BUILD_INTERP=1 \
    -DWAMR_BUILD_LIBC_WASI=1 \
    -DWAMR_BUILD_LIBC_BUILTIN=1 \
    -DWAMR_BUILD_JIT=1 \
    -DWAMR_BUILD_AOT=1 \
    -DBUILD_SHARED_LIBS=1
   $ cmake --build . -j 2
   $ cmake --install . --prefix ${HOME}/.wamr
   ```

## Install wasi-sdk
Download https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-32/wasi-sdk-32.0-arm64-linux.tar.gz.  
Unpack it
```bash
$ sudo tar zxf asi-sdk-32.0-arm64-linux.tar.gz
```

## Build test codes and run
### benchmark wasm
```bash
$ cd wasm_runtime_evaluation/benchmark_wasm
$ mkdir build && cd build
$ cmake .. -DWASI_SDK_PATH=/Path/To/wasi-sdk-32.0-arm64-linux/
$ cmake --build .
```
You will find the file `benchmark.wasm`.

### wasmtime runner
```bash
$ cd wasm_runtime_evaluation/wasmtime_test
$ mkdir build && cd build
$ cmake .. -DWASMTIME_ROOT=${HOME}/.wasmtime/wasmtime-v43.0.1-aarch64-android-c-api
$ cmake --build .
# Run wasm file
$ ./wasmtime_runner ../../benchmark_wasm/build/benchmark.wasm
# Product AOT wasm file
$ export PATH=${HOME}/.wasmtime/bin:$PATH
$ wasmtime compile -W concurrency-support=n -o benchmark.cwasm ../../benchmark_wasm/build/benchmark.wasm
# Run AOT wasm file
$ ./wasmtime_aot_runner benchmark.cwasm
```

### wasmedge runner
```bash
$ cd wasm_runtime_evaluation/wasmedge_test
$ mkdir build && cd build
$ cmake .. -DWASMEDGE_ROOT=${HOME}/.wasmedge/
$ cmake --build .
# Run wasm file
$ ./wasmedge_runner ../../benchmark_wasm/build/benchmark.wasm
# Product AOT wasm file
$ source ${HOME}/.wasmedge/env
$ wasmedge compile ../../benchmark_wasm/build/benchmark.wasm benchmark.so
# Run AOT wasm file
$ ./wasmedge_aot_runner benchmark.so
```

### wamr runner
```bash
$ cd wasm_runtime_evaluation/wamr_test
$ mkdir build && cd build
$ cmake .. -DWAMR_ROOT=${HOME}/.wamr
$ cmake --build .
# Run wasm file
$ ./wamr_runner ../../benchmark_wasm/build/benchmark.wasm
# Product AOT wasm file
$ export PATH=${HOME}/.wamr/bin:${PATH}
$ wamrc -o benchmark.aot ../../benchmark_wasm/build/benchmark.wasm
```
