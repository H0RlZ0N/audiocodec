cmake_minimum_required(VERSION 2.8)

include_directories(${PROJECT_SOURCE_DIR}/thirdlib/ffmpeg/include)
include_directories(${PROJECT_SOURCE_DIR}/include)


link_directories(${PROJECT_SOURCE_DIR}/thirdlib/ffmpeg/lib)

aux_source_directory(./src/ c_file)


add_library(audiocodec SHARED ${c_file} )

target_link_libraries(audiocodec avdevice avformat avcodec avfilter swscale swresample avutil fdk-aac opencore-amrnb opencore-amrwb)
target_link_libraries(audiocodec libz.a libbz2.a libwinpthread.a libbcrypt.a libncrypt.a libiconv.a libwsock32.a libws2_32.a libole32.a libSecur32.a)