# wasm_runtime_evaluation
Sample code for evaluating WASM runtimes.

My test environment is a Raspi 4 board with Ubuntu 22.04.  
The reason for using Ubuntu 22.04 is that the WAMR manual specifies using Ubuntu 22.04.

## Install WASM runtime environment

1. **wasmtime**

   ```bash
   $ curl https://wasmtime.dev/install.sh -sSf | bash
   ```
   C API sdk is https://github.com/bytecodealliance/wasmtime/releases/download/v43.0.1/wasmtime-v43.0.1-aarch64-linux-c-api.tar.xz.  
   Download it and copy min/include and min/lib to ${HOME}/.wasmtime

2. **wasmedge**

    ```bash
    $ curl -sSf https://raw.githubusercontent.com/WasmEdge/WasmEdge/master/utils/install.sh | bash
    ```
    Move installed directory to ${HOME}/.wasmedge

3. **wamr**

   The official website does not provide an aarch64 SDK, so it is necessary to compile from the source code.  
   ```bash
   # 1. Download source codes
   $ git clone https://github.com/bytecodealliance/wasm-micro-runtime.git

   # 2. Checkout latest version
   $ cd wasm-micro-runtime
   $ git checkout -b WAMR-2.4.4 WAMR-2.4.4

   # 3. Install dependent packages
   $ sudo apt install build-essential cmake ccache python3-pip

   # 4. build C API sdk and iwasm
   $ cd product-mini/platforms/linux
   $ mkdir build && cd build
   $ cmake .. \
    -DWAMR_BUILD_PLATFORM=linux \
    -DWAMR_BUILD_TARGET=aarch64 \
    -DWAMR_BUILD_INTERP=1 \
    -DWAMR_BUILD_FAST_INTERP=1 \
    -DWAMR_BUILD_LIBC_WASI=1 \
    -DWAMR_BUILD_LIBC_BUILTIN=1 \
    -DWAMR_BUILD_JIT=1 \
    -DWAMR_BUILD_WAMR_COMPILER=1 \
    -DWAMR_BUILD_AOT=1 \
    -DBUILD_SHARED_LIBS=1 \
    -DCMAKE_BUILD_TYPE=Release
   $ cmake --build . -j 2
   $ cmake --install . --prefix ${HOME}/.wamr

   # 5. build wamrc
   $ cd wamr-compiler
   $ ./build_llvm.sh
   $ mkdir build && cd build
   $ cmake ..
   $ cmake --build . -j 2
   ```

## Install wasi-sdk
Download https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-32/wasi-sdk-32.0-arm64-linux.tar.gz.

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
$ ./wasmtime_runner ../../benchmark_wasm/build/benchmark.wasm
```

### wasmedge runner
```bash
$ cd wasm_runtime_evaluation/wasmedge_test
$ mkdir build && cd build
$ cmake .. -DWASMEDGE_ROOT=${HOME}/.wasmedge/
$ cmake --build .
$ ./wasmedge_runner ../../benchmark_wasm/build/benchmark.wasm
```

### wamr runner
```bash
$ cd wasm_runtime_evaluation/wamr_test
$ mkdir build && cd build
$ cmake .. -DWAMR_ROOT=${HOME}/.wamr
$ cmake --build .
$ ./wamr_runner ../../benchmark_wasm/build/benchmark.wasm
```
