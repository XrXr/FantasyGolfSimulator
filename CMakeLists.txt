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
set(shader_dir ${CMAKE_SOURCE_DIR}/shaders)

set(asset_headers "")

macro(generate_asset_header header_name asset_path tool)
    if(${ARGC} GREATER 3)
        set(extra_arg ${ARGV3})
    endif(${ARGC} GREATER 3)

    list(APPEND asset_headers ${header_name})

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
endmacro(generate_asset_header)

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

# shaders

macro(gen_shader_header name type)
    string(TOUPPER ${name}_${type} upper_snake)
    generate_asset_header(
        ${name}_${type}.h
        ${shader_dir}/${name}.${type}
        string_var
        ${upper_snake}
    )
endmacro(gen_shader_header)

gen_shader_header(mesh vs)
gen_shader_header(mesh fs)
gen_shader_header(window_space vs)
gen_shader_header(font fs)
gen_shader_header(ui fs)
gen_shader_header(grid vs)
gen_shader_header(grid gs)
gen_shader_header(grid fs)

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
