cmake_minimum_required(VERSION 3.2)

project(dit)

set(CMAKE_CXX_STANDARD 14)

find_package(libcluon REQUIRED)
include_directories(SYSTEM ${CLUON_INCLUDE_DIRS})

add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/messages.cpp
                   WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                   COMMAND cluon-msc --cpp-sources --cpp-add-include-file=messages.hpp --out=${CMAKE_BINARY_DIR}/messages.cpp ${CMAKE_CURRENT_SOURCE_DIR}/messages.odvd
                   COMMAND cluon-msc --cpp-headers --out=${CMAKE_BINARY_DIR}/messages.hpp ${CMAKE_CURRENT_SOURCE_DIR}/messages.odvd
DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/messages.odvd)

include_directories(SYSTEM ${CMAKE_BINARY_DIR})

add_executable(${PROJECT_NAME}.Sender ${CMAKE_CURRENT_SOURCE_DIR}/Sender.cpp ${CMAKE_BINARY_DIR}/messages.cpp)
target_link_libraries(${PROJECT_NAME}.Sender ${CLUON_LIBRARIES})

add_executable(${PROJECT_NAME}.Receiver ${CMAKE_CURRENT_SOURCE_DIR}/Receiver.cpp ${CMAKE_BINARY_DIR}/messages.cpp)
target_link_libraries(${PROJECT_NAME}.Receiver ${CLUON_LIBRARIES})

#enable_testing()
#add_executable(${PROJECT_NAME}-Runner TestPrimeChecker.cpp ${CMAKE_CURRENT_SOURCE_DIR}/PrimeChecker.cpp ${CMAKE_BINARY_DIR}/messages.cpp)
#target_link_libraries(${PROJECT_NAME}-Runner ${CLUON_LIBRARIES})


