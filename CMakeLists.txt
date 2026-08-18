# --- Detect if the plugin is build out of tree or not ---
if(CMAKE_PROJECT_NAME STREQUAL "obs-studio")
  set(BUILD_OUT_OF_TREE OFF)
else()
  set(BUILD_OUT_OF_TREE ON)
  cmake_minimum_required(VERSION 3.18)
endif()

project(obs-shaderfilter VERSION 2.6.0)
set(PROJECT_FULL_NAME "OBS Shaderfilter")

# Set new UUIDs when you start to create a new plugin.
set(MACOS_PACKAGE_UUID "7DA301DD-35F4-44C3-BE34-B33528D487F5")
set(MACOS_INSTALLER_UUID "D1247630-89C0-498A-8A3B-B0752080DEC2")

add_library(${PROJECT_NAME} MODULE)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version.h.in ${CMAKE_CURRENT_SOURCE_DIR}/version.h)

target_sources(${PROJECT_NAME} PRIVATE
	obs-shaderfilter.h
	plugin-main.c
	shader-loader.c
	shader-convert.c
	shader-params.c
	shader-filter.c
	shader-transition.c
	version.h)
	
if(POLICY CMP0177)
  cmake_policy(SET CMP0177 NEW)
endif()

if(BUILD_OUT_OF_TREE)
  find_package(libobs QUIET)
  if(NOT TARGET OBS::libobs)
    find_library(LIBOBS_LIB NAMES obs libobs PATHS ${CMAKE_PREFIX_PATH}/lib ${CMAKE_PREFIX_PATH}/bin NO_DEFAULT_PATH)
    if(NOT LIBOBS_LIB)
      find_library(LIBOBS_LIB NAMES obs libobs)
    endif()
    find_path(LIBOBS_INCLUDE_DIR NAMES obs.h PATHS ${CMAKE_PREFIX_PATH}/include ${CMAKE_PREFIX_PATH}/include/obs NO_DEFAULT_PATH)
    if(NOT LIBOBS_INCLUDE_DIR)
      find_path(LIBOBS_INCLUDE_DIR NAMES obs.h)
    endif()

    if(LIBOBS_LIB AND LIBOBS_INCLUDE_DIR)
      add_library(OBS::libobs UNKNOWN IMPORTED GLOBAL)
      set_target_properties(OBS::libobs PROPERTIES
        IMPORTED_LOCATION "${LIBOBS_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${LIBOBS_INCLUDE_DIR}"
      )
    else()
      find_package(PkgConfig QUIET)
      if(PkgConfig_FOUND)
        pkg_check_modules(LIBOBS IMPORTED_TARGET libobs)
        if(TARGET PkgConfig::LIBOBS)
          add_library(OBS::libobs ALIAS PkgConfig::LIBOBS)
        endif()
      endif()
    endif()
  endif()
  include(cmake/ObsPluginHelpers.cmake)
endif()

if(OS_WINDOWS OR WIN32)
	get_filename_component(ISS_FILES_DIR "${CMAKE_BINARY_DIR}\\..\\package" ABSOLUTE)
	file(TO_NATIVE_PATH "${ISS_FILES_DIR}" ISS_FILES_DIR)
	get_filename_component(ISS_PACKAGE_DIR "${CMAKE_PACKAGE_PREFIX}\\.." ABSOLUTE)
	file(TO_NATIVE_PATH "${ISS_PACKAGE_DIR}" ISS_PACKAGE_DIR)
	get_filename_component(ISS_SOURCE_DIR "${PROJECT_SOURCE_DIR}" ABSOLUTE)
	file(TO_NATIVE_PATH "${ISS_SOURCE_DIR}" ISS_SOURCE_DIR)
	configure_file("installer.iss.in"
			"${PROJECT_BINARY_DIR}/installer.iss"
		)
	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/resource.rc.in ${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}.rc)
	target_sources(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}.rc)
	
elseif(OS_MACOS OR APPLE)
	set(MACOS_BUNDLEID "com.exeldro.${PROJECT_NAME}")
	set(MACOSX_PLUGIN_GUI_IDENTIFIER "${MACOS_BUNDLEID}")
	set(MACOSX_PLUGIN_BUNDLE_VERSION "${PROJECT_VERSION}")
	set(MACOSX_PLUGIN_SHORT_VERSION_STRING "1")
	configure_file(cmake/bundle/macos/installer-macos.pkgproj.in ${CMAKE_BINARY_DIR}/installer-macos.generated.pkgproj)
	target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wno-error)
endif()

target_link_libraries(${PROJECT_NAME}
		OBS::libobs)

target_include_directories(${PROJECT_NAME} PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}
		${LIBOBS_INCLUDE_DIRS}
		${LIBOBS_INCLUDEDIR}
		/usr/include/obs
		/usr/local/include/obs)

if(UNIX AND NOT APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH "$ORIGIN;/app/lib;/usr/lib"
    )
endif()

if(BUILD_OUT_OF_TREE)
    if(NOT LIB_OUT_DIR)
        set(LIB_OUT_DIR "/lib/obs-plugins")
    endif()
    if(NOT DATA_OUT_DIR)
        set(DATA_OUT_DIR "/share/obs/obs-plugins/${PROJECT_NAME}")
    endif()
    set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
    install(TARGETS ${PROJECT_NAME}
        LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/${LIB_OUT_DIR})
    install(DIRECTORY data/examples data/internal data/locale data/textures
        DESTINATION ${CMAKE_INSTALL_PREFIX}/${DATA_OUT_DIR})
    setup_plugin_target(${PROJECT_NAME})
else()
    if(OBS_CMAKE_VERSION VERSION_GREATER_EQUAL 3.0.0)
        set_target_properties_obs(${PROJECT_NAME} PROPERTIES FOLDER "plugins/exeldro" PREFIX "")
    else()
        set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "plugins/exeldro")
        setup_plugin_target(${PROJECT_NAME})
    endif()
endif()
