cmake_minimum_required (VERSION 3.5.2)
project (FantasyGolfSimulator)

find_package(OpenGL REQUIRED)
find_package(GLUT REQUIRED)
find_package(GLEW REQUIRED)

include_directories( ${OPENGL_INCLUDE_DIR}  ${GLUT_INCLUDE_DIR} )

add_executable(golf_sim main.c)
target_link_libraries(golf_sim
    ${OPENGL_LIBRARIES}
    ${GLUT_LIBRARIES}
    ${GLEW_LIBRARIES}
)

