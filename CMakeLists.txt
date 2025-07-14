cmake_minimum_required(VERSION 3.15)
project(BUDCController VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(FetchContent)

option(BUILD_CLI_ONLY "Build only the CLI application" OFF)

# --- Dependencies ---
find_package(PkgConfig REQUIRED)

# Handle macOS Homebrew paths
if(APPLE)
    # Add Homebrew paths for Apple Silicon and Intel Macs
    list(APPEND CMAKE_PREFIX_PATH 
        "/opt/homebrew"        # Apple Silicon
        "/usr/local"           # Intel Mac
    )
    set(ENV{PKG_CONFIG_PATH} "/opt/homebrew/lib/pkgconfig:/usr/local/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
endif()

pkg_check_modules(LIBSERIALPORT libserialport)

# Debug output to see what pkg-config found
if(LIBSERIALPORT_FOUND)
    message(STATUS "libserialport found:")
    message(STATUS "  LIBSERIALPORT_LIBRARIES: ${LIBSERIALPORT_LIBRARIES}")
    message(STATUS "  LIBSERIALPORT_LIBRARY_DIRS: ${LIBSERIALPORT_LIBRARY_DIRS}")
    message(STATUS "  LIBSERIALPORT_LDFLAGS: ${LIBSERIALPORT_LDFLAGS}")
    message(STATUS "  LIBSERIALPORT_INCLUDE_DIRS: ${LIBSERIALPORT_INCLUDE_DIRS}")
endif()

if(NOT LIBSERIALPORT_FOUND)
    # Fallback: try to find libserialport manually on macOS
    if(APPLE)
        find_library(LIBSERIALPORT_LIBRARIES 
            NAMES serialport libserialport
            PATHS /opt/homebrew/lib /usr/local/lib
            NO_DEFAULT_PATH
        )
        find_path(LIBSERIALPORT_INCLUDE_DIRS 
            NAMES libserialport.h
            PATHS /opt/homebrew/include /usr/local/include
            NO_DEFAULT_PATH
        )
        if(LIBSERIALPORT_LIBRARIES AND LIBSERIALPORT_INCLUDE_DIRS)
            set(LIBSERIALPORT_FOUND TRUE)
            # Set the LDFLAGS manually for macOS
            set(LIBSERIALPORT_LDFLAGS ${LIBSERIALPORT_LIBRARIES})
            message(STATUS "Found libserialport manually: ${LIBSERIALPORT_LIBRARIES}")
        endif()
    endif()
    
    # Fallback for Windows cross-compilation (MinGW)
    if(WIN32 AND CMAKE_CROSSCOMPILING)
        find_library(LIBSERIALPORT_LIBRARIES 
            NAMES serialport libserialport
            PATHS ${CMAKE_FIND_ROOT_PATH}/lib
            NO_DEFAULT_PATH
        )
        find_path(LIBSERIALPORT_INCLUDE_DIRS 
            NAMES libserialport.h
            PATHS ${CMAKE_FIND_ROOT_PATH}/include
            NO_DEFAULT_PATH
        )
        if(LIBSERIALPORT_LIBRARIES AND LIBSERIALPORT_INCLUDE_DIRS)
            set(LIBSERIALPORT_FOUND TRUE)
            message(STATUS "Found libserialport for Windows cross-compilation: ${LIBSERIALPORT_LIBRARIES}")
        endif()
    endif()
    
    if(NOT LIBSERIALPORT_FOUND)
        message(FATAL_ERROR "libserialport not found. Please install it for your distribution.")
    endif()
endif()

# --- GUI dependencies (only if building the GUI) ---
if(NOT BUILD_CLI_ONLY)
    find_package(OpenGL REQUIRED)

    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG        v1.92.1
    )
    FetchContent_MakeAvailable(imgui)

    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG        3.4
    )
    set(GLFW_BUILD_WAYLAND OFF)
    FetchContent_MakeAvailable(glfw)

    # Create embedded GLAD files with timestamp to force regeneration
    string(TIMESTAMP BUILD_TIMESTAMP "%Y%m%d%H%M%S")
    set(GLAD_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/glad_include_${BUILD_TIMESTAMP})
    set(GLAD_SRC_DIR ${CMAKE_CURRENT_BINARY_DIR}/glad_src_${BUILD_TIMESTAMP})
    
    file(MAKE_DIRECTORY ${GLAD_INCLUDE_DIR}/glad)
    file(MAKE_DIRECTORY ${GLAD_SRC_DIR})
    
    # Always regenerate GLAD files
    file(WRITE ${GLAD_INCLUDE_DIR}/glad/glad.h
"#ifndef GLAD_GLAD_H_
#define GLAD_GLAD_H_

#ifdef __cplusplus
extern \"C\" {
#endif

#include <stddef.h>

#ifndef APIENTRY
#define APIENTRY
#endif
#ifndef APIENTRYP
#define APIENTRYP APIENTRY *
#endif
#ifndef GLAPI
#define GLAPI extern
#endif

typedef void* (*GLADloadproc)(const char *name);

typedef void GLvoid;
typedef unsigned int GLenum;
typedef float GLfloat;
typedef int GLint;
typedef int GLsizei;
typedef unsigned int GLbitfield;
typedef double GLdouble;
typedef unsigned int GLuint;
typedef unsigned char GLboolean;
typedef unsigned char GLubyte;

#define GL_FALSE 0
#define GL_TRUE 1
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_TRIANGLES 0x0004

typedef void (APIENTRYP PFNGLCLEARPROC) (GLbitfield mask);
typedef void (APIENTRYP PFNGLCLEARCOLORPROC) (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
typedef void (APIENTRYP PFNGLDRAWARRAYSPROC) (GLenum mode, GLint first, GLsizei count);
typedef void (APIENTRYP PFNGLVIEWPORTPROC) (GLint x, GLint y, GLsizei width, GLsizei height);

GLAPI PFNGLCLEARPROC glad_glClear;
GLAPI PFNGLCLEARCOLORPROC glad_glClearColor;
GLAPI PFNGLDRAWARRAYSPROC glad_glDrawArrays;
GLAPI PFNGLVIEWPORTPROC glad_glViewport;

#define glClear glad_glClear
#define glClearColor glad_glClearColor
#define glDrawArrays glad_glDrawArrays
#define glViewport glad_glViewport

GLAPI int gladLoadGLLoader(GLADloadproc load);

#ifdef __cplusplus
}
#endif

#endif
")

    file(WRITE ${GLAD_SRC_DIR}/glad.c
"#include <glad/glad.h>
#include <stddef.h>

PFNGLCLEARPROC glad_glClear = NULL;
PFNGLCLEARCOLORPROC glad_glClearColor = NULL;
PFNGLDRAWARRAYSPROC glad_glDrawArrays = NULL;
PFNGLVIEWPORTPROC glad_glViewport = NULL;

int gladLoadGLLoader(GLADloadproc load) {
    glad_glClear = (PFNGLCLEARPROC)load(\"glClear\");
    glad_glClearColor = (PFNGLCLEARCOLORPROC)load(\"glClearColor\");
    glad_glDrawArrays = (PFNGLDRAWARRAYSPROC)load(\"glDrawArrays\");
    glad_glViewport = (PFNGLVIEWPORTPROC)load(\"glViewport\");
    return glad_glClear != NULL;
}
")
    
    # Create glad library target
    add_library(glad STATIC ${GLAD_SRC_DIR}/glad.c)
    target_include_directories(glad PUBLIC ${GLAD_INCLUDE_DIR})
    
    # Link with libdl on Linux
    if(UNIX AND NOT APPLE)
        target_link_libraries(glad PRIVATE ${CMAKE_DL_LIBS})
    endif()

    set(IMGUI_SOURCES
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    )
endif()

# --- Core C Library ---
add_library(budc_scpi src/budc_scpi.c)
target_include_directories(budc_scpi PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(LIBSERIALPORT_FOUND)
    target_include_directories(budc_scpi PUBLIC ${LIBSERIALPORT_INCLUDE_DIRS})
    
    # Add library directories with PUBLIC visibility so they propagate to executables
    if(LIBSERIALPORT_LIBRARY_DIRS)
        target_link_directories(budc_scpi PUBLIC ${LIBSERIALPORT_LIBRARY_DIRS})
    endif()
    
    # Use target_link_libraries with PUBLIC visibility
    if(LIBSERIALPORT_LIBRARIES)
        target_link_libraries(budc_scpi PUBLIC ${LIBSERIALPORT_LIBRARIES})
    endif()
    
    # For pkg-config results, also process LDFLAGS which contains both -L and -l flags
    if(LIBSERIALPORT_LDFLAGS)
        # Convert LDFLAGS to a list and apply all flags with PUBLIC visibility
        string(REPLACE " " ";" LDFLAGS_LIST ${LIBSERIALPORT_LDFLAGS})
        foreach(flag ${LDFLAGS_LIST})
            if(flag MATCHES "^-L")
                # Extract library directory from -L flag
                string(SUBSTRING ${flag} 2 -1 lib_dir)
                target_link_directories(budc_scpi PUBLIC ${lib_dir})
            elseif(flag MATCHES "^-l")
                # Extract library name from -l flag  
                string(SUBSTRING ${flag} 2 -1 lib_name)
                target_link_libraries(budc_scpi PUBLIC ${lib_name})
            else()
                # Other flags
                target_link_options(budc_scpi PUBLIC ${flag})
            endif()
        endforeach()
    endif()
endif()

# Disable debug in Release builds
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(budc_scpi PRIVATE BUDC_DEBUG=0)
else()
    target_compile_definitions(budc_scpi PRIVATE BUDC_DEBUG=1)
endif()

if(WIN32)
    target_link_libraries(budc_scpi PRIVATE setupapi ole32)
endif()

# --- Executables ---
add_executable(budc_cli src/cli.c)
target_link_libraries(budc_cli PRIVATE budc_scpi)

# Since budc_scpi is a static library, we need to propagate the libserialport dependency

if(NOT BUILD_CLI_ONLY)
    add_executable(budc_gui
        src/gui.cpp
        ${IMGUI_SOURCES}
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    )
    
    target_include_directories(budc_gui PRIVATE
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
    )

    target_link_libraries(budc_gui PRIVATE
        budc_scpi
        glfw
        glad
        OpenGL::GL
    )
    
    if(WIN32)
        target_link_libraries(budc_gui PRIVATE gdi32 user32 kernel32)
    endif()
endif()

# --- Installation & Packaging Rules ---
install(TARGETS budc_cli RUNTIME DESTINATION bin COMPONENT cli)
if(NOT BUILD_CLI_ONLY)
    install(TARGETS budc_gui RUNTIME DESTINATION bin COMPONENT gui)
endif()

# --- CPack Configuration for Debian (.deb) Packages ---
set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Controller for BUDC devices from Lotus systems")
set(CPACK_PACKAGE_VENDOR "Sebastien Dudek (Penthertz)")
set(CPACK_PACKAGE_CONTACT "contact@penthertz.com")
set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${CPACK_PACKAGE_VENDOR} <${CPACK_PACKAGE_CONTACT}>")
set(CPACK_DEBIAN_PACKAGE_SECTION "electronics")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")

# Define runtime dependencies for the executables
set(BUDC_GUI_DEPS
    "libc6 (>= 2.34)"
    "libserialport0" 
    "libglfw3"
    "libgl1"
    "libx11-6"
    "libxrandr2"
    "libxinerama1"
    "libxcursor1"
    "libxi6"
)
set(BUDC_CLI_DEPS
    "libc6 (>= 2.34)"
    "libserialport0"
)

# Set the final dependency string for the package
if(NOT BUILD_CLI_ONLY)
    string(REPLACE ";" ", " CPACK_DEBIAN_PACKAGE_DEPENDS "${BUDC_GUI_DEPS}")
else()
    string(REPLACE ";" ", " CPACK_DEBIAN_PACKAGE_DEPENDS "${BUDC_CLI_DEPS}")
endif()

# Include the CPack module at the very end
include(CPack)