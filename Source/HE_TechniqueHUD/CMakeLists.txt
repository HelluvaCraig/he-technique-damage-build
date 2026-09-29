cmake_minimum_required(VERSION 3.24)
project(HelluvaHUD VERSION 0.3.7 LANGUAGES CXX)

find_package(CommonLibSSE CONFIG REQUIRED)

set(SOURCES
    src/main.cpp
    src/pch.h
    src/StoneMap.h
)

add_commonlibsse_plugin(${PROJECT_NAME}
    AUTHOR "HE / OpenAI"
    SOURCES ${SOURCES}
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_precompile_headers(${PROJECT_NAME} PRIVATE src/pch.h)
target_include_directories(${PROJECT_NAME} PRIVATE src)
target_compile_definitions(${PROJECT_NAME} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /permissive- /Zc:preprocessor)
endif()
