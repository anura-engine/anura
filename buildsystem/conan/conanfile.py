from conan import ConanFile

class Anura(ConanFile):
    name = "Anura"
    version = "2025-07-27" # Simply put here the date of the last tweak

    settings = [
        "os",
        "arch",
        "compiler",
        "build_type",
    ]

    # Anything we actually link against is explicitly pinned
    requires = [
        "boost/1.88.0",
        "cairo/1.18.0",
        "freetype/2.13.2",
        "glew/2.2.0",
        "sdl_image/2.8.2",
        "sdl_ttf/2.24.0",
        "sdl/2.28.3",
        "vorbis/1.3.7",
    ]

    generators = [
        "CMakeDeps",
        "CMakeToolchain",
    ]

    default_options = {
        # Force static libs by default
        "*:shared": False,
    }
