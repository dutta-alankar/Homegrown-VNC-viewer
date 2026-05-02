cmake_minimum_required(VERSION 3.20)
project(vnc_client_app VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(VNC_USE_SUBMODULE_DEPS "Prefer dependencies from git submodules under external/" ON)

# Project-local cmake module overrides (e.g. FindWrapOpenGL without defunct AGL on macOS 26+).
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

if (VNC_USE_SUBMODULE_DEPS)
    if (EXISTS ${CMAKE_SOURCE_DIR}/external/qtbase/lib/cmake/Qt6)
        list(PREPEND CMAKE_PREFIX_PATH ${CMAKE_SOURCE_DIR}/external/qtbase)
    endif()
endif()

find_package(Qt6 COMPONENTS Core Gui Widgets QUIET)
if (Qt6_FOUND)
    set(QT_PACKAGE Qt6)
else()
    find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets)
    set(QT_PACKAGE Qt5)
endif()

# macOS 26 (Tahoe) removed AGL but its stub still exists, causing ld to fail.
# cmake/FindWrapOpenGL.cmake (project-local) prevents the WrapOpenGL target from
# adding AGL. As a belt-and-suspenders measure, patch Qt's prl files at configure
# time (idempotent) and strip AGL from all relevant imported target interfaces.
if (APPLE AND Qt6_FOUND)
    get_filename_component(_qt_lib_dir "${Qt6_DIR}/../../.." ABSOLUTE)
    foreach(_fw QtGui QtWidgets)
        set(_prl "${_qt_lib_dir}/${_fw}.framework/Versions/A/Resources/${_fw}.prl")
        if (EXISTS "${_prl}")
            file(READ "${_prl}" _prl_content)
            if (_prl_content MATCHES "-framework AGL")
                string(REPLACE " -framework AGL" "" _prl_content "${_prl_content}")
                string(REPLACE ";-framework AGL" "" _prl_content "${_prl_content}")
                file(WRITE "${_prl}" "${_prl_content}")
                message(STATUS "Patched ${_prl}: removed defunct -framework AGL (macOS 26+)")
            endif()
        endif()
    endforeach()
    foreach(_tgt WrapOpenGL::WrapOpenGL Qt6::Gui Qt6::Widgets Qt6::Core)
        if (TARGET ${_tgt})
            get_target_property(_libs ${_tgt} INTERFACE_LINK_LIBRARIES)
            if (_libs)
                list(FILTER _libs EXCLUDE REGEX "AGL")
                set_property(TARGET ${_tgt} PROPERTY INTERFACE_LINK_LIBRARIES "${_libs}")
            endif()
        endif()
    endforeach()
endif()

set(VNC_CLIENT_TARGET "")
set(VNC_CLIENT_INCLUDE_DIRS "")
set(VNC_CLIENT_LIBRARIES "")
set(VNC_CLIENT_COMPILE_OPTIONS "")

if (VNC_USE_SUBMODULE_DEPS AND EXISTS ${CMAKE_SOURCE_DIR}/external/libvncserver/CMakeLists.txt)
    add_subdirectory(external/libvncserver EXCLUDE_FROM_ALL)
    if (TARGET vncclient)
        set(VNC_CLIENT_TARGET vncclient)
        list(APPEND VNC_CLIENT_INCLUDE_DIRS
            ${CMAKE_SOURCE_DIR}/external/libvncserver/include
            ${CMAKE_BINARY_DIR}/external/libvncserver/include
        )
    endif()
endif()

if (NOT VNC_CLIENT_TARGET)
    find_package(PkgConfig QUIET)
    if (PkgConfig_FOUND)
        pkg_check_modules(LIBVNC QUIET libvncclient)
    endif()

    if (LIBVNC_FOUND)
        set(VNC_CLIENT_INCLUDE_DIRS ${LIBVNC_INCLUDE_DIRS})
        set(VNC_CLIENT_LIBRARIES ${LIBVNC_LIBRARIES})
        set(VNC_CLIENT_COMPILE_OPTIONS ${LIBVNC_CFLAGS_OTHER})
    else()
        find_path(LIBVNC_INCLUDE_DIRS NAMES rfb/rfbclient.h)
        find_library(LIBVNC_CLIENT_LIBRARY NAMES vncclient)

        if (NOT LIBVNC_INCLUDE_DIRS OR NOT LIBVNC_CLIENT_LIBRARY)
            message(FATAL_ERROR "LibVNCClient not found. Initialize submodules or install libvncclient development files.")
        endif()

        set(VNC_CLIENT_INCLUDE_DIRS ${LIBVNC_INCLUDE_DIRS})
        set(VNC_CLIENT_LIBRARIES ${LIBVNC_CLIENT_LIBRARY})
    endif()
endif()

add_executable(vnc-client
    src/main.cpp
    src/resources.qrc
    src/ConnectionProfile.h
    src/ConnectionStore.h
    src/ConnectionStore.cpp
    src/ConnectionDialog.h
    src/ConnectionDialog.cpp
    src/SshTunnel.h
    src/SshTunnel.cpp
    src/VncClientSession.h
    src/VncClientSession.cpp
    src/VncViewerWidget.h
    src/VncViewerWidget.cpp
    src/MainWindow.h
    src/MainWindow.cpp
)

target_include_directories(vnc-client PRIVATE
    ${VNC_CLIENT_INCLUDE_DIRS}
)

target_link_libraries(vnc-client PRIVATE
    ${QT_PACKAGE}::Core
    ${QT_PACKAGE}::Gui
    ${QT_PACKAGE}::Widgets
)

if (VNC_CLIENT_TARGET)
    target_link_libraries(vnc-client PRIVATE ${VNC_CLIENT_TARGET})
else()
    target_link_libraries(vnc-client PRIVATE ${VNC_CLIENT_LIBRARIES})
endif()

if (VNC_CLIENT_COMPILE_OPTIONS)
    target_compile_options(vnc-client PRIVATE ${VNC_CLIENT_COMPILE_OPTIONS})
endif()

if (UNIX AND NOT APPLE)
    target_link_libraries(vnc-client PRIVATE pthread)
endif()

install(TARGETS vnc-client RUNTIME DESTINATION bin)
