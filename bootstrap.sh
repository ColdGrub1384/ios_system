#!/bin/bash

if [ -z "$LIBSSH2_XCFRAMEWORK_PATH" ] || [ -z "$OPENSSL_XCFRAMEWORK_PATH" ]; then
    echo "Set LIBSSH2_XCFRAMEWORK_PATH and OPENSSL_XCFRAMEWORK_PATH environment variables to compile ios_system"
    exit 1
fi

cp -R "$LIBSSH2_XCFRAMEWORK_PATH" "ssh2.xcframework"
cp -R "$OPENSSL_XCFRAMEWORK_PATH" "openssl.xcframework"

BUILD() {
    if [[ "$2" = "maccatalyst" ]]; then
        sdk="macosx"
        ADDITIONAL_FLAGS="-destination 'platform=macOS,variant=Mac Catalyst,arch=$3' SUPPORTS_MAC_CATALYST=YES ARCHS=$3"
    else
        sdk="$2"
        ADDITIONAL_FLAGS="-arch $3"
    fi
    eval xcodebuild -project "ios_system.xcodeproj" -scheme $1 -sdk $sdk -configuration Release SYMROOT="build_$2.$3" "$ADDITIONAL_FLAGS"
}

MAKE_FAT_FRAMEWORK() {
    build_dir="build_$2"
    mkdir -p "$build_dir"
    
    if [[ "$2" = "maccatalyst" ]]; then
        binary_path="$build_dir/$1.framework/Versions/Current/$1"
    else
        binary_path="$build_dir/$1.framework/$1"
    fi
    
    binaries=""
    for arch in $3; do
        binaries="$binaries '$build_dir.$arch/$1.framework/$1'"
    done
    
    for arch in $3; do
        cp -R "$build_dir.$arch/$1.framework" "$build_dir/"
        break
    done
    
    rm "$binary_path"
    
    eval lipo -create $binaries -output "$binary_path"
}

rm -rf "ios_system.xcframework" "shell.xcframework" "tar.xcframework" "text.xcframework" "files.xcframework" "awk.xcframework" "curl_ios.xcframework"

## ios_system ##

BUILD ios_system iphoneos         arm64
BUILD ios_system iphonesimulator  arm64
BUILD ios_system iphonesimulator  x86_64

BUILD ios_system maccatalyst      arm64
BUILD ios_system maccatalyst      x86_64

BUILD ios_system watchos          armv7k
BUILD ios_system watchos          arm64_32
BUILD ios_system watchos          arm64
BUILD ios_system watchsimulator   arm64
BUILD ios_system watchsimulator   x86_64

BUILD ios_system appletvos        arm64
BUILD ios_system appletvsimulator arm64
BUILD ios_system appletvsimulator x86_64

## shell ##

BUILD shell iphoneos         arm64
BUILD shell iphonesimulator  arm64
BUILD shell iphonesimulator  x86_64

BUILD shell maccatalyst      arm64
BUILD shell maccatalyst      x86_64

BUILD shell watchos          armv7k
BUILD shell watchos          arm64_32
BUILD shell watchos          arm64
BUILD shell watchsimulator   arm64
BUILD shell watchsimulator   x86_64

BUILD shell appletvos        arm64
BUILD shell appletvsimulator arm64
BUILD shell appletvsimulator x86_64

## tar ##

BUILD tar iphoneos         arm64
BUILD tar iphonesimulator  arm64
BUILD tar iphonesimulator  x86_64

BUILD tar maccatalyst      arm64
BUILD tar maccatalyst      x86_64

BUILD tar watchos          armv7k
BUILD tar watchos          arm64_32
BUILD tar watchos          arm64
BUILD tar watchsimulator   arm64
BUILD tar watchsimulator   x86_64

BUILD tar appletvos        arm64
BUILD tar appletvsimulator arm64
BUILD tar appletvsimulator x86_64

## text ##

BUILD text iphoneos         arm64
BUILD text iphonesimulator  arm64
BUILD text iphonesimulator  x86_64

BUILD text maccatalyst      arm64
BUILD text maccatalyst      x86_64

BUILD text watchos          armv7k
BUILD text watchos          arm64_32
BUILD text watchos          arm64
BUILD text watchsimulator   arm64
BUILD text watchsimulator   x86_64

BUILD text appletvos        arm64
BUILD text appletvsimulator arm64
BUILD text appletvsimulator x86_64


## files ##

BUILD files iphoneos         arm64
BUILD files iphonesimulator  arm64
BUILD files iphonesimulator  x86_64

BUILD files maccatalyst      arm64
BUILD files maccatalyst      x86_64

BUILD files watchos          armv7k
BUILD files watchos          arm64_32
BUILD files watchos          arm64
BUILD files watchsimulator   arm64
BUILD files watchsimulator   x86_64

BUILD files appletvos        arm64
BUILD files appletvsimulator arm64
BUILD files appletvsimulator x86_64

## awk ##

BUILD awk iphoneos         arm64
BUILD awk iphonesimulator  arm64
BUILD awk iphonesimulator  x86_64

BUILD awk maccatalyst      arm64
BUILD awk maccatalyst      x86_64

BUILD awk watchos          armv7k
BUILD awk watchos          arm64_32
BUILD awk watchos          arm64
BUILD awk watchsimulator   arm64
BUILD awk watchsimulator   x86_64

BUILD awk appletvos        arm64
BUILD awk appletvsimulator arm64
BUILD awk appletvsimulator x86_64

## curl_ios ##

BUILD curl_ios iphoneos         arm64
BUILD curl_ios iphonesimulator  arm64
BUILD curl_ios iphonesimulator  x86_64

BUILD curl_ios maccatalyst      arm64
BUILD curl_ios maccatalyst      x86_64

BUILD curl_ios watchos          armv7k
BUILD curl_ios watchos          arm64_32
BUILD curl_ios watchos          arm64
BUILD curl_ios watchsimulator   arm64
BUILD curl_ios watchsimulator   x86_64

BUILD curl_ios appletvos        arm64
BUILD curl_ios appletvsimulator arm64
BUILD curl_ios appletvsimulator x86_64

for build_folder in build_*/Release-*; do
    containing_folder="$(dirname $build_folder)"
    mv $build_folder/* "$containing_folder/"
    rm -rf "$build_folder"
done

## Cleanup ##

rm -rf "openssl.xcframework" "ssh2.xcframework"

## Make Fat Frameworks ##

MAKE_FAT_FRAMEWORK "ios_system" "maccatalyst" "arm64 x86_64"
MAKE_FAT_FRAMEWORK "shell"      "maccatalyst" "arm64 x86_64"
MAKE_FAT_FRAMEWORK "tar"        "maccatalyst" "arm64 x86_64"
MAKE_FAT_FRAMEWORK "text"       "maccatalyst" "arm64 x86_64"
MAKE_FAT_FRAMEWORK "files"      "maccatalyst" "arm64 x86_64"
MAKE_FAT_FRAMEWORK "awk"        "maccatalyst" "arm64 x86_64"
MAKE_FAT_FRAMEWORK "curl_ios"   "maccatalyst" "arm64 x86_64"

MAKE_FAT_FRAMEWORK "ios_system" "iphonesimulator" "arm64 x86_64"
MAKE_FAT_FRAMEWORK "shell"      "iphonesimulator" "arm64 x86_64"
MAKE_FAT_FRAMEWORK "tar"        "iphonesimulator" "arm64 x86_64"
MAKE_FAT_FRAMEWORK "text"       "iphonesimulator" "arm64 x86_64"
MAKE_FAT_FRAMEWORK "files"      "iphonesimulator" "arm64 x86_64"
MAKE_FAT_FRAMEWORK "awk"        "iphonesimulator" "arm64 x86_64"
MAKE_FAT_FRAMEWORK "curl_ios"   "iphonesimulator" "arm64 x86_64"

MAKE_FAT_FRAMEWORK "ios_system" "watchos" "armv7k arm64_32 arm64"
MAKE_FAT_FRAMEWORK "shell"      "watchos" "armv7k arm64_32 arm64"
MAKE_FAT_FRAMEWORK "tar"        "watchos" "armv7k arm64_32 arm64"
MAKE_FAT_FRAMEWORK "text"       "watchos" "armv7k arm64_32 arm64"
MAKE_FAT_FRAMEWORK "files"      "watchos" "armv7k arm64_32 arm64"
MAKE_FAT_FRAMEWORK "awk"        "watchos" "armv7k arm64_32 arm64"
MAKE_FAT_FRAMEWORK "curl_ios"   "watchos" "armv7k arm64_32 arm64"

MAKE_FAT_FRAMEWORK "ios_system" "watchsimulator" "arm64 x86_64"
MAKE_FAT_FRAMEWORK "shell"      "watchsimulator" "arm64 x86_64"
MAKE_FAT_FRAMEWORK "tar"        "watchsimulator" "arm64 x86_64"
MAKE_FAT_FRAMEWORK "text"       "watchsimulator" "arm64 x86_64"
MAKE_FAT_FRAMEWORK "files"      "watchsimulator" "arm64 x86_64"
MAKE_FAT_FRAMEWORK "awk"        "watchsimulator" "arm64 x86_64"
MAKE_FAT_FRAMEWORK "curl_ios"   "watchsimulator" "arm64 x86_64"

MAKE_FAT_FRAMEWORK "ios_system" "appletvsimulator" "arm64 x86_64"
MAKE_FAT_FRAMEWORK "shell"      "appletvsimulator" "arm64 x86_64"
MAKE_FAT_FRAMEWORK "tar"        "appletvsimulator" "arm64 x86_64"
MAKE_FAT_FRAMEWORK "text"       "appletvsimulator" "arm64 x86_64"
MAKE_FAT_FRAMEWORK "files"      "appletvsimulator" "arm64 x86_64"
MAKE_FAT_FRAMEWORK "awk"        "appletvsimulator" "arm64 x86_64"
MAKE_FAT_FRAMEWORK "curl_ios"   "appletvsimulator" "arm64 x86_64"

## Make XCFrameworks ##

xcodebuild -create-xcframework \
    -framework "build_iphoneos.arm64/ios_system.framework" \
    -framework "build_iphonesimulator/ios_system.framework" \
    -framework "build_maccatalyst/ios_system.framework" \
    -framework "build_watchos/ios_system.framework" \
    -framework "build_watchsimulator/ios_system.framework" \
    -framework "build_appletvos.arm64/ios_system.framework" \
    -framework "build_appletvsimulator/ios_system.framework" \
    -output    "ios_system.xcframework"

xcodebuild -create-xcframework \
    -framework "build_iphoneos.arm64/shell.framework" \
    -framework "build_iphonesimulator/shell.framework" \
    -framework "build_maccatalyst/shell.framework" \
    -framework "build_watchos/shell.framework" \
    -framework "build_watchsimulator/shell.framework" \
    -framework "build_appletvos.arm64/shell.framework" \
    -framework "build_appletvsimulator/shell.framework" \
    -output    "shell.xcframework"

xcodebuild -create-xcframework \
    -framework "build_iphoneos.arm64/tar.framework" \
    -framework "build_iphonesimulator/tar.framework" \
    -framework "build_maccatalyst/tar.framework" \
    -framework "build_watchos/tar.framework" \
    -framework "build_watchsimulator/tar.framework" \
    -framework "build_appletvos.arm64/tar.framework" \
    -framework "build_appletvsimulator/tar.framework" \
    -output    "tar.xcframework"

xcodebuild -create-xcframework \
    -framework "build_iphoneos.arm64/text.framework" \
    -framework "build_iphonesimulator/text.framework" \
    -framework "build_maccatalyst/text.framework" \
    -framework "build_watchos/text.framework" \
    -framework "build_watchsimulator/text.framework" \
    -framework "build_appletvos.arm64/text.framework" \
    -framework "build_appletvsimulator/text.framework" \
    -output    "text.xcframework"

xcodebuild -create-xcframework \
    -framework "build_iphoneos.arm64/files.framework" \
    -framework "build_iphonesimulator/files.framework" \
    -framework "build_maccatalyst/files.framework" \
    -framework "build_watchos/files.framework" \
    -framework "build_watchsimulator/files.framework" \
    -framework "build_appletvos.arm64/files.framework" \
    -framework "build_appletvsimulator/files.framework" \
    -output    "files.xcframework"

xcodebuild -create-xcframework \
    -framework "build_iphoneos.arm64/awk.framework" \
    -framework "build_iphonesimulator/awk.framework" \
    -framework "build_maccatalyst/awk.framework" \
    -framework "build_watchos/awk.framework" \
    -framework "build_watchsimulator/awk.framework" \
    -framework "build_appletvos.arm64/awk.framework" \
    -framework "build_appletvsimulator/awk.framework" \
    -output    "awk.xcframework"

xcodebuild -create-xcframework \
    -framework "build_iphoneos.arm64/curl_ios.framework" \
    -framework "build_iphonesimulator/curl_ios.framework" \
    -framework "build_maccatalyst/curl_ios.framework" \
    -framework "build_watchos/curl_ios.framework" \
    -framework "build_watchsimulator/curl_ios.framework" \
    -framework "build_appletvos.arm64/curl_ios.framework" \
    -framework "build_appletvsimulator/curl_ios.framework" \
    -output    "curl_ios.xcframework"
