cmake_minimum_required(VERSION 3.28)
set(PROJECT_NAME "AndesiteEditor")
project(${PROJECT_NAME}
        VERSION 0.1
        LANGUAGES C CXX OBJC
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/CPM.cmake)

# ---------------- my framework ---------------------
CPMAddPackage(
        NAME mythril
#        SOURCE_DIR "$ENV{HOME}/Projects/Personal/mythril_library"
        URI "gh:externalhost0/Mythril#main"
        OPTIONS
        "MYTH_ENABLE_IMGUI_STANDARD OFF"
        "MYTH_ENABLE_IMGUI_DOCKING ON"
        "RUN_SAMPLES OFF"
)

#-------------- ImNodeFlow -------------------
CPMAddPackage(
        NAME ImNodeFlow
        GITHUB_REPOSITORY "Fattorino/ImNodeFlow"
        GIT_TAG "master"
)
if(ImNodeFlow_ADDED)
    add_library(ImNodeFlow STATIC
            ${ImNodeFlow_SOURCE_DIR}/src/ImNodeFlow.cpp
    )
    target_include_directories(ImNodeFlow
            PUBLIC
            ${ImNodeFlow_SOURCE_DIR}/include
    )
    target_link_libraries(ImNodeFlow PUBLIC mythril)
    target_compile_definitions(ImNodeFlow PUBLIC IMGUI_DEFINE_MATH_OPERATORS)
endif()

#--------------- SDL3 -----------------
CPMAddPackage(
        NAME SDL3
        GITHUB_REPOSITORY "libsdl-org/SDL"
        GIT_TAG release-3.4.0
        OPTIONS
        "SDL_SHARED ON"
        "SDL_STATIC OFF"
        "SDL_TEST_LIBRARY OFF"
        "SDL_TESTS OFF"
        "SDL_DISABLE_INSTALL ON"
        "SDL_DISABLE_INSTALL_DOCS ON"
        "SDL_INSTALL_TESTS OFF"
)

#--------------- GLM -----------------
CPMAddPackage(
        NAME glm
        GITHUB_REPOSITORY "g-truc/glm"
        GIT_TAG 1.0.1
)
add_compile_definitions(GLM_FORCE_DEPTH_ZERO_TO_ONE)

# ----------------- stb_image ---------------------
CPMAddPackage(
        NAME stb_image
        GITHUB_REPOSITORY "nothings/stb"
        GIT_TAG master
)
if(stb_image_ADDED)
    add_library(stb_image INTERFACE IMPORTED)
    target_include_directories(stb_image INTERFACE ${stb_image_SOURCE_DIR})
endif()

# ----------------- nfd ---------------------------
CPMAddPackage(
        NAME nfd
        GITHUB_REPOSITORY "btzy/nativefiledialog-extended"
        GIT_TAG master
        OPTIONS
        "DNFD_BUILD_TESTS OFF"
)

#--------------- ImGui SDL3 backend  -----------------
# because mythril exposes it we dont have to redownload imgui code
# and can just directly include whatever source file we want!
add_library(imgui_sdl3_backend STATIC
        ${MYTHRIL_IMGUI_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
)
target_link_libraries(imgui_sdl3_backend PUBLIC imgui SDL3::SDL3)

# -------------------- executable setup --------------------------
add_executable(${PROJECT_NAME}
        src/main.cpp
)
target_link_libraries(${PROJECT_NAME}
        PUBLIC
        mythril
        ImNodeFlow
        glm
        SDL3::SDL3
        imgui_sdl3_backend
        stb_image
        nfd
)
target_compile_definitions(${PROJECT_NAME} PRIVATE IMGUI_DEFINE_MATH_OPERATORS)