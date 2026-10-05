cmake_minimum_required(VERSION 3.20)
project(cpp-bundler CXX)

find_package(Clang REQUIRED CONFIG)
message(STATUS "Using Clang ${LLVM_PACKAGE_VERSION} from ${Clang_DIR}")

separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})

# Link the clang/LLVM static archives (not libclang-cpp.so / libLLVM.so) so the
# binaries only depend on glibc at runtime. The imported CMake targets would pull
# in the shared libraries, so the archives are listed explicitly.
function(static_libs out)
	set(files)
	foreach(lib ${ARGN})
		list(APPEND files ${LLVM_LIBRARY_DIR}/lib${lib}.a)
	endforeach()
	set(${out} ${files} PARENT_SCOPE)
endfunction()
function(llvm_static_libs out)
	execute_process(
		COMMAND ${LLVM_TOOLS_BINARY_DIR}/llvm-config --link-static --libfiles ${ARGN}
		OUTPUT_VARIABLE files OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
	separate_arguments(files NATIVE_COMMAND ${files})
	set(${out} ${files} PARENT_SCOPE)
endfunction()
include(FetchContent)
set(ZLIB_BUILD_SHARED OFF)
set(ZLIB_BUILD_TESTING OFF)
set(ZLIB_INSTALL OFF)
FetchContent_Declare(zlib
	URL https://github.com/madler/zlib/releases/download/v1.3.2/zlib-1.3.2.tar.gz
	URL_HASH SHA256=bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16)
set(ZSTD_BUILD_SHARED OFF)
set(ZSTD_BUILD_PROGRAMS OFF)
set(ZSTD_BUILD_TESTS OFF)
set(ZSTD_LEGACY_SUPPORT OFF)
FetchContent_Declare(zstd
	URL https://github.com/facebook/zstd/releases/download/v1.5.7/zstd-1.5.7.tar.gz
	URL_HASH SHA256=eb33e51f49a15e023950cd7825ca74a4a2b43db8354825ac24fc1b7ee09e6fa3
	SOURCE_SUBDIR build/cmake)
FetchContent_MakeAvailable(zlib zstd)
set(system_libs zlibstatic libzstd_static dl m)
if(APPLE)
	set(group_start)
	set(group_end)
else()
	list(APPEND system_libs rt)
	set(group_start -Wl,--start-group)
	set(group_end -Wl,--end-group)
endif()

add_executable(cpp-bundle src/cpp_bundle.cpp)
static_libs(bundle_clang_libs
	clangTooling clangFrontend clangDriver clangParse clangSerialization clangSema
	clangEdit clangAnalysis clangASTMatchers clangAST clangToolingCore
	clangToolingInclusions clangRewrite clangFormat clangLex clangBasic clangSupport
	clangAPINotes)
llvm_static_libs(bundle_llvm_libs
	support option targetparser frontendopenmp frontendhlsl bitreader bitstreamreader
	profiledata windowsdriver demangle binaryformat core)
target_link_libraries(cpp-bundle PRIVATE
	${group_start} ${bundle_clang_libs} ${bundle_llvm_libs} ${group_end})

add_executable(cpp-minify src/cpp_minify.cpp)
static_libs(minify_clang_libs clangLex clangBasic)
llvm_static_libs(minify_llvm_libs support targetparser frontendopenmp demangle)
target_link_libraries(cpp-minify PRIVATE
	${group_start} ${minify_clang_libs} ${minify_llvm_libs} ${group_end})

foreach(t cpp-bundle cpp-minify)
	target_compile_features(${t} PRIVATE cxx_std_20)
	target_include_directories(${t} SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})
	target_compile_definitions(${t} PRIVATE ${LLVM_DEFINITIONS_LIST})
	if(NOT LLVM_ENABLE_RTTI)
		target_compile_options(${t} PRIVATE -fno-rtti)
	endif()
	target_link_libraries(${t} PRIVATE ${system_libs})
	if(NOT APPLE)
		target_link_options(${t} PRIVATE -static-libstdc++ -static-libgcc)
	endif()
endforeach()

install(TARGETS cpp-bundle cpp-minify)

enable_testing()
add_test(NAME fixtures COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/run.sh $<TARGET_FILE:cpp-bundle> $<TARGET_FILE:cpp-minify>)
