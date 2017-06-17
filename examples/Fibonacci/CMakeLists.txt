cmake_minimum_required(VERSION 3.0)
project(Fibonacci C CXX)

add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)
add_dependencies(${PROJECT_NAME} generate-ocr-config)

include_directories(
    .
    $ENV{XSTG_ROOT}/apps/apps/libs/install/x86/include
    $ENV{XSTG_ROOT}/ocr/ocr/install/include
    $ENV{OCXXR_LIB_ROOT}/include
)

add_definitions(
    -DOCR_TYPE_H=x86.h
    -D__STDC_FORMAT_MACROS
)

find_package(Threads)

target_link_libraries(${PROJECT_NAME}
    $ENV{XSTG_ROOT}/ocr/ocr/install/lib/libocr_x86.a
    ${CMAKE_THREAD_LIBS_INIT}
)

add_custom_command(OUTPUT default.cfg
    COMMAND $ENV{XSTG_ROOT}/ocr/ocr/install/share/ocr/scripts/Configs/config-generator.py
    --threads 4 --remove-destination --output default.cfg
)

add_custom_target(generate-ocr-config
    DEPENDS default.cfg
)

add_custom_target(run
    COMMAND ${PROJECT_NAME} -ocr:cfg default.cfg $ENV{WORKLOAD_ARGS}
    DEPENDS ${PROJECT_NAME}
    WORKING_DIRECTORY ${CMAKE_PROJECT_DIR}
)

set(CMAKE_CXX_FLAGS "-std=c++14 -O2 -Werror -Wall -Wextra")
