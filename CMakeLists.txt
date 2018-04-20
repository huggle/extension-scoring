# This is a build file for Huggle (used with cmake)
# You will need to update it by hand!
cmake_minimum_required (VERSION 2.8.7)
PROJECT(huggle_scoring)
#  cmake <folder> -DQT5_BUILD=true
option(QT5_BUILD "Build huggle using the Qt5 framework" FALSE)
SET(CMAKE_CXX_FLAGS "-std=c++11")
SET(QT_USE_QTNETWORK TRUE)
SET(QT_USE_QTXML TRUE)
SET(QT_USE_QTDECLARATIVE TRUE)
SET(CMAKE_BUILD_TYPE Release)
SET(CMAKE_INCLUDE_CURRENT_DIR ON)
SET(CMAKE_AUTOMOC ON)
include_directories(../../)
#### Qt 4 and 5 ####
if(QT5_BUILD)
    find_package(Qt5Core REQUIRED)
    find_package(Qt5Gui REQUIRED)
    find_package(Qt5Xml REQUIRED)
    find_package(Qt5Widgets REQUIRED)
    find_package(Qt5Network REQUIRED)
    set(QT_INCLUDES
        ${Qt5Gui_INCLUDE_DIRS}
        ${Qt5Widgets_INCLUDE_DIRS}
        ${Qt5Network_INCLUDE_DIRS}
        ${Qt5Xml_INCLUDE_DIRS}
        ${Qt5Svg_INCLUDE_DIRS}
    )
    include_directories(${QT_INCLUDES})
else()
    find_package(Qt4 REQUIRED)
endif()
if (PYTHON_BUILD)
    find_package(PythonLibs REQUIRED)
    include_directories(${PYTHON_INCLUDE_DIRS})
endif()

file(GLOB huggle_scoring_srcx
    "src/*.cpp"
)
file (GLOB huggle_scoring_resx
    "src/*.qrc"
)
file (GLOB huggle_scoring_uixx
    "src/*.ui"
)

SET(huggle_scoring_SOURCES ${huggle_scoring_srcx})
SET(huggle_scoring_FORMS ${huggle_scoring_uixx})
SET(huggle_scoring_RESOURCES ${huggle_scoring_resx})
if(QT5_BUILD)
    QT5_WRAP_CPP(huggle_scoring_HEADERS_MOC ${huggle_scoring_HEADERS})
    QT5_WRAP_UI(huggle_scoring_FORMS_HEADERS ${huggle_scoring_FORMS})
    QT5_ADD_RESOURCES(huggle_scoring_RESOURCES_RCC ${huggle_scoring_RESOURCES})
else()
    QT4_WRAP_CPP(huggle_scoring_HEADERS_MOC ${huggle_scoring_HEADERS})
    QT4_WRAP_UI(huggle_scoring_FORMS_HEADERS ${huggle_scoring_FORMS})
    QT4_ADD_RESOURCES(huggle_scoring_RESOURCES_RCC ${huggle_scoring_RESOURCES})
endif()
if (NOT QT5_BUILD)
  INCLUDE(${QT_USE_FILE})
endif()
ADD_DEFINITIONS(${QT_DEFINITIONS})
ADD_DEFINITIONS( -DQT_USE_QSTRINGBUILDER )
ADD_LIBRARY(huggle_scoring SHARED ${huggle_scoring_SOURCES} ${huggle_scoring_HEADERS_MOC} ${huggle_scoring_FORMS_HEADERS} ${huggle_scoring_RESOURCES_RCC})
TARGET_LINK_LIBRARIES(huggle_scoring huggle_core)
if (NOT PYTHON_BUILD)
    TARGET_LINK_LIBRARIES(huggle_scoring ${QT_LIBRARIES})
else()
    TARGET_LINK_LIBRARIES(huggle_scoring ${PYTHON_LIBRARIES} ${QT_LIBRARIES})
endif()
if (QT5_BUILD)
    qt5_use_modules(huggle_scoring Core Gui Widgets Xml)
endif()
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})
if (LINUX_SNAP)
    INSTALL(TARGETS huggle_scoring DESTINATION bin/extensions)
else()
    INSTALL(TARGETS huggle_scoring DESTINATION share/huggle/extensions)
endif()
