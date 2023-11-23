# Anura Engine

![](https://raw.github.com/anura-engine/anura/master/utils/Logo%20Images/Anura%20Logo.png)

Anura is the tech behind the spectacular [Frogatto & Friends](https://github.com/frogatto/frogatto/wiki). It is a fully-featured game engine, free for commercial and non-commercial use.

Note: For historic reasons – unlike most projects – Anura's main development branch is `trunk` as opposed to `master`. For example, to clone the repository, you might run `git clone --branch trunk git@github.com:anura-engine/anura.git`.

To compile Anura for [Argentum Age](https://github.com/davewx7/citadel) (formerly Citadel), use the `argentum-age` branch. There are some issues on `trunk` which prevent AA from running there at the moment.

## CI

![Unit Tests / Required / Dynamic / Linux](https://github.com/anura-engine/anura/actions/workflows/push-unit-tests-dynamic-linux.yaml/badge.svg?branch=trunk)

At the time of writing (2023-07) the CI setup tests the following permutations:

On every push:

* Unit Tests
  * Ubuntu 22.04 / clang
  * NOT IMPLEMENTED Windows
  * NOT IMPLEMENTED macOS

On Pull Requests and Merge Queue events:

* Smoketest dynamic builds on Linux (both g++ and clang)
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
fix. Pull requests welcome.

Smoketest static builds on Linux:

* NOT IMPLEMENTED Steam

All publishing builds will happen from the module side, most notably from
[Frogatto](https://github.com/frogatto/frogatto).
