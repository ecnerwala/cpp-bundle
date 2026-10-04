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
set(system_libs rt dl m)
foreach(lib z zstd)
	find_library(${lib}_static lib${lib}.a)
	if(${lib}_static)
		list(APPEND system_libs ${${lib}_static})
	else()
		list(APPEND system_libs ${lib})
	endif()
endforeach()

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
	-Wl,--start-group ${bundle_clang_libs} ${bundle_llvm_libs} -Wl,--end-group)

add_executable(cpp-minify src/cpp_minify.cpp)
static_libs(minify_clang_libs clangLex clangBasic)
llvm_static_libs(minify_llvm_libs support targetparser frontendopenmp demangle)
target_link_libraries(cpp-minify PRIVATE
	-Wl,--start-group ${minify_clang_libs} ${minify_llvm_libs} -Wl,--end-group)

foreach(t cpp-bundle cpp-minify)
	target_compile_features(${t} PRIVATE cxx_std_20)
	target_include_directories(${t} SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})
	target_compile_definitions(${t} PRIVATE ${LLVM_DEFINITIONS_LIST})
	if(NOT LLVM_ENABLE_RTTI)
		target_compile_options(${t} PRIVATE -fno-rtti)
	endif()
	target_link_libraries(${t} PRIVATE ${system_libs})
	target_link_options(${t} PRIVATE -static-libstdc++ -static-libgcc)
endforeach()

install(TARGETS cpp-bundle cpp-minify)

enable_testing()
add_test(NAME fixtures COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/run.sh $<TARGET_FILE:cpp-bundle> $<TARGET_FILE:cpp-minify>)
