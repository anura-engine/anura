# Anura Engine

![Anura
Logo](https://raw.github.com/anura-engine/anura/master/utils/Logo%20Images/Anura%20Logo.png)

Anura is the tech behind the spectacular [Frogatto &
Friends](https://github.com/frogatto/frogatto/wiki). It is a fully-featured game
engine, free for commercial and non-commercial use.

To compile Anura for [Argentum Age](https://github.com/davewx7/citadel)
(formerly Citadel), use the `argentum-age` branch.

At the time of writing (2023-08), Anura is only committed to supporting
explicitly one module / game: [Frogatto](https://github.com/frogatto/frogatto).
For anything else (like Argentum Age above) you are on your own.

## Build Systems

Anura is a multi platform C++ engine rolling its own functional scripting
language (FFL) to build games.

While there have been multiple native mobile, console and other embedded
platforms supported in the past, at the time of writing (2023-08) only three
major desktop operating systems are supported.

* Windows
* macOS
* Linux

While Anura might build and run on other operating systems, on those you will be
on your own. It is not completely out of the question to eventually widen the
set of platforms targeted, but currently (2023-08) the main focus is to clean
Anura up and to ensure its compilability for years (and compilers and language
standards and library upgrades) to come.

### Language Standard

Anura is built using the ISO C++ 17 standard on all platforms without any
language specification extensions.

### Known Dependencies

Anura is not fully known or understood at this point in time, but we know for
certain some of parts of it are actually dead (not used anymore) or unused
(never used, at least not by Frogatto). The effort to reverse engineer and
identify and prune the truly dead parts off Anura off is currently (2023-08)
starting.

For the majority of these we do not, at the time of writing (2023-08), know the
upper or lower bounds of what the engine actually happily builds, links and runs
against.

This list represents our current (2023-08) understanding:

* [UNIX/POSIX Threads (pthread)](https://en.wikipedia.org/wiki/Pthreads)
* [UNIX/POSIX Realtime Extensions (librt)](https://unix.org/version2/whatsnew/realtime.html)
* [Boost](https://www.boost.org/)
  * [Boost Filesystem](https://www.boost.org/doc/libs/1_82_0/libs/filesystem/doc/index.htm)
  * [Boost Regex](https://www.boost.org/doc/libs/1_82_0/libs/regex/doc/html/index.html)
  * [Boost Locale](https://www.boost.org/doc/libs/1_82_0/libs/locale/doc/html/index.html)
* [zlib](https://www.zlib.net/)
* [OpenGL](https://www.opengl.org/sdk/)
  * [GLEW](https://www.opengl.org/sdk/libs/GLEW/)
  * [GLM](https://github.com/g-truc/glm)
* [Ogg](https://xiph.org/ogg/)
* [Vorbis](https://xiph.org/vorbis/)
* [Freetype 2](https://freetype.org/)
* [SDL 2](https://wiki.libsdl.org/SDL2/FrontPage)
  * [SDL2 Image](https://wiki.libsdl.org/SDL2_image/FrontPage)
  * [SDL2 Mixer](https://wiki.libsdl.org/SDL2_mixer/FrontPage)
  * [SDL2 TTF](https://wiki.libsdl.org/SDL2_ttf/FrontPage)
* [Dear ImGui](https://github.com/ocornut/imgui)
* [Cairo](https://www.cairographics.org/)

We know the above is not an exhaustive list: there are other ones too, but
pulling at least these in results in a build, which builds, links and runs. The
investigation continues (2023-08) to find and minimize the true full set.

### Types of Builds

All builds of Anura are 64bit only and come in two flavors: Debug and Release
builds.

The default build type on every platform is Debug.

Additionally on some platforms it is possible to also build the engine
dynamically linked against the operating system provided libraries. The default
build type on every platform is to use vcpkg to pull the dependencies in and to
statically link those in into the resulting binary for the ease of distribution.

Anura is an old clunky beast with over a decade of history. It's currently
(2023-08) spitting out a lot of compiler warnings on most platforms we
eventually aim to peel off one by one so that we would feel comfortable going
for applying static analysis tooling and also eventually driving those results
down to zero.

Aspirational concept level build type descriptions are provided below. The finer
details will vary on a per platform basis as the toolchains and the exact
command line flags they use can be very different across ecosystems.

Reference material:

* [GCC Option Summary](https://gcc.gnu.org/onlinedocs/gcc/Option-Summary.html)
* [Clang Command Line Argument
  Reference](https://clang.llvm.org/docs/ClangCommandLineReference.html)
* [C++ Asserts](https://en.cppreference.com/w/cpp/error/assert)
* [C++ Standard Library](https://en.cppreference.com/w/cpp/standard_library)


#### Debug

As a rule of thumb the Debug builds add the appropriate per platform flags to
enable the debuggability of the binary with reasonable performance.

Compiler flags:

* `-g`
* `-Og`

#### Release

Release builds add optimization flags for the best level of performance, which
is still language specification compliant.

The release builds also turn all debug features (like Asserts!) off.

Precompiler flags:

* `NDEBUG`

Compiler flags:

* `-O3`

### Dynamic Builds

We can only feasibly build, link and run against operating system provided
libraries on various UNIXen, out of which we currently only support Linux.

#### Linux

Currently (2023-08) we use an organically grown poorly understood legacy
`Makefile` to build the project on Linux operating systems.

We officially track the buildability on four major distributions with two
compilers: g++ and clang++.

On any rolling release distributions we assume you know what you are doing and
you are on your own.

There is a loose plan to have an Anura package available again in Debian in time
for the Debian Trixie release expected in early Summer 2025.

##### Debian

Check the
[CI](https://github.com/anura-engine/anura/blob/trunk/.github/workflows/pr-smoketest-debian.yaml)
out and repeat after it what it is doing.

##### Ubuntu

Check the
[CI](https://github.com/anura-engine/anura/blob/trunk/.github/workflows/pr-smoketest-ubuntu.yaml)
out and repeat after it what it is doing.

#### Fedora

Check the
[CI](https://github.com/anura-engine/anura/blob/trunk/.github/workflows/pr-smoketest-fedora.yaml)
out and repeat after it what it is doing.

#### openSUSE Leap

Check the
[CI](https://github.com/anura-engine/anura/blob/trunk/.github/workflows/pr-smoketest-opensuse-leap.yaml)
out and repeat after it what it is doing.

### Static Builds

On most platforms we must bundle our libraries in with the binary as that is the
way of the land.

#### Windows

Undocumented as of 2023-08.

#### macOS

Undocumented, known broken as of 2023-08.

#### macOS Legacy

Undocumented, known broken as of 2023-08.

#### Linux

Unimplemented as of 2023-08.

##### Steam Linux Runtime

Unimplemented as of 2023-08.

A loose plan to target the
[Sniper](https://gitlab.steamos.cloud/steamrt/steam-runtime-tools/-/blob/main/docs/container-runtime.md#steam-runtime-3-sniper)
runtime exists.

## CI

![Unit Tests / Required / Dynamic /
Linux](https://github.com/anura-engine/anura/actions/workflows/push-unit-tests-dynamic-linux.yaml/badge.svg?branch=trunk)

### Branch Protection Rules / Flow of Incoming Changes

All incoming changes must:

* Start off on an unmerged branch
* Come through a Pull Request
* Have all the Hard Quality Gates green

We recommend to use [Atomic
Commits](https://www.pauline-vos.nl/git-legit-cheatsheet/) and minimal topical
Pull Requests where ever possible. These help you teach us about your change and
do not leave you as the only person in the universe understanding how your
change works. In a Pull Request event the one making the Pull Request is the
teacher and the reviewer is the student. The end result should be akin to the
student passing the test with flying colors.

### Soft Quality Gates

Soft Quality Gates are a FYI level informative peek into where Anura can
currently build out of the box, but are not of major concern and quite often the
correct course of action is to drop the build in question and amend the
documentation to match.

On Pull Requests and Merge Queue events:

* Smoketest dynamic builds on Linux (both g++ and clang++)
  * Debian
    * 11 / Bullseye
    * 12 / Bookworm
    * 13 / Trixie
  * Ubuntu
    * 22.04 / Jammy Jellyfish
  * Fedora
    * 32
    * 33
    * 34
    * 35
    * 36
    * 37
    * 38
    * 39
  * openSUSE Leap
    * 15.3
    * 15.4
    * 15.5

This set should cover most popular use cases and also derivative distributions
of these root distributions. There are no plans to test on rolling release
distributions: on those you are on your own and we welcome the heads up if
something breaks for you and can take a look, but will not guarantee a rapid
fix. Pull requests to add more known-good targets to the set of are most
welcome, but zero effort will be spent on our side to turn a build like that
from red to green - it has to be green when coming in.

### Hard Quality Gates

Hard Quality Gates are our non-negotiable baseline of quality expectations on
incoming changes. Not green -> not getting merged.

On every push:
* NOT IMPLEMENTED Linters
  * NOT IMPLEMENTED [markdownlint](https://github.com/DavidAnson/markdownlint)
  * NOT IMPLEMENTED [ClangFormat](https://clang.llvm.org/docs/ClangFormat.html)
  * NOT IMPLEMENTED [JSON
    Formatter](https://github.com/callumlocke/json-formatter)
* Unit Tests
  * NOT IMPLEMENTED Windows / msbuild / Release / static
  * NOT IMPLEMENTED Windows / msbuild / Debug / static
  * NOT IMPLEMENTED macOS / xcodebuild / Release / static
  * NOT IMPLEMENTED macOS / xcodebuild / Debug / static
  * Ubuntu 22.04 / clang++ / Release / dynamic
  * NOT IMPLEMENTED Ubuntu 22.04 / clang++ / Debug / dynamic
  * NOT IMPLEMENTED Ubuntu 22.04 / clang++ / Release / static
  * NOT IMPLEMENTED Steam Linux Runtime / clang++ / Release / static
* NOT IMPLEMENTED Integration Tests
  * Rough sketch to have a module, which boots, runs a short cutscene and quits
    * Anura runs just fine in a virtual framebuffer with `xvfb-run`
* NOT IMPLEMENTED [LLVM
  scan-build](https://clang-analyzer.llvm.org/scan-build.html)

## CD

All publishing will happen from the module side, most notably from
[Frogatto](https://github.com/frogatto/frogatto).
