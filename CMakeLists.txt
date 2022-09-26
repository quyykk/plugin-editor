cmake_minimum_required(VERSION 3.13)

include(CMakeDependentOption)
cmake_dependent_option(ES_GLES "Build the game with OpenGL ES" OFF UNIX OFF)

# Support Debug and Release configurations.
set(CMAKE_CONFIGURATION_TYPES "Debug" "Release" CACHE STRING "" FORCE)

# Use C++17 without any compiler specific extensions.
set(CMAKE_CXX_STANDARD 17 CACHE STRING "")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# Use LTO for Release builds only.
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_DEBUG FALSE)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
# On Linux use relative RPATH.
set(CMAKE_BUILD_RPATH_USE_ORIGIN ON)

# Add our overlays  for vcpkg and setup vcpkg.
set(VCPKG_OVERLAY_TRIPLETS "${CMAKE_CURRENT_SOURCE_DIR}/overlays")
set(VCPKG_OVERLAY_PORTS "${CMAKE_CURRENT_SOURCE_DIR}/overlays")
set(VCPKG_BOOTSTRAP_OPTIONS "-disableMetrics")
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(X_VCPKG_APPLOCAL_DEPS_INSTALL ON)
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake"
	CACHE STRING "Vcpkg toolchain file" FORCE)

project("Editor" VERSION 0.9.15
	DESCRIPTION "Unofficial plugin editor for Endless Sky"
	HOMEPAGE_URL https://github.com/quyykk/plugin-editor
	LANGUAGES CXX)

string(TIMESTAMP EDITOR_CURRENT_DATE "%d %b %Y")
configure_file("./source/Version.h.in" "${CMAKE_CURRENT_SOURCE_DIR}/source/Version.h" @ONLY)

# Find the needed library dependencies.
find_package(SDL2 CONFIG REQUIRED)
find_package(PNG REQUIRED)
find_package(JPEG REQUIRED)
if(NOT APPLE)
	find_package(GLEW REQUIRED)
endif()

# Only very new versions of OpenAL provide a config package, so try to find it manually.
find_package(OpenAL CONFIG QUIET)
if(NOT OpenAL_FOUND)
	find_library(OPENAL openal REQUIRED)
	add_library(OpenAL UNKNOWN IMPORTED)
	add_library(OpenAL::OpenAL ALIAS OpenAL)
	set_target_properties(OpenAL PROPERTIES IMPORTED_LOCATION ${OPENAL})
endif()

# libmad doesn't provide native cmake support.
find_path(LIBMAD_INCLUDE_DIR mad.h)
find_library(LIBMAD_LIB_DEBUG NAMES mad libmad NAMES_PER_DIR PATHS "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/lib")
find_library(LIBMAD_LIB_RELEASE NAMES mad libmad NAMES_PER_DIR PATHS "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib")

# Libraries that aren't distributed in distros.
find_package(imgui CONFIG REQUIRED)
# nativefiledialog-extended doesn't provide native cmake support.
find_path(NFDE_INCLUDE_DIR nfd.h)
find_library(NFDE_LIB_DEBUG NAMES nfd NAMES_PER_DIR PATHS "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/lib" NO_DEFAULT_PATH)
find_library(NFDE_LIB_RELEASE NAMES nfd NAMES_PER_DIR PATHS "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib" NO_DEFAULT_PATH)

# Create game target.
if(APPLE)
	add_executable(Editor MACOSX_BUNDLE source/main.cpp)

	# MacOS bundles are a bit special and need every resource specified when
	# creating the executable.
	foreach(folder "data" "images" "sounds")
		file(GLOB_RECURSE RESOURCES "${CMAKE_CURRENT_SOURCE_DIR}/endless-sky/${folder}/*")
		target_sources(Editor PRIVATE ${RESOURCES})

		foreach(FILE ${RESOURCES})
		    # Get the relative path from the root folder to the current file.
		    file(RELATIVE_PATH NEW_FILE "${CMAKE_CURRENT_SOURCE_DIR}/endless-sky" ${FILE})
			# Get the parent directory for the new location.
			get_filename_component(FILE_PATH ${NEW_FILE} DIRECTORY)

			# Resources belong under Resources/.
			set_source_files_properties(${FILE} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/${FILE_PATH}")
		endforeach()
	endforeach()

	# Create the icns file, required for the bundle icon.
	add_custom_command(
		OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/resources/editor.icns
		COMMAND iconutil -c icns -o resources/editor.icns resources/editor.iconset
		DEPENDS resources/editor.iconset/icon_16x16.png resources/editor.iconset/icon_16x16@2x.png
			resources/editor.iconset/icon_32x32.png resources/editor.iconset/icon_32x32@2x.png
			resources/editor.iconset/icon_128x128.png resources/editor.iconset/icon_128x128@2x.png
			resources/editor.iconset/icon_256x256.png resources/editor.iconset/icon_256x256@2x.png
			resources/editor.iconset/icon_512x512.png resources/editor.iconset/icon_512x512@2x.png
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

	# Now do the same to standone files.
	foreach(file "endless-sky/license.txt" "endless-sky/keys.txt" "endless-sky/credits.txt" "endless-sky/copyright" "endless-sky/changelog" "resources/editor.icns")
		target_sources(Editor PRIVATE ${file})
		set_source_files_properties(${file} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/")
	endforeach()

	# Add plist to bundle.
	set_target_properties(Editor PROPERTIES
		MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_LIST_DIR}/resources/Editor-Info.plist.in
		XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME editor)
elseif(WIN32)
	add_executable(Editor WIN32 source/main.cpp)
	target_sources(Editor PRIVATE source/WinApp.rc)
else()
	add_executable(Editor source/main.cpp)
endif()

# Add the source files to the target.
add_subdirectory(source)
target_include_directories(Editor PRIVATE endless-sky/source)

# The 'mingw32' lib needs to appear first.
if(MINGW)
	target_link_libraries(Editor PRIVATE mingw32)

	# On Windows copy the MinGW runtime DLLs to the output folder as well.
	# This is to avoid the situation where a user has other MinGW runtime DLLs
	# in their PATH that are incompatible with the MinGW used to compile ES.
	if(WIN32)
		get_filename_component(PARENT_DIR ${CMAKE_CXX_COMPILER} DIRECTORY)
		get_filename_component(MINGW_RUNTIME_DIR ${PARENT_DIR} DIRECTORY)

		# MinGW doesn't have seh exceptions support for 32-bit Windows unfortunately,
		# and requires libgcc_s_dw2-1.dll instead of libgcc_s_seh-1.dll. There's no
		# perfect way to figure out which one to copy, so we simply copy both.
		# The executable will choose the correct DLL anyways.
		foreach(lib "stdc++-6" "winpthread-1" "gcc_s_seh-1" "gcc_s_dw2-1")
			file(GLOB_RECURSE FILE_PATH "${MINGW_RUNTIME_DIR}/lib${lib}.dll")
			if(FILE_PATH)
				add_custom_command(TARGET Editor POST_BUILD
					COMMAND ${CMAKE_COMMAND} -E copy_if_different ${FILE_PATH} $<TARGET_FILE_DIR:Editor>
					COMMAND_EXPAND_LISTS)

				# Add an install rule for this DLLs, so that it is also included when installing.
				install(FILES ${FILE_PATH} DESTINATION .)
			endif()
		endforeach()
	endif()
endif()

# Link with the library dependencies.
target_link_libraries(Editor PRIVATE SDL2::SDL2main)

target_include_directories(Editor PUBLIC ${LIBMAD_INCLUDE_DIR} ${NFDE_INCLUDE_DIR})
target_link_libraries(Editor PRIVATE SDL2::SDL2 PNG::PNG JPEG::JPEG OpenAL::OpenAL imgui::imgui
	$<IF:$<CONFIG:Debug>,${LIBMAD_LIB_DEBUG},${LIBMAD_LIB_RELEASE}>
	$<IF:$<CONFIG:Debug>,${NFDE_LIB_DEBUG},${NFDE_LIB_RELEASE}>)

target_include_directories(Editor PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/endless-sky/source")

# Link the needed OS-specific dependencies, if any.
if(WIN32)
	target_link_libraries(Editor PUBLIC rpcrt4 Winmm)
elseif(UNIX AND NOT APPLE)
	target_link_libraries(Editor PUBLIC pthread dbus-1 ${CMAKE_DL_LIBS})
endif()

# Link with the UUID library, which is different for each OS.
if(UNIX)
	if(APPLE)
		find_library(UUID_LIB NAMES System PATHS /lib /usr/lib /usr/local/lib)
		find_path(UUID_INCLUDE uuid/uuid.h /usr/local/include /usr/include)
		target_link_libraries(Editor PUBLIC ${UUID_LIB})
		target_include_directories(Editor PUBLIC ${UUID_INCLUDE})
	else()
		find_package(unofficial-libuuid CONFIG REQUIRED)
		target_link_libraries(Editor PUBLIC unofficial::UUID::uuid)
	endif()
endif()

# Use OpenGL or OpenGL ES.
if(ES_GLES)
	find_package(OpenGL REQUIRED OpenGL EGL)
	target_link_libraries(Editor PRIVATE OpenGL::OpenGL OpenGL::EGL)
	target_compile_definitions(Editor PUBLIC ES_GLES)
else()
	find_package(OpenGL REQUIRED)
	target_link_libraries(Editor PRIVATE OpenGL::GL)

	if(APPLE)
		# Apple deprecated OpenGL in MacOS 10.14, but we don't care.
		target_compile_definitions(Editor PUBLIC GL_SILENCE_DEPRECATION)
	else()
		# GLEW is only needed on Linux and Windows.
		target_link_libraries(Editor PRIVATE GLEW::GLEW)
	endif()
endif()

# Set the appropriate compiler flags.
if(MSVC)
	target_compile_options(Editor PUBLIC "/W3" "/permissive-" "/Gm-" "/sdl" "/MP" "/analyze-"
		"-Wno-nonportable-include-path" "$<$<CONFIG:Release>:/Gy;/WX;/Oi>")
	target_compile_definitions(Editor PUBLIC "_UNICODE" "UNICODE" "$<IF:$<CONFIG:Debug>,_DEBUG,NDEBUG>")
else()
	target_compile_options(Editor PUBLIC "-Wall" "$<$<CONFIG:Release>:-Werror>")
endif()

# Various helpful options for IDEs.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Editor)
set_property(TARGET Editor PROPERTY VS_JUST_MY_CODE_DEBUGGING ON)

# Installation configurations.
if(APPLE)
	install(TARGETS Editor CONFIGURATIONS Release BUNDLE DESTINATION .)
elseif(WIN32)
	# Install the binary.
	install(TARGETS Editor CONFIGURATIONS Release RUNTIME DESTINATION .)

	# The MinGW DLLs needed were already installed above, and vcpkg installs the library DLLs.

	# Install the resouce files.
	install(DIRECTORY endless-sky/data DESTINATION .)
	install(DIRECTORY endless-sky/images DESTINATION .)
	install(DIRECTORY endless-sky/sounds DESTINATION .)
	install(FILES endless-sky/credits.txt DESTINATION .)
	install(FILES endless-sky/keys.txt DESTINATION .)
	install(FILES endless-sky/copyright DESTINATION .)
	install(FILES endless-sky/changelog DESTINATION .)
elseif(UNIX)
	# Use lowercase target name on Linux.
	set_target_properties(Editor PROPERTIES OUTPUT_NAME "editor")

	# Install the binary.
	install(TARGETS Editor CONFIGURATIONS Release RUNTIME DESTINATION games)

	# Install the desktop file.
	install(FILES resources/editor.desktop DESTINATION share/applications)

	# Install app center metadata.
	install(FILES resources/io.github.quyykk.editor.appdata.xml DESTINATION share/metainfo)

	# Install icons, keeping track of all the paths.
	# Most Ubuntu apps supply 16, 22, 24, 32, 48, and 256, and sometimes others.
	foreach(size "16x16" "22x22" "24x24" "32x32" "48x48" "128x128" "256x256" "512x512")
		install(FILES "resources/icons/icon_${size}.png" DESTINATION "share/icons/hicolor/${size}/apps"
			RENAME editor.png)
	endforeach()

	# Install the resouce files.
	install(DIRECTORY endless-sky/data DESTINATION share/games/editor)
	install(DIRECTORY endless-sky/images DESTINATION share/games/editor)
	install(DIRECTORY endless-sky/sounds DESTINATION share/games/editor)
	install(FILES endless-sky/credits.txt DESTINATION share/games/editor)
	install(FILES endless-sky/keys.txt DESTINATION share/games/editor)
	install(FILES endless-sky/copyright DESTINATION share/doc/editor)
	install(FILES endless-sky/changelog DESTINATION share/doc/editor)
endif()
