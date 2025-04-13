cmake_minimum_required(VERSION 3.10)

project(vpxbct)

set(CMAKE_CXX_STANDARD 20)

find_package(PkgConfig REQUIRED)
find_package(OpenMP REQUIRED)

pkg_check_modules(IMAGEQUANT REQUIRED imagequant)

include_directories(
   ${CMAKE_SOURCE_DIR}
   ${IMAGEQUANT_INCLUDE_DIRS}
)

link_directories(
   ${IMAGEQUANT_LIBRARY_DIRS}
)

add_definitions(-DMG_MAX_RECV_SIZE=20971520)

add_executable(vpxbct
   main.cpp
   VPXBatoceraCaptureTool.h
   VPXBatoceraCaptureTool.cpp
   inc/mongoose/mongoose.h
   inc/mongoose/mongoose.c
   inc/lodepng/lodepng.cpp
   inc/lodepng/lodepng.h
)

target_link_libraries(vpxbct
   libimagequant.a
   OpenMP::OpenMP_CXX
)

add_custom_command(TARGET vpxbct POST_BUILD
   COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:vpxbct>/assets"
)
