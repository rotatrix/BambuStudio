# Match the application's original macOS deployment targets, not the host's
# Homebrew bottle minimum. Boost iostreams uses zstd::libzstd_shared.
set(_zstd_macos_target 10.15)
if(CMAKE_OSX_ARCHITECTURES MATCHES "arm64")
    set(_zstd_macos_target 11.3)
endif()
bambustudio_add_cmake_project(Zstd
    URL https://github.com/facebook/zstd/releases/download/v1.5.7/zstd-1.5.7.tar.gz
    URL_HASH SHA256=eb33e51f49a15e023950cd7825ca74a4a2b43db8354825ac24fc1b7ee09e6fa3
    SOURCE_SUBDIR build/cmake
    CMAKE_ARGS
        -DCMAKE_OSX_ARCHITECTURES:STRING=${CMAKE_OSX_ARCHITECTURES}
        -DCMAKE_OSX_DEPLOYMENT_TARGET=${_zstd_macos_target}
        -DZSTD_BUILD_SHARED=ON
        -DZSTD_BUILD_STATIC=OFF
        -DZSTD_BUILD_PROGRAMS=OFF
        -DZSTD_BUILD_TESTS=OFF
)
ExternalProject_Add_Step(dep_Zstd license
    DEPENDEES install
    COMMAND ${CMAKE_COMMAND} -E make_directory ${DESTDIR}/usr/local/share/licenses/zstd
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        <SOURCE_DIR>/LICENSE ${DESTDIR}/usr/local/share/licenses/zstd/LICENSE
)
