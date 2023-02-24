cmake_minimum_required(VERSION 2.8)

project(audiocodec)

if( WIN32 )
    if(DDEBUG_MODE)
       set(TARGETTYPE Debug)
    else(DDEBUG_MODE)
       set(TARGETTYPE Release)
    endif(DDEBUG_MODE)
elseif(UNIX)
    set(TARGETTYPE linux)
endif(WIN32)


include_directories(${PROJECT_SOURCE_DIR}/thirdlib/ffmpeg/include)
include_directories(${PROJECT_SOURCE_DIR}/include)


link_directories(${PROJECT_SOURCE_DIR}/thirdlib/ffmpeg/lib)

add_compile_options(-fPIC)

aux_source_directory(./src/ c_file)

add_library(audiocodec${NODE_TARGET} SHARED ${c_file} )

target_link_libraries(audiocodec${NODE_TARGET} avcodec-58 avdevice-58 avfilter-7 avformat-58 avutil-56 postproc-55 swresample-3 swscale-5)
