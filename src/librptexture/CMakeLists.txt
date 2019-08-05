PROJECT(librpbase)

# ZLIB and libpng are checked in the top-level CMakeLists.txt.

# Sources.
SET(librptexture_SRCS
	img/rp_image.cpp
	img/rp_image_backend.cpp
	img/rp_image_ops.cpp
	img/un-premultiply.cpp
	img/RpImageLoader.cpp

	decoder/ImageDecoder_Linear.cpp
	decoder/ImageDecoder_GCN.cpp
	decoder/ImageDecoder_NDS.cpp
	decoder/ImageDecoder_N3DS.cpp
	decoder/ImageDecoder_S3TC.cpp
	decoder/ImageDecoder_DC.cpp
	decoder/ImageDecoder_ETC1.cpp
	decoder/ImageDecoder_BC7.cpp
	decoder/PixelConversion.cpp
	)
# Headers.
SET(librptexture_H
	img/rp_image.hpp
	img/rp_image_p.hpp
	img/rp_image_backend.hpp
	img/RpImageLoader.hpp

	decoder/ImageDecoder.hpp
	decoder/ImageDecoder_p.hpp
	decoder/PixelConversion.hpp
	)

IF(JPEG_FOUND)
	SET(librptexture_SRCS ${librptexture_SRCS} img/RpJpeg.cpp)
	SET(librptexture_H ${librptexture_H} img/RpJpeg.hpp)
ENDIF(JPEG_FOUND)

IF(WIN32)
	SET(librptexture_OS_SRCS
		img/GdiplusHelper.cpp
		img/RpGdiplusBackend.cpp
		)
	SET(librptexture_OS_H
		img/GdiplusHelper.hpp
		img/RpGdiplusBackend.hpp
		)
ENDIF(WIN32)

# CPU-specific and optimized sources.
IF(CPU_i386 OR CPU_amd64)
	# MSVC does not support MMX intrinsics in 64-bit builds.
	# Reference: https://msdn.microsoft.com/en-us/library/08x3t697(v=vs.110).aspx
	#
	# amd64 CPUs support SSE2 as a minimum anyway, so there's
	# no point in building MMX code for 64-bit.
	SET(librptexture_SSE2_SRCS
		img/rp_image_ops_sse2.cpp
		decoder/ImageDecoder_Linear_sse2.cpp
		)
	SET(librptexture_SSSE3_SRCS
		decoder/ImageDecoder_Linear_ssse3.cpp
		)
	IF(JPEG_FOUND)
		SET(librptexture_SSSE3_SRCS
			${librptexture_SSSE3_SRCS}
			img/RpJpeg_ssse3.cpp
			)
	ENDIF(JPEG_FOUND)
	# TODO: Disable SSE 4.1 if not supported by the compiler?
	SET(librptexture_SSE41_SRCS
		img/un-premultiply_sse41.cpp
		)

	# IFUNC requires glibc.
	# We're not checking for glibc here, but we do have preprocessor
	# checks, so even if this does get compiled on a non-glibc system,
	# it won't do anything.
	# TODO: Might be supported on other Unix-like operating systems...
	IF(UNIX AND NOT APPLE)
		SET(librptexture_IFUNC_SRCS decoder/ImageDecoder_ifunc.cpp)
		# Disable LTO on the IFUNC files if LTO is known to be broken.
		IF(GCC_5xx_LTO_ISSUES)
			FOREACH(ifunc_file ${librptexture_IFUNC_SRCS})
				SET_SOURCE_FILES_PROPERTIES(${ifunc_file}
					APPEND_STRING PROPERTIES COMPILE_FLAGS " -fno-lto ")
			ENDFOREACH()
		ENDIF(GCC_5xx_LTO_ISSUES)
	ENDIF(UNIX AND NOT APPLE)

	IF(MSVC AND NOT CMAKE_CL_64)
		SET(SSE2_FLAG "/arch:SSE2")
		SET(SSSE3_FLAG "/arch:SSE2")
		SET(SSE41_FLAG "/arch:SSE2")
	ELSEIF(NOT MSVC)
		# TODO: Other compilers?
		SET(MMX_FLAG "-mmmx")
		SET(SSE2_FLAG "-msse2")
		SET(SSSE3_FLAG "-mssse3")
		SET(SSE41_FLAG "-msse4.1")
	ENDIF()

	IF(MMX_FLAG)
		FOREACH(mmx_file ${librptexture_MMX_SRCS})
			SET_SOURCE_FILES_PROPERTIES(${mmx_file}
				APPEND_STRING PROPERTIES COMPILE_FLAGS " ${MMX_FLAG} ")
		ENDFOREACH()
	ENDIF(MMX_FLAG)

	IF(SSE2_FLAG)
		FOREACH(sse2_file ${librptexture_SSE2_SRCS})
			SET_SOURCE_FILES_PROPERTIES(${sse2_file}
				APPEND_STRING PROPERTIES COMPILE_FLAGS " ${SSE2_FLAG} ")
		ENDFOREACH()
	ENDIF(SSE2_FLAG)

	IF(SSSE3_FLAG)
		FOREACH(ssse3_file ${librptexture_SSSE3_SRCS})
			SET_SOURCE_FILES_PROPERTIES(${ssse3_file}
				APPEND_STRING PROPERTIES COMPILE_FLAGS " ${SSSE3_FLAG} ")
		ENDFOREACH()
	ENDIF(SSSE3_FLAG)

	IF(SSE41_FLAG)
		FOREACH(sse41_file ${librptexture_SSE41_SRCS})
			SET_SOURCE_FILES_PROPERTIES(${sse41_file}
				APPEND_STRING PROPERTIES COMPILE_FLAGS " ${SSE41_FLAG} ")
		ENDFOREACH()
	ENDIF(SSE41_FLAG)
ENDIF()
UNSET(arch)

# Write the config.h files.
CONFIGURE_FILE("${CMAKE_CURRENT_SOURCE_DIR}/config.librptexture.h.in" "${CMAKE_CURRENT_BINARY_DIR}/config.librptexture.h")

######################
# Build the library. #
######################

ADD_LIBRARY(rptexture STATIC
	${librptexture_SRCS} ${librptexture_H}
	${librptexture_OS_SRCS} ${librptexture_OS_H}
	${librptexture_IFUNC_SRCS}
	${librptexture_MMX_SRCS}
	${librptexture_SSE2_SRCS}
	${librptexture_SSSE3_SRCS}
	${librptexture_SSE41_SRCS}
	)
INCLUDE(SetMSVCDebugPath)
SET_MSVC_DEBUG_PATH(rptexture)

# TODO: More public/private library stuff.
TARGET_LINK_LIBRARIES(rptexture PUBLIC rpbase)

# Include paths:
# - Public: Current source and binary directories.
# - Private: Parent source and binary directories,
#            and top-level binary directory for git_version.h.
TARGET_INCLUDE_DIRECTORIES(rptexture
	PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>		# librptexture
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>		# librptexture
	PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>	# src
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/..>	# src
		$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>			# build
	)
IF(NOT ZLIB_LIBRARY)
	MESSAGE(FATAL_ERROR "ZLIB_LIBRARY has not been set by CheckZLIB.cmake.")
ENDIF(NOT ZLIB_LIBRARY)
TARGET_LINK_LIBRARIES(rptexture PRIVATE ${ZLIB_LIBRARY})
IF(MSVC)
	TARGET_LINK_LIBRARIES(rptexture PRIVATE delayimp)
ENDIF(MSVC)
# Exclude from ALL builds.
SET_TARGET_PROPERTIES(rptexture PROPERTIES EXCLUDE_FROM_ALL TRUE)

# Other libraries.
IF(ZLIB_FOUND)
	TARGET_LINK_LIBRARIES(rptexture PRIVATE ${ZLIB_LIBRARIES})
	TARGET_INCLUDE_DIRECTORIES(rptexture PRIVATE ${ZLIB_INCLUDE_DIRS})
	TARGET_COMPILE_DEFINITIONS(rptexture PRIVATE ${ZLIB_DEFINITIONS})
ENDIF(ZLIB_FOUND)
IF(PNG_FOUND)
	TARGET_LINK_LIBRARIES(rptexture PRIVATE ${PNG_LIBRARY})
	TARGET_INCLUDE_DIRECTORIES(rptexture PRIVATE ${PNG_INCLUDE_DIRS})
	TARGET_COMPILE_DEFINITIONS(rptexture PRIVATE ${PNG_DEFINITIONS})
ENDIF(PNG_FOUND)
IF(JPEG_FOUND)
	TARGET_LINK_LIBRARIES(rptexture PRIVATE ${JPEG_LIBRARY})
	TARGET_INCLUDE_DIRECTORIES(rptexture PRIVATE ${JPEG_INCLUDE_DIRS})
ENDIF(JPEG_FOUND)
IF(WIN32)
	# libwin32common
	TARGET_LINK_LIBRARIES(rptexture PRIVATE win32common)
	# Windows system libraries
	TARGET_LINK_LIBRARIES(rptexture PRIVATE gdiplus)
ENDIF(WIN32)

# Unix: Add -fpic/-fPIC in order to use this static library in plugins.
IF(UNIX AND NOT APPLE)
	SET(CMAKE_C_FLAGS	"${CMAKE_C_FLAGS} -fpic -fPIC")
	SET(CMAKE_CXX_FLAGS	"${CMAKE_CXX_FLAGS} -fpic -fPIC")
ENDIF(UNIX AND NOT APPLE)

# Test suite.
IF(BUILD_TESTING)
	ADD_SUBDIRECTORY(tests)
ENDIF(BUILD_TESTING)