cmake_minimum_required(VERSION 3.19)

project(noexcept_personality LANGUAGES CXX)

set(GCC_PLUGIN_DIR "" CACHE FILEPATH "The location of GCC's plugin files")
if(GCC_PLUGIN_DIR STREQUAL "")
	message(CHECK_START "Looking for GCC's plugin directory")
	execute_process(
		COMMAND ${CMAKE_CXX_COMPILER} "-print-file-name=plugin"
		OUTPUT_VARIABLE GCC_PLUGIN_DIR_TMP
		COMMAND_ERROR_IS_FATAL ANY
	)
	string(STRIP "${GCC_PLUGIN_DIR_TMP}" GCC_PLUGIN_DIR_TMP)
	if (GCC_PLUGIN_DIR_TMP STREQUAL "plugin")
		message(CHECK_FAIL "Not found")
		message(FATAL_ERROR "Could not automatically locate GCC's plugin directory.  Specify it with -DGCC_PLUGIN_DIR=... and reconfigure the project.")
	endif()
	set(GCC_PLUGIN_DIR "${GCC_PLUGIN_DIR_TMP}" CACHE FILEPATH "The location of GCC's plugin files" FORCE)
	message(CHECK_PASS "Found at ${GCC_PLUGIN_DIR}")
endif()

add_library(noexcept_personality_plugin SHARED noexcept_personality_plugin.cpp)
target_include_directories(noexcept_personality_plugin PRIVATE ${GCC_PLUGIN_DIR}/include)
target_compile_options(noexcept_personality_plugin PRIVATE "-fno-rtti")
set_target_properties(noexcept_personality_plugin PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(WIN32)
	target_link_libraries(noexcept_personality_plugin PRIVATE "${GCC_PLUGIN_DIR}/cc1plus.exe.a")
endif()

add_library(noexcept_personality_func OBJECT noexcept_personality_func.cpp)

add_library(use_noexcept_personality INTERFACE)
add_dependencies(use_noexcept_personality noexcept_personality_plugin)
target_compile_options(use_noexcept_personality INTERFACE $<$<COMPILE_LANGUAGE:CXX>:-fplugin=$<TARGET_FILE:noexcept_personality_plugin>>)
target_sources(use_noexcept_personality INTERFACE $<TARGET_OBJECTS:noexcept_personality_func>)

option(BUILD_SAMPLES "Add targets for building a program demonstrating the plugin" ON)
if(BUILD_SAMPLES)
	add_executable(sample-without-plugin sample1.cpp sample2.cpp)

	add_executable(sample-with-plugin sample1.cpp sample2.cpp)
	target_link_libraries(sample-with-plugin use_noexcept_personality)

	add_custom_target(run-sample-without-plugin
		COMMENT "Running sample-without-plugin"
		COMMAND $<TARGET_FILE:sample-without-plugin>
		VERBATIM
	)

	add_custom_target(run-sample-with-plugin
		COMMENT "Running sample-with-plugin"
		COMMAND $<TARGET_FILE:sample-with-plugin>
		VERBATIM
	)

	add_custom_target(run-samples
		DEPENDS run-sample-without-plugin run-sample-with-plugin
	)
endif()
