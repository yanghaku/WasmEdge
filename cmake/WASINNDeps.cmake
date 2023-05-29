# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2019-2022 Second State INC

# Add backends building flags.
foreach(BACKEND ${WASMEDGE_PLUGIN_WASI_NN_BACKEND})
  string(TOLOWER ${BACKEND} BACKEND)
  if(BACKEND STREQUAL "openvino")
    message(STATUS "WASI-NN: Build OpenVINO backend for WASI-NN")
    find_package(InferenceEngine REQUIRED)
    add_definitions(-DWASMEDGE_PLUGIN_WASI_NN_BACKEND_OPENVINO)
    list(APPEND WASMEDGE_PLUGIN_WASI_NN_DEPS
      ${InferenceEngine_LIBRARIES}
    )
  elseif(BACKEND STREQUAL "pytorch")
    message(STATUS "WASI-NN: Build PyTorch backend for WASI-NN")
    find_package(Torch REQUIRED)
    add_definitions(-DWASMEDGE_PLUGIN_WASI_NN_BACKEND_TORCH)
    list(APPEND WASMEDGE_PLUGIN_WASI_NN_DEPS
      ${TORCH_LIBRARIES}
    )
  elseif(BACKEND STREQUAL "tensorflowlite")
    message(STATUS "WASI-NN: Build Tensorflow lite backend for WASI-NN")
    # TODO: Move these complicated steps into a helper cmake.
    add_definitions(-DWASMEDGE_PLUGIN_WASI_NN_BACKEND_TFLITE)

    if(NOT WASMEDGE_DEPS_VERSION)
      set(WASMEDGE_DEPS_VERSION "TF-2.12.0-CC")
    endif()

    # Clone required shared libraries
    if(ANDROID)
      if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
        set(WASMEDGE_TENSORFLOW_SYSTEM_NAME "android_aarch64")
        set(WASMEDGE_TENSORFLOW_DEPS_TFLITE_HASH "2d7dcd7381479d9ffc0968ea66e24a5207b404c7f2ccbdddec6f2a4d6f9813f2")
      elseif()
        message(FATAL_ERROR "Unsupported architecture: ${CMAKE_SYSTEM_PROCESSOR}")
      endif()
    elseif(APPLE)
      if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "AMD64")
        set(WASMEDGE_TENSORFLOW_SYSTEM_NAME "darwin_x86_64")
        set(WASMEDGE_TENSORFLOW_DEPS_TFLITE_HASH "04b58f4b97220633a8e299a63aba73d9a1f228904081e7d5f18e78d1e38d5f00")
      elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64" or CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
        set(WASMEDGE_TENSORFLOW_SYSTEM_NAME "darwin_arm64")
        set(WASMEDGE_TENSORFLOW_DEPS_TFLITE_HASH "cb4562a80ac2067bdabe2464b80e129b9d8ddc6d97ad1a2d7215e06a1e1e8cda")
      else()
        message(FATAL_ERROR "Unsupported architecture: ${CMAKE_SYSTEM_PROCESSOR}")
      endif()
    elseif(UNIX)
      if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "AMD64")
        set(WASMEDGE_TENSORFLOW_SYSTEM_NAME "manylinux2014_x86_64")
        set(WASMEDGE_TENSORFLOW_DEPS_TFLITE_HASH "110a06bcda1fdc3e744b1728157b66981e235de130f3a34755684e6adcf08341")
      elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
        set(WASMEDGE_TENSORFLOW_SYSTEM_NAME "manylinux2014_aarch64")
        set(WASMEDGE_TENSORFLOW_DEPS_TFLITE_HASH "672b81d3f4b5a6c25dc9bbc3b8c6ac1c0357cfab8105b2a85b8bb8c0b59afcb4")
      else()
        message(FATAL_ERROR "Unsupported architecture: ${CMAKE_SYSTEM_PROCESSOR}")
      endif()
    else()
      message(FATAL_ERROR "Unsupported system: ${CMAKE_SYSTEM_NAME}")
    endif()

    include(FetchContent)

    # Fetch Tensorflow-lite library.
    FetchContent_Declare(
      wasmedgetensorflowdepslite
      URL "https://github.com/second-state/WasmEdge-tensorflow-deps/releases/download/${WASMEDGE_DEPS_VERSION}/WasmEdge-tensorflow-deps-TFLite-${WASMEDGE_DEPS_VERSION}-${WASMEDGE_TENSORFLOW_SYSTEM_NAME}.tar.gz"
      URL_HASH "SHA256=${WASMEDGE_TENSORFLOW_DEPS_TFLITE_HASH}"
    )
    FetchContent_GetProperties(wasmedgetensorflowdepslite)

    if(NOT wasmedgetensorflowdepslite_POPULATED)
      message(STATUS "Downloading dependency: libtensorflowlite")
      FetchContent_Populate(wasmedgetensorflowdepslite)
      message(STATUS "Downloading dependency: libtensorflowlite - done")
    endif()

    # Setup Tensorflow-lite library.
    if(APPLE)
      set(WASMEDGE_TENSORFLOW_DEPS_TFLITE_LIB
        "${wasmedgetensorflowdepslite_SOURCE_DIR}/libtensorflowlite_c.dylib"
      )
    elseif(UNIX)
      set(WASMEDGE_TENSORFLOW_DEPS_TFLITE_LIB
        "${wasmedgetensorflowdepslite_SOURCE_DIR}/libtensorflowlite_c.so"
      )
    endif()

    include(FetchContent)
    FetchContent_Declare(
      wasmedge_tensorflow_deps
      GIT_REPOSITORY https://github.com/second-state/WasmEdge-tensorflow-deps.git
      GIT_TAG ${WASMEDGE_DEPS_VERSION}
    )
    FetchContent_GetProperties(wasmedge_tensorflow_deps)

    if(NOT wasmedge_tensorflow_deps_POPULATED)
      message(STATUS "Fetching WasmEdge-tensorflow-deps repository")
      FetchContent_Populate(wasmedge_tensorflow_deps)
      message(STATUS "Fetching WasmEdge-tensorflow-deps repository - done")
      message(STATUS "WASI-NN: Set TensorFlow-Lite include path: ${wasmedge_tensorflow_deps_SOURCE_DIR}")
      message(STATUS "WASI-NN: Set TensorFlow-Lite shared library path: ${WASMEDGE_TENSORFLOW_DEPS_TFLITE_LIB}")
    endif()

    set(WASMEDGE_TENSORFLOW_DEPS_PATH ${wasmedge_tensorflow_deps_SOURCE_DIR})
    list(APPEND WASMEDGE_PLUGIN_WASI_NN_INCLUDES
      ${WASMEDGE_TENSORFLOW_DEPS_PATH}
    )
    list(APPEND WASMEDGE_PLUGIN_WASI_NN_DEPS
      ${WASMEDGE_TENSORFLOW_DEPS_TFLITE_LIB}
    )
  else()
    # Add the other backends here.
    message(FATAL_ERROR "WASI-NN: backend ${BACKEND} not found or unimplemented.")
  endif()
endforeach()

function(wasmedge_setup_wasinn_target target)
  target_include_directories(${target}
    PUBLIC
    ${WASMEDGE_PLUGIN_WASI_NN_INCLUDES}
  )
  target_link_libraries(${target}
    PUBLIC
    ${WASMEDGE_PLUGIN_WASI_NN_DEPS}
  )
endfunction()
