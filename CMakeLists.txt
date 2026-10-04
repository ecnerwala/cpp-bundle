cmake_minimum_required(VERSION 3.20)
project(cpp-bundler CXX)

find_package(Clang REQUIRED CONFIG)
message(STATUS "Using Clang ${LLVM_PACKAGE_VERSION} from ${Clang_DIR}")

separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})

add_executable(cpp-bundle src/cpp_bundle.cpp)
add_executable(cpp-minify src/cpp_minify.cpp)
foreach(t cpp-bundle cpp-minify)
	target_compile_features(${t} PRIVATE cxx_std_20)
	target_include_directories(${t} SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})
	target_compile_definitions(${t} PRIVATE ${LLVM_DEFINITIONS_LIST})
	if(NOT LLVM_ENABLE_RTTI)
		target_compile_options(${t} PRIVATE -fno-rtti)
	endif()
	target_link_libraries(${t} PRIVATE clang-cpp LLVM)
endforeach()

install(TARGETS cpp-bundle cpp-minify)

enable_testing()
add_test(NAME fixtures COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/run.sh $<TARGET_FILE:cpp-bundle> $<TARGET_FILE:cpp-minify>)
