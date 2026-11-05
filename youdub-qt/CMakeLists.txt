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

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Quick Network Multimedia QuickControls2 QuickDialogs2 LinguistTools)

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
    src/viewmodels/SettingsVm.cpp
    src/viewmodels/SettingsVm.h
    src/upload/MultipartUpload.cpp
    src/upload/MultipartUpload.h
    main.cpp
)

# Theme.qml 要在模块中注册为单例：pragma Singleton 只约束 QML 编译器，
# 构建系统必须显式标记 QT_QML_SINGLETON_TYPE，否则 qmldir 里是普通类型，
# 运行时 Theme.xxx 绑定全部失败（背景变黑、间距归零、卡片高度塌陷）。
set_source_files_properties(qml/Theme.qml PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE
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
        qml/pages/Settings.qml
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
    Qt6::QuickDialogs2
)

# 让类型注册生成代码 __has_include(<AuthManager.h>) 能找到头文件。
target_include_directories(youdub-qt PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/network
    ${CMAKE_CURRENT_SOURCE_DIR}/src/models
    ${CMAKE_CURRENT_SOURCE_DIR}/src/viewmodels
    ${CMAKE_CURRENT_SOURCE_DIR}/src/upload
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video
)

# Windows 部署（windeployqt）：阶段 7 打包。
# 用法：cmake --build build --target deploy
if(WIN32)
    get_filename_component(_qt_bin_dir "${Qt6_DIR}/../../../bin" ABSOLUTE)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_target(deploy
            COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:youdub-qt>/translations"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "$<TARGET_FILE_DIR:youdub-qt>/youdub-qt_en.qm"
                    "$<TARGET_FILE_DIR:youdub-qt>/translations/youdub-qt_en.qm"
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                    --release
                    --no-translations
                    --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/qml"
                    "$<TARGET_FILE:youdub-qt>"
            DEPENDS youdub-qt
            COMMENT "Staging .qm + running windeployqt to bundle Qt runtime dependencies"
            VERBATIM
        )
    else()
        message(STATUS "windeployqt not found; skipping deploy target")
    endif()
endif()

# i18n：源串为中文，en 为英文翻译。lupdate 更新 .ts，lrelease 生成 .qm。
# 运行时从 <exe-dir>/translations/youdub-qt_en.qm 加载（见 main.cpp 回退路径）。
set(_ts_dir "${CMAKE_CURRENT_SOURCE_DIR}/translations")
qt_add_translations(youdub-qt
    TS_FILES
        "${_ts_dir}/youdub-qt_en.ts"
    QM_FILES_OUTPUT_VARIABLE _qm_files
)

# 把生成的 .qm 拷到 exe 同级 translations/ 子目录：在 deploy 目标里完成（避免 POST_BUILD 嵌套 cmd 触发沙箱问题）。

install(TARGETS youdub-qt
    RUNTIME DESTINATION bin
)
install(FILES ${_qm_files}
    DESTINATION bin/translations
)

# CPack：阶段7 出包。优先 ZIP 便携包；NSIS 若已安装则生成安装器。
set(CPACK_PACKAGE_NAME "YouDub")
set(CPACK_PACKAGE_VENDOR "YouDub")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "YouDub native desktop client (Qt6 / C++ / QML)")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "YouDub")
set(CPACK_GENERATORS "ZIP;NSIS")
include(CPack)
