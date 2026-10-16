cmake_minimum_required(VERSION 3.16)
project(ghatv CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

add_executable(ghatv
    src/main.cpp
    src/github.cpp
    src/poller.cpp
    src/util.cpp
    src/ui.cpp
    src/runs_window.cpp
    src/run_window.cpp
    src/log_window.cpp
)
target_link_libraries(ghatv PRIVATE tvision CURL::libcurl nlohmann_json::nlohmann_json Threads::Threads)
install(TARGETS ghatv RUNTIME DESTINATION bin)
