cmake_minimum_required(VERSION 2.8)
SET(PROJECT_NAME socket)
project(${PROJECT_NAME})

# point to cosmos/source folder
set(COSMOS_SOURCE ${PROJECT_SOURCE_DIR}/../../..)
set(USE_COSMOS_FROM "SOURCE")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic -std=c++17 -Wno-invalid-offsetof")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

# -----------------------------------------------
# To use COSMOS from the source folder
include(${COSMOS_SOURCE}/core/cmake/use_cosmos_from_source.cmake)

set(COSMOS_LIBS
    CosmosSupport
    CosmosAgent
    CosmosDeviceGeneral
    CosmosDeviceSerial
    CosmosDeviceCpu
    CosmosDeviceDisk
    )

find_package(OpenSSL REQUIRED)
find_package(Boost COMPONENTS system REQUIRED)

# -----------------------------------------------
# Windows specific
if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    set(COSMOS_LIBS ${COSMOS_LIBS} ws2_32)
endif()

include_directories(${PROJECT_SOURCE_DIR}/include)
add_executable(agent_socket agent_socket.cpp)

target_include_directories(agent_socket
  PRIVATE ${PROJECT_SOURCE_DIR}/include
  ${Boost_INCLUDE_DIR}
)

target_link_libraries(agent_socket
  PRIVATE
  ${COSMOS_LIBS}
  OpenSSL::SSL
  OpenSSL::Crypto
  ${Boost_SYSTEM_LIBRARY}
  stdc++fs
)

#target_link_libraries(agent_mongo ${COSMOS_LIBS} bson-1.0 bsoncxx mongoc-1.0 mongocxx ssl crypto boost_system stdc++fs)

# -----------------------------------------------
# to deploy on a remote device
#file(WRITE "${CMAKE_SOURCE_DIR}/QtCreatorDeployment.txt" "cosmos/bin\n")

#macro(add_deployment_file SRC DEST)
#    file(RELATIVE_PATH path ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
#    file(APPEND "${CMAKE_SOURCE_DIR}/QtCreatorDeployment.txt" "${path}/${SRC}:${DEST}\n")
#endmacro()

#macro(add_deployment_directory SRC DEST)
#    file(GLOB_RECURSE files RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "${SRC}/*")
#    foreach(filename ${files})
#        get_filename_component(path ${filename} PATH)
#        add_deployment_file("${filename}" "${DEST}/${path}")
#    endforeach(filename)
#endmacro()

#set(program_name programs/agents/agent)
#file(RELATIVE_PATH relativeDir ${PROJECT_SOURCE_DIR} ${PROJECT_BINARY_DIR}/${program_name})
#add_deployment_file(${relativeDir} "bin")

#set(program_name "${COSMOS_SOURCE}/core/programs/agents/agent_cpu")
#file(RELATIVE_PATH relativeDir ${PROJECT_SOURCE_DIR} ${PROJECT_BINARY_DIR}/${program_name})
#add_deployment_file(${relativeDir} "bin")

