cmake_minimum_required(VERSION 3.20)
project(cpp-bundler CXX)

find_package(Clang REQUIRED CONFIG)
message(STATUS "Using Clang ${LLVM_PACKAGE_VERSION} from ${Clang_DIR}")

add_executable(cpp-bundle src/cpp_bundle.cpp)
target_compile_features(cpp-bundle PRIVATE cxx_std_20)
target_include_directories(cpp-bundle SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
target_compile_definitions(cpp-bundle PRIVATE ${LLVM_DEFINITIONS_LIST}
	CPP_BUNDLE_RESOURCE_DIR="${LLVM_LIBRARY_DIR}/clang/${LLVM_VERSION_MAJOR}")
if(NOT LLVM_ENABLE_RTTI)
	target_compile_options(cpp-bundle PRIVATE -fno-rtti)
endif()
target_link_libraries(cpp-bundle PRIVATE clang-cpp LLVM)

install(TARGETS cpp-bundle)

enable_testing()
add_test(NAME fixtures COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/run.sh $<TARGET_FILE:cpp-bundle>)
