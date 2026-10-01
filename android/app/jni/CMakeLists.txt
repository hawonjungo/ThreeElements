# Native part of the Android app: SDL2, SDL2_image and the game itself (libmain.so, which SDLActivity loads and
# whose SDL_main() is the game's main()). SDL/ and SDL_image/ are downloaded by android/fetch_deps.sh (not in git).
cmake_minimum_required(VERSION 3.10)
project(ThreeElementsAndroid C CXX)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/SDL/CMakeLists.txt" OR NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/SDL_image/CMakeLists.txt")
    message(FATAL_ERROR "SDL sources missing: run android/fetch_deps.sh first (from Git Bash).")
endif()

add_subdirectory(SDL)

# SDL2_image: PNG (through the bundled stb_image) and BMP are all the game loads; every other format is off so no
# third-party library has to be built.
set(SDL2IMAGE_VENDORED OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_SAMPLES OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_TESTS OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_BACKEND_STB ON CACHE BOOL "" FORCE)
set(SDL2IMAGE_PNG ON CACHE BOOL "" FORCE)
set(SDL2IMAGE_BMP ON CACHE BOOL "" FORCE)
foreach(format AVIF GIF JPG JXL LBM PCX PNM QOI SVG TGA TIF WEBP XCF XPM XV)
    set(SDL2IMAGE_${format} OFF CACHE BOOL "" FORCE)
endforeach()
add_subdirectory(SDL_image)

# The game: the same sources as the desktop (Visual Studio) and web (web/build.sh) builds.
set(GAME_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../Three Elements")
add_library(main SHARED
    "${GAME_DIR}/main.cpp"
    "${GAME_DIR}/GameManager.cpp"
    "${GAME_DIR}/MainPlayer.cpp"
    "${GAME_DIR}/BaseObject.cpp"
    "${GAME_DIR}/Enemy.cpp"
    "${GAME_DIR}/Keyboard.cpp"
    "${GAME_DIR}/Skill.cpp"
    "${GAME_DIR}/PixelText.cpp"
    "${GAME_DIR}/ImpTimer.cpp"
    "${GAME_DIR}/Draw.cpp"
    "${GAME_DIR}/SkillVfx.cpp"
    "${GAME_DIR}/Audio.cpp"
    "${GAME_DIR}/Online.cpp"
    "${GAME_DIR}/TouchLayout.cpp"
    "${GAME_DIR}/Core/Invoker.cpp"
    "${GAME_DIR}/Practice/Practice.cpp"
    "${GAME_DIR}/Practice/Tutorial.cpp"
    "${GAME_DIR}/Practice/Boss.cpp"
    "${GAME_DIR}/Practice/Items.cpp"
)
set_target_properties(main PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
target_include_directories(main PRIVATE "${GAME_DIR}")
target_link_libraries(main PRIVATE SDL2::SDL2 SDL2_image::SDL2_image)
