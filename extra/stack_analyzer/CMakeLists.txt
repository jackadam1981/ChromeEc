cmake_minimum_required(VERSION 3.7)
find_package(LLVM 5.0 REQUIRED CONFIG)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -std=c++11")

add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})

add_library(LLVMAnnotator MODULE llvm_annotator.cc)
