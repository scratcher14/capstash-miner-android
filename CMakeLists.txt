# capstash-miner — Termux Android build
cmake_minimum_required(VERSION 3.16)
project(capstash_miner VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ── Android/Termux detection ──────────────────────────────────────────────
if(ANDROID_BUILD OR DEFINED ENV{TERMUX_VERSION})
    message(STATUS "[capstash-miner] Termux/Android build detected")
    # Native compile — march=native gets the exact CPU features
    set(ARCH_FLAGS "-O3 -march=native -mtune=native -fomit-frame-pointer")
    # Termux pthread is in libc
    set(TERMUX_BUILD ON)
else()
    # Desktop fallback
    include(CheckCCompilerFlag)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
        set(ARCH_FLAGS "-O3 -march=armv8-a+crypto -mtune=native")
    else()
        check_c_compiler_flag("-mavx2" HAS_AVX2)
        if(HAS_AVX2)
            set(ARCH_FLAGS "-O3 -march=native -mavx2")
        else()
            set(ARCH_FLAGS "-O3 -march=native")
        endif()
    endif()
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ARCH_FLAGS}")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# ── Sources ───────────────────────────────────────────────────────────────
add_executable(capstash-miner
    main.c
    whirlpool.c
    sha256.c
    rpc.c
    miner.c
)

target_include_directories(capstash-miner PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# ── Link libraries ────────────────────────────────────────────────────────
if(TERMUX_BUILD)
    # Termux: pthread is in libc, no extra link needed
    target_link_libraries(capstash-miner m)
else()
    find_package(Threads REQUIRED)
    target_link_libraries(capstash-miner Threads::Threads m)
endif()

install(TARGETS capstash-miner DESTINATION ${CMAKE_INSTALL_PREFIX})

message(STATUS "[capstash-miner] Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "[capstash-miner] C flags:    ${CMAKE_C_FLAGS}")
