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

set(asset_headers "")

function(generate_asset_header header_name asset_path tool)
    if(${ARGC} GREATER 3)
        set(extra_arg ${ARGV3})
    endif(${ARGC} GREATER 3)

    list(APPEND asset_headers ${header_name})
    set(asset_headers ${asset_headers} PARENT_SCOPE)

    add_custom_command(
        OUTPUT ${header_name}
        DEPENDS ${tool}
        DEPENDS ${asset_path}
        COMMAND ${CMAKE_COMMAND}
            -DEXE_COMMAND=${tool}
            -DARG_ONE=${asset_path}
            -DARG_TWO=${extra_arg}
            -DOUTPUT_PATH=${CMAKE_BINARY_DIR}/${header_name}
            -P ${gen_dir}/save_standard_out.cmake
    )
endfunction(generate_asset_header)

generate_asset_header(bitmap_font_meta.h "${gen_dir}/dvmono.fnt" bitmap_meta)
generate_asset_header(bitmap_font_img.h ${gen_dir}/dvmono_0.png bitmap_img)
generate_asset_header(
    golf_ball_mesh.h
    ${gen_dir}/golf_ball.obj
    string_var
    GOLF_BALL_MESH
)
generate_asset_header(
    wind_arrow_mesh.h
    ${gen_dir}/arrow.obj
    string_var
    WIND_ARROW_MESH
)

file(GLOB project_src
    "*.h"
    "*.c"
)

add_executable(golf_sim
    ${project_src}
    ${asset_headers}
)
target_link_libraries(golf_sim
    ${OPENGL_LIBRARIES}
    ${GLUT_LIBRARIES}
    ${GLEW_LIBRARIES}
    m
)
