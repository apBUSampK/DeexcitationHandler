cmake_minimum_required(VERSION 3.14)

project(
    Deexcitation
    VERSION 1.0.0
    LANGUAGES CXX
)

find_package(Geant4 REQUIRED)

find_package(COLA REQUIRED)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(CMAKE_INSTALL_PREFIX ${COLA_DIR})
set(COLA_MODULE Deexcitation)

set(COLA_CMAKE_CXX_CLANG_TIDY
    ""
    CACHE STRING "Passed to CMAKE_CXX_CLANG_TIDY for ${COLA_MODULE} targets"
)

file(GLOB SOURCES CONFIGURE_DEPENDS Deexcitation/*.cpp Deexcitation/handler/*.cpp)

add_library(${COLA_MODULE} SHARED ${SOURCES})

target_link_libraries(${COLA_MODULE} PUBLIC COLA ${Geant4_LIBRARIES})
target_include_directories(${COLA_MODULE} PUBLIC ${Geant4_INCLUDE_DIR})
target_compile_options(${COLA_MODULE} PRIVATE -Wall -Werror -Wextra -Wpedantic)
target_compile_features(${COLA_MODULE} PRIVATE cxx_std_20)

target_include_directories(
    ${COLA_MODULE} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
                          $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/Deexcitation>
)

set_target_properties(
    ${COLA_MODULE}
    PROPERTIES VERSION "${PROJECT_VERSION}"
               SOVERSION "${PROJECT_VERSION_MAJOR}"
               CXX_CLANG_TIDY "${COLA_CMAKE_CXX_CLANG_TIDY}"
               INSTALL_RPATH_USE_LINK_PATH TRUE
)

install(
    TARGETS ${COLA_MODULE}
    EXPORT ${COLA_MODULE}Export
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
    DIRECTORY ${COLA_MODULE}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING
    PATTERN "*.h"
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/${COLA_MODULE}Config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${COLA_MODULE}
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/${COLA_MODULE}ConfigVersion.cmake
    VERSION ${Deexcitation_VERSION}
    COMPATIBILITY AnyNewerVersion
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${COLA_MODULE}Config.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/${COLA_MODULE}ConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${COLA_MODULE}
)

install(
    EXPORT ${COLA_MODULE}Export
    FILE ${COLA_MODULE}Export.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${COLA_MODULE}
)

enable_testing()
if (BUILD_TESTING)
    add_subdirectory(tests)
endif ()

setup_cola_quality_tools(EXCLUDE_PATTERNS "(^|/)COLA/" "(^|/)geant4/")
