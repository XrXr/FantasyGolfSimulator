# Fantasy Golf Simulator
![Demo](http://i.imgur.com/oIPAKZQ.gif)

# Building
The app should run on OpenGL 3.3+ capable hardware on 64 bit GNU/Linux and Windows.
It is tested on Arch Linux and Windows 10.

Build Depenencies
 - CMake
 - SOIL
 - freeglut
 - OpenGL
 - GLEW

All the assets are made into header files and compiled into the final executable.
The app can't build with Visual Studio since VS's length limit for string
literal is 65535 characters.

Thanks to @syoyo for his mesh loader, [tinyobjloader-c](https://github.com/syoyo/tinyobjloader-c)!
