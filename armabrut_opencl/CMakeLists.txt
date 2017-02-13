cmake_minimum_required(VERSION 2.8)

## section: Macro
MACRO(ADD_MSVC_PRECOMPILED_HEADER PrecompiledHeader PrecompiledSource SourcesVar)
    IF (MSVC)
        GET_FILENAME_COMPONENT(PrecompiledBasename ${PrecompiledHeader} NAME_WE)
        SET(PrecompiledBinary "${CMAKE_CURRENT_BINARY_DIR}/${PrecompiledBasename}.pch")
        SET(Sources ${${SourcesVar}})

        SET_SOURCE_FILES_PROPERTIES(${PrecompiledSource}
                PROPERTIES COMPILE_FLAGS "/Yc\"${PrecompiledHeader}\" /Fp\"${PrecompiledBinary}\""
                OBJECT_OUTPUTS "${PrecompiledBinary}")
        SET_SOURCE_FILES_PROPERTIES(${Sources}
                PROPERTIES COMPILE_FLAGS "/Yu\"${PrecompiledBinary}\" /FI\"${PrecompiledBinary}\" /Fp\"${PrecompiledBinary}\""
                OBJECT_DEPENDS "${PrecompiledBinary}")
        # Add precompiled header to SourcesVar
        LIST(APPEND ${SourcesVar} ${PrecompiledSource})
    ENDIF (MSVC)
ENDMACRO(ADD_MSVC_PRECOMPILED_HEADER)

## start setting
SET(this_target cuda_test)
PROJECT(${this_target})

## section: include directory
INCLUDE_DIRECTORIES(
        $(AMDAPPSDKROOT)/include
)

## section: source files
SET(${this_target}_SOURCE_FILES
        armabrut.cpp
        brute_opencl.cpp
        dll.cpp
        )

## section: header files
SET(${this_target}_HEADER_FILES
        armabrut.h
        dll.h
        md5.h
        )

SOURCE_GROUP("Header Files" FILES
        armabrut.h
        md5.h
        dll.h
        )
SOURCE_GROUP("Source Files" FILES
        armabrut.cpp
        dll.cpp
        brute_opencl.cpp
        )


## section: precompiled header	
#ADD_MSVC_PRECOMPILED_HEADER("precompiled.h" "precompiled.cpp" MySources)
#ADD_LIBRARY(MyLibrary ${MySources})

SET_SOURCE_FILES_PROPERTIES(${this_target}_HEADER_FILES
        PROPERTIES HEADER_FILE_ONLY TRUE)
LIST(APPEND ${this_target}_SOURCE_FILES ${${this_target}_HEADER_FILES})

## section: add definitions
ADD_DEFINITIONS()

## section: add target
ADD_EXECUTABLE(${this_target} ${${this_target}_SOURCE_FILES})

## section: set link libraries
TARGET_LINK_LIBRARIES(${this_target}
        OpenCL.lib
        kernel32.lib
        user32.lib
        gdi32.lib
        winspool.lib
        comdlg32.lib
        advapi32.lib
        shell32.lib
        ole32.lib
        oleaut32.lib
        uuid.lib
        odbc32.lib
        odbccp32.lib
        )
