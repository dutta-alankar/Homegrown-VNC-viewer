# Copyright (C) 2022 The Qt Company Ltd.
# SPDX-License-Identifier: BSD-3-Clause
#
# Project-local override of Qt's FindWrapOpenGL.cmake.
#
# macOS 26 (Tahoe) removed the AGL framework but its stub still exists under
# /System/Library/Frameworks, so CMake's find_library locates it and Qt's
# upstream FindWrapOpenGL.cmake adds -framework AGL to the link command.
# The Apple linker then fails because the framework is no longer present.
# This file is an exact copy of Qt's module with the AGL block removed.
# It takes precedence over Qt's copy because CMAKE_MODULE_PATH is searched
# before CMAKE_PREFIX_PATH.

# We can't create the same interface imported target multiple times, CMake will complain if we do
# that. This can happen if the find_package call is done in multiple different subdirectories.
if(TARGET WrapOpenGL::WrapOpenGL)
    set(WrapOpenGL_FOUND ON)
    return()
endif()

set(WrapOpenGL_FOUND OFF)

find_package(OpenGL ${WrapOpenGL_FIND_VERSION})

if (OpenGL_FOUND)
    set(WrapOpenGL_FOUND ON)

    add_library(WrapOpenGL::WrapOpenGL INTERFACE IMPORTED)
    if(APPLE)
        # CMake 3.27 and older:
        # On Darwin platforms FindOpenGL sets IMPORTED_LOCATION to the absolute path of the library
        # within the framework. This ends up as an absolute path link flag, which we don't want,
        # because that makes our .prl files un-relocatable.
        # Extract the framework path instead, and use that in INTERFACE_LINK_LIBRARIES,
        # which CMake ends up transforming into a relocatable -framework flag.
        # See https://gitlab.kitware.com/cmake/cmake/-/issues/20871 for details.
        #
        # CMake 3.28 and above:
        # IMPORTED_LOCATION is the absolute path to the OpenGL.framework folder.
        get_target_property(__opengl_fw_lib_path OpenGL::GL IMPORTED_LOCATION)
        if(__opengl_fw_lib_path AND NOT __opengl_fw_lib_path MATCHES "/([^/]+)\\.framework$")
            get_filename_component(__opengl_fw_path "${__opengl_fw_lib_path}" DIRECTORY)
        endif()

        if(NOT __opengl_fw_path)
            # Just a safety measure in case if no OpenGL::GL target exists.
            set(__opengl_fw_path "-framework OpenGL")
        endif()

        # AGL was removed from macOS in macOS 26 (Tahoe). Its stub still exists under
        # /System/Library/Frameworks but the Apple linker can no longer resolve it.
        # We intentionally do not add -framework AGL here.
        target_link_libraries(WrapOpenGL::WrapOpenGL INTERFACE ${__opengl_fw_path})
    else()
        target_link_libraries(WrapOpenGL::WrapOpenGL INTERFACE OpenGL::GL)
    endif()
elseif(UNIX AND NOT APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "Integrity")
    # Requesting only the OpenGL component ensures CMake does not mark the package as
    # not found if neither GLX nor libGL are available. This allows finding OpenGL
    # on an X11-less Linux system.
    find_package(OpenGL ${WrapOpenGL_FIND_VERSION} COMPONENTS OpenGL)
    if (OpenGL_FOUND)
        set(WrapOpenGL_FOUND ON)
        add_library(WrapOpenGL::WrapOpenGL INTERFACE IMPORTED)
        target_link_libraries(WrapOpenGL::WrapOpenGL INTERFACE OpenGL::OpenGL)
    endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(WrapOpenGL DEFAULT_MSG WrapOpenGL_FOUND)
