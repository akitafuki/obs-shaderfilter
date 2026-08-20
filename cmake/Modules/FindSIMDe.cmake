# FindSIMDe.cmake
# Find SIMDe header-only library

find_path(
  SIMDE_INCLUDE_DIR
  NAMES simde/simde-features.h simde/simde-common.h simde/x86/sse.h
  PATHS
    /opt/homebrew/include
    /usr/local/include
    /usr/include
    ${CMAKE_PREFIX_PATH}
  PATH_SUFFIXES
    simde
    include
    deps/simde
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SIMDe DEFAULT_MSG SIMDE_INCLUDE_DIR)

if(SIMDe_FOUND AND NOT TARGET SIMDe::SIMDe)
  add_library(SIMDe::SIMDe INTERFACE IMPORTED GLOBAL)
  set_target_properties(SIMDe::SIMDe PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${SIMDE_INCLUDE_DIR}"
  )
endif()
