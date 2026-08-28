cmake_minimum_required(VERSION 3.21)

project(youdub-qt
    VERSION 0.1.0
    LANGUAGES CXX
    DESCRIPTION "YouDub native desktop client (Qt6 / C++ / QML)"
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 资源源码 UTF-8（Windows MSVC 默认 locale 不是 UTF-8）
if(MSVC)
    add_compile_options(/utf-8 /W3)
    add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

# 默认 Ninja（Qt 安装器附带的 Ninja），无则 Make/MSBuild。
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

# Qt 工具链前缀：优先 CMAKE_PREFIX_PATH，否则回退到本机 MinGW 安装路径。
if(NOT DEFINED CMAKE_PREFIX_PATH OR CMAKE_PREFIX_PATH STREQUAL "")
    file(TO_CMAKE_PATH "D:/Qt/6.8.3/mingw_64" _qt_default_prefix)
    if(EXISTS "${_qt_default_prefix}/lib/cmake/Qt6/Qt6Config.cmake")
        set(CMAKE_PREFIX_PATH "${_qt_default_prefix}" CACHE PATH "Qt6 install prefix" FORCE)
    endif()
endif()

# 后端 API 基地址（默认本机 FastAPI）。可由 -DYOUDUB_API_BASE_URL=... 覆盖。
set(YOUDUB_API_BASE_URL "http://127.0.0.1:8000" CACHE STRING "YouDub backend API base URL")
set(QML_IMPORT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/qml")

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Quick Network Multimedia QuickControls2)

qt_standard_project_setup(REQUIRES 6.5)

# --- QML 模块 --------------------------------------------------------------
qt_add_executable(youdub-qt WIN32
    src/network/CookieJar.cpp
    src/network/CookieJar.h
    src/network/ApiClient.cpp
    src/network/ApiClient.h
    src/network/AuthManager.cpp
    src/network/AuthManager.h
    src/models/TaskTypes.cpp
    src/models/TaskTypes.h
    src/models/TaskListModel.cpp
    src/models/TaskListModel.h
    src/models/StageModel.cpp
    src/models/StageModel.h
    src/viewmodels/TaskListVm.cpp
    src/viewmodels/TaskListVm.h
    src/viewmodels/TaskDetailVm.cpp
    src/viewmodels/TaskDetailVm.h
    main.cpp
)

qt_add_qml_module(youdub-qt
    URI YouDub
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/pages/Login.qml
        qml/pages/Home.qml
        qml/pages/TaskDetail.qml
        qml/components/StatusBadge.qml
        qml/components/Separator.qml
        qml/components/TaskListItem.qml
    RESOURCES
        qml/assets/youdub-icon.svg
)

# API 基地址注入运行时。
target_compile_definitions(youdub-qt PRIVATE
    YOUDUB_API_BASE_URL="${YOUDUB_API_BASE_URL}"
)

target_link_libraries(youdub-qt PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Network
    Qt6::Multimedia
)

# 让类型注册生成代码 __has_include(<AuthManager.h>) 能找到头文件。
target_include_directories(youdub-qt PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/network
    ${CMAKE_CURRENT_SOURCE_DIR}/src/models
    ${CMAKE_CURRENT_SOURCE_DIR}/src/viewmodels
    ${CMAKE_CURRENT_SOURCE_DIR}/src/upload
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video
)

# Windows 部署（windeployqt）：留到阶段 7 打包阶段统一处理。

# 安装规则（给 CPack/打包阶段用）。
install(TARGETS youdub-qt
    RUNTIME DESTINATION bin
)
