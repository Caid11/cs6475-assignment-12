cmake_minimum_required(VERSION 3.10)

project(cs6475-assignment-12)

set(CMAKE_CXX_STANDARD 17)

find_package(LLVM REQUIRED CONFIG)

include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

add_executable(test main.cpp)

llvm_map_components_to_libnames(LLVM_LIBS support core irreader)

target_link_libraries(test PRIVATE ${LLVM_LIBS})
