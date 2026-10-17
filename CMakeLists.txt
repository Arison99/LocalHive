cmake_minimum_required(VERSION 3.16)
project(LocalHive VERSION 0.1.0 LANGUAGES C CXX)

# ==============================================================================
# Language Standard & Compiler Requirements
# ==============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Platform and compiler checks
message(STATUS "Configuring LocalHive on platform: ${CMAKE_SYSTEM_NAME} (${CMAKE_SYSTEM_PROCESSOR})")
message(STATUS "C++ Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")

# Warning flags
if(MSVC)
    add_compile_options(/W4 /permissive- /EHsc)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ==============================================================================
# Dependency Discovery
# ==============================================================================
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Platform libraries
if(WIN32)
    set(PLATFORM_NET_LIBS ws2_32)
else()
    set(PLATFORM_NET_LIBS "")
endif()

# Optional Boost discovery for network integration
find_package(Boost COMPONENTS system QUIET)
if(Boost_FOUND)
    message(STATUS "Boost found: ${Boost_VERSION}")
else()
    message(STATUS "Boost not detected in standard system paths; Core compiled with standalone transport primitives.")
endif()

# Optional SQLite discovery
find_package(SQLite3 QUIET)
if(SQLite3_FOUND)
    message(STATUS "System SQLite3 detected: ${SQLite3_VERSION}")
else()
    message(STATUS "System SQLite3 not detected in default paths.")
endif()

# ==============================================================================
# Target: localhive_core (Static Library)
# ==============================================================================
set(CORE_SOURCES
    src/core/platform/platform.cpp
    src/core/config/config.cpp
    src/core/logging/logging.cpp
    src/core/lifecycle/lifecycle.cpp
    src/core/metrics/metrics.cpp
    src/core/crypto/crypto.cpp
    src/core/net/net.cpp
)

set(CORE_HEADERS
    include/localhive/core/types.hpp
    include/localhive/core/platform/platform.hpp
    include/localhive/core/config/config.hpp
    include/localhive/core/logging/logging.hpp
    include/localhive/core/lifecycle/lifecycle.hpp
    include/localhive/core/metrics/metrics.hpp
    include/localhive/core/crypto/crypto.hpp
    include/localhive/core/net/net.hpp
)

add_library(localhive_core STATIC
    ${CORE_SOURCES}
    ${CORE_HEADERS}
)

target_include_directories(localhive_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(localhive_core PUBLIC
    Threads::Threads
    ${PLATFORM_NET_LIBS}
)

if(Boost_FOUND)
    target_compile_definitions(localhive_core PUBLIC LOCALHIVE_HAS_BOOST_ASIO=1)
    target_link_libraries(localhive_core PUBLIC Boost::system)
endif()

# ==============================================================================
# Unit Tests & CTest Integration
# ==============================================================================
option(BUILD_TESTING "Build unit tests" ON)

if(BUILD_TESTING)
    enable_testing()
    include(CTest)

    macro(add_core_test test_name source_file)
        add_executable(${test_name} ${source_file})
        target_link_libraries(${test_name} PRIVATE localhive_core)
        add_test(NAME ${test_name} COMMAND ${test_name})
    endmacro()

    add_core_test(test_core_platform  tests/core/test_platform.cpp)
    add_core_test(test_core_config    tests/core/test_config.cpp)
    add_core_test(test_core_logging   tests/core/test_logging.cpp)
    add_core_test(test_core_lifecycle tests/core/test_lifecycle.cpp)
    add_core_test(test_core_metrics   tests/core/test_metrics.cpp)
    add_core_test(test_core_crypto    tests/core/test_crypto.cpp)
    add_core_test(test_core_net       tests/core/test_net.cpp)
endif()

# ==============================================================================
# Legacy Prototype Subsystem
# ==============================================================================
option(LOCALHIVE_BUILD_LEGACY_S3 "Build legacy s3-Bucket prototype" OFF)
if(LOCALHIVE_BUILD_LEGACY_S3)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/s3-Bucket/CmakeLists.txt")
        add_subdirectory(s3-Bucket)
    endif()
endif()
