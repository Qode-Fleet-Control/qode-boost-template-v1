cmake_minimum_required(VERSION 3.16)

project(app VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Boost.Beast and Boost.Asio are header-only; Boost.System is header-only
# since 1.69, so only the headers are needed.
find_package(Boost 1.74 REQUIRED)
find_package(Threads REQUIRED)

add_executable(app src/main.cpp)
target_link_libraries(app PRIVATE Boost::boost Threads::Threads)

install(TARGETS app RUNTIME DESTINATION bin)
