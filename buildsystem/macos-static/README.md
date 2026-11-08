# Building on macOS

This target uses Conan for static dependencies and CMake's Xcode generator. The generated Xcode projects are in `buildsystem/macos-static/build/Debug` and `buildsystem/macos-static/build/Release`.

Install the build tools with Homebrew:

```sh
brew install ccache cmake conan pkg-config
conan profile detect --force
```

Install dependencies for the configuration you want to build, then generate the Xcode project:

```sh
conan install buildsystem/conan --lockfile=buildsystem/conan.lock --lockfile-partial -s build_type=Debug -s compiler.cppstd=17 --build=missing
cmake buildsystem/macos-static --preset Debug
open buildsystem/macos-static/build/Debug/anura.xcodeproj
```

Use `Release` instead of `Debug` to generate the Release project. Run unit tests with:

```sh
ctest --output-on-failure --test-dir buildsystem/macos-static/build/Debug --build-config Debug -L unittest
```
