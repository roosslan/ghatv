cmake_minimum_required(VERSION 3.16)
project(ghatv VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(GHATV_TESTS "Собирать модульные тесты" OFF)
# Версия для «О программе»; в CI при сборке релиза передаётся имя тега
set(GHATV_VERSION "${PROJECT_VERSION}" CACHE STRING "Версия ghatv")

include(FetchContent)

# Turbo Vision (magiblot), зафиксированный коммит
set(TV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(TV_BUILD_USING_GPM OFF CACHE BOOL "" FORCE)
FetchContent_Declare(tvision
    GIT_REPOSITORY https://github.com/magiblot/tvision.git
    GIT_TAG b5fc1e50dcc2da062d72f43eb3662894d25aaf30
)
FetchContent_MakeAvailable(tvision)

find_package(CURL REQUIRED)
find_package(nlohmann_json 3 REQUIRED)
find_package(Threads REQUIRED)

# Всё, кроме точки входа: используется программой и тестами
add_library(ghatv_core STATIC
    src/github.cpp
    src/poller.cpp
    src/util.cpp
    src/ui.cpp
    src/runs_window.cpp
    src/pulls_window.cpp
    src/pull_window.cpp
    src/issues_window.cpp
    src/table.cpp
    src/run_window.cpp
    src/log_window.cpp
)
target_include_directories(ghatv_core PUBLIC src)
target_link_libraries(ghatv_core PUBLIC tvision CURL::libcurl nlohmann_json::nlohmann_json Threads::Threads)

add_executable(ghatv src/main.cpp)
target_compile_definitions(ghatv PRIVATE GHATV_VERSION="${GHATV_VERSION}")
target_link_libraries(ghatv PRIVATE ghatv_core)
install(TARGETS ghatv RUNTIME DESTINATION bin)

if(GHATV_TESTS)
    enable_testing()
    add_executable(ghatv_tests
        tests/main.cpp
        tests/test_util.cpp
        tests/test_parse.cpp
        tests/test_log.cpp
        tests/test_ci.cpp
    )
    target_link_libraries(ghatv_tests PRIVATE ghatv_core)
    add_test(NAME ghatv_tests COMMAND ghatv_tests)
endif()
