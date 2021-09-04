cmake_minimum_required(VERSION 3.2)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake;/usr/local")

project(hwdectogl)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -W -Wall -Wextra -g -O2 -mfpu=neon")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -W -Wall -Wextra -g -O2 -mfpu=neon")

# ffmpeg search path, and others. using -shared- makes it build! And seems to work!
set(CMAKE_PREFIX_PATH "/home/pi/dev/rpi-ffmpeg/out/armv7-buster-shared-rel/install")
# Incorrect set(FFMPEG_ROOT "/usr")

find_package(PkgConfig REQUIRED)



## Find ffmpeg
##
# The Order (see 'Requires:' in /usr/local/lib/pkgconfig)
# /usr/local/lib/libavutil.a /usr/local/lib/libswscale.a  /usr/local/lib/libswresample.a /usr/local/lib/libavcodec.a /usr/local/lib/libavformat.a /usr/local/lib/libavfilter.a /usr/local/lib/libavdevice.a 

pkg_check_modules(PC_FFMPEG avcodec avutil avformat)

find_path(AVFORMAT_INCLUDE_DIR libavformat/avformat.h HINTS ${PC_FFMPEG_LIBAVFORMAT_INCLUDEDIR} ${PC_FFMPEG_INCLUDE_DIRS})
find_library(AVFORMAT_LIBRARY NAMES avformat HINTS ${PC_FFMPEG_LIBAVFORMAT_LIBDIR} ${PC_FFMPEG_LIBRARY_DIRS})

find_path(AVCODEC_INCLUDE_DIR libavcodec/avcodec.h HINTS ${PC_FFMPEG_LIBAVCODEC_INCLUDEDIR} ${PC_FFMPEG_INCLUDE_DIRS})
find_library(AVCODEC_LIBRARY NAMES avcodec HINTS ${PC_FFMPEG_LIBAVCODEC_LIBDIR} ${PC_FFMPEG_LIBRARY_DIRS})

find_path(AVUTIL_INCLUDE_DIR libavutil/avutil.h HINTS ${PC_FFMPEG_LIBAVUTIL_INCLUDEDIR} ${PC_FFMPEG_INCLUDE_DIRS})
find_library(AVUTIL_LIBRARY NAMES avutil HINTS ${PC_FFMPEG_LIBAVUTIL_LIBDIR} ${PC_FFMPEG_LIBRARY_DIRS})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(AVFormat DEFAULT_MSG AVFORMAT_LIBRARY AVFORMAT_INCLUDE_DIR)
find_package_handle_standard_args(AVCodec DEFAULT_MSG AVCODEC_LIBRARY AVCODEC_INCLUDE_DIR)
find_package_handle_standard_args(AVUtil DEFAULT_MSG AVUTIL_LIBRARY AVUTIL_INCLUDE_DIR)

mark_as_advanced(AVFORMAT_INCLUDE_DIR AVFORMAT_LIBRARY)
mark_as_advanced(AVCODEC_INCLUDE_DIR AVCODEC_LIBRARY)
mark_as_advanced(AVUTIL_INCLUDE_DIR AVUTIL_LIBRARY)

set(FFMPEG_INCLUDE_DIRS ${AVFORMAT_INCLUDE_DIR} ${AVCODEC_INCLUDE_DIR} ${AVUTIL_INCLUDE_DIR})
set(FFMPEG_LIBRARIES ${AVUTIL_LIBRARY} ${AVCODEC_LIBRARY} ${AVFORMAT_LIBRARY})

if(${AVFORMAT_FOUND} AND ${AVCODEC_FOUND} AND ${AVUTIL_FOUND})
	set(FFMPEG_FOUND TRUE)
	message("--- FFMPEG FOUND ---")
	#message("${FFMPEG_INCLUDE_DIRS}")
	message("${FFMPEG_LIBRARIES}")
else()
	set(FFMPEG_FOUND FALSE)
	message("--- FFMPEG NOT FOUND ---")
endif()


message("${AVCODEC_LDFLAGS}")

include_directories(
	include
	/usr/include/libdrm
	${FFMPEG_INCLUDE_DIRS}
	/home/pi/dev/rpi-ffmpeg
	/home/pi/dev/rpi-ffmpeg/out/armv7-buster-shared-rel )

add_executable(glHevc glHevc.c)
	
target_link_libraries(glHevc ${FFMPEG_LIBRARIES} -ldrm -lpthread -lm -lGLESv2 -lglfw -lEGL)


