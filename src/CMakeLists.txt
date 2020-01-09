cmake_minimum_required(VERSION 3.15)

project(OS_lab6)

add_library(functions STATIC
  functions.cpp
  )

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
#set(CMAKE_CXX_FLAGS "-g")

add_executable(server
  server.cpp
  )

add_executable(worker
  worker.cpp
  )

target_link_libraries(server functions zmq pthread)
target_link_libraries(worker functions zmq pthread)  
