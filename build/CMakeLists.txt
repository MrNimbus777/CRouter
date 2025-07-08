cmake_minimum_required(VERSION 3.10)
project(CRouter LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../bin)
set(BOOST_ROOT "") #path to boost
set(BOOST_LIBRARYDIR "${BOOST_ROOT}/stage/lib")
include_directories("${BOOST_ROOT}")

set(BOOST_LIB_SYSTEM "${BOOST_LIBRARYDIR}/libboost_system-mgw13-mt-x64-1_88.a")
set(BOOST_LIB_THREAD "${BOOST_LIBRARYDIR}/libboost_thread-mgw13-mt-x64-1_88.a")

add_executable(${PROJECT_NAME}
    ../src/main.cpp
)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ws2_32
        mswsock
        pthread
    )
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${BOOST_LIB_SYSTEM}
    ${BOOST_LIB_THREAD}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${BOOST_ROOT}
    ../src/libs/asio-1.34.2/include
    ../src/libs/rapidjson-master/include
    ../src/libs
)
