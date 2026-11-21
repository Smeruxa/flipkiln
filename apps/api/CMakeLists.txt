cmake_minimum_required(VERSION 3.24)
project(flipkiln-api VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Drogon 1.9.14 EXACT CONFIG REQUIRED)
find_package(PostgreSQL REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json 3.11.2 EXACT CONFIG REQUIRED)

add_library(flipkiln-domain
    src/domain/evaluation.cpp
    src/domain/validation.cpp
)
target_include_directories(flipkiln-domain PUBLIC src)
target_link_libraries(flipkiln-domain PUBLIC nlohmann_json::nlohmann_json)

add_executable(flipkiln-api
    src/control/routes.cpp
    src/control/sessions.cpp
    src/http.cpp
    src/main.cpp
    src/migrations.cpp
    src/postgres.cpp
    src/runtime/routes.cpp
    src/security.cpp
    src/service.cpp
    src/storage/access.cpp
    src/storage/releases.cpp
    src/storage/runtime.cpp
)
target_link_libraries(flipkiln-api PRIVATE
    flipkiln-domain
    Drogon::Drogon
    PostgreSQL::PostgreSQL
    OpenSSL::Crypto
)
foreach(target flipkiln-api flipkiln-domain)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
endforeach()

option(FLIPKILN_BUILD_TESTS "Build Flipkiln domain tests" ON)
if(FLIPKILN_BUILD_TESTS)
    include(FetchContent)
    include(CTest)
    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG 52eb8108c5bdec04579160ae17225d66034bd723
    )
    FetchContent_MakeAvailable(googletest)
    add_executable(flipkiln-domain-test tests/domain_test.cpp)
    target_link_libraries(flipkiln-domain-test PRIVATE flipkiln-domain GTest::gtest_main)
    include(GoogleTest)
    gtest_discover_tests(flipkiln-domain-test)
endif()
