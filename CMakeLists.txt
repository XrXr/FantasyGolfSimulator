cmake_minimum_required (VERSION 3.5.2)
project (FantasyGolfSimulator)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(GLUT REQUIRED)

include_directories(
    ${OPENGL_INCLUDE_DIR}
    ${GLUT_INCLUDE_DIR}
    ${GLEW_INCLUDE_DIR}
)

add_executable(bitmap_img build_time_generation/bitmap_img)
target_link_libraries(bitmap_img SOIL ${OPENGL_LIBRARIES})

add_executable(bitmap_meta build_time_generation/bitmap_meta)
add_executable(string_var build_time_generation/string_var)

set(gen_dir ${CMAKE_SOURCE_DIR}/build_time_generation)

add_custom_command(
    OUTPUT bitmap_font_meta.h
    DEPENDS bitmap_img
    DEPENDS ${gen_dir}/dvmono.fnt
    COMMAND ${CMAKE_COMMAND}
        -DEXE_COMMAND=bitmap_meta
        -DARG_ONE=${gen_dir}/dvmono.fnt
        -DOUTPUT_PATH=${CMAKE_BINARY_DIR}/bitmap_font_meta.h
        -P ${gen_dir}/save_standard_out.cmake
)

add_custom_command(
    OUTPUT bitmap_font_img.h
    DEPENDS bitmap_img
    DEPENDS ${gen_dir}/dvmono_0.png
    COMMAND ${CMAKE_COMMAND}
        -DEXE_COMMAND=bitmap_img
        -DARG_ONE=${gen_dir}/dvmono_0.png
        -DOUTPUT_PATH=${CMAKE_BINARY_DIR}/bitmap_font_img.h
        -P ${gen_dir}/save_standard_out.cmake
)

add_custom_command(
    OUTPUT golf_ball_mesh.h
    DEPENDS string_var
    DEPENDS ${gen_dir}/golf_ball.obj
    COMMAND ${CMAKE_COMMAND}
        -DEXE_COMMAND=string_var
        -DARG_ONE=${gen_dir}/golf_ball.obj
        -DARG_TWO="GOLF_BALL_MESH"
        -DOUTPUT_PATH=${CMAKE_BINARY_DIR}/golf_ball_mesh.h
        -P ${gen_dir}/save_standard_out.cmake
)

add_custom_command(
    OUTPUT wind_arrow_mesh.h
    DEPENDS string_var
    DEPENDS ${gen_dir}/arrow.obj
    COMMAND ${CMAKE_COMMAND}
        -DEXE_COMMAND=string_var
        -DARG_ONE=${gen_dir}/arrow.obj
        -DARG_TWO="WIND_ARROW_MESH"
        -DOUTPUT_PATH=${CMAKE_BINARY_DIR}/wind_arrow_mesh.h
        -P ${gen_dir}/save_standard_out.cmake
)

file(GLOB project_src
    "*.h"
    "*.c"
)

add_executable(golf_sim
    ${project_src}
    bitmap_font_meta.h
    bitmap_font_img.h
    golf_ball_mesh.h
    wind_arrow_mesh.h
)
target_link_libraries(golf_sim
    ${OPENGL_LIBRARIES}
    ${GLUT_LIBRARIES}
    ${GLEW_LIBRARIES}
    m
)
