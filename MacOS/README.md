# Building on macOS

In order build Anura, you will need [XCode](https://developer.apple.com/xcode/).

The solution and project wrap vcpkg for you and you should be simply able to
"press play".

An extra build target to refresh the submodules for you exists.

An alternate route to build on the command line with `xcodebuild`. See the
[CI](https://github.com/anura-engine/anura/blob/trunk/.github/workflows/push-unit-tests-static-macos.yaml)
definition for the current details.
