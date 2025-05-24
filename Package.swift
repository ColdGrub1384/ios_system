// swift-tools-version:5.5
import PackageDescription

let remotePackageSite = "https://gatites.no.binarios.cl/git/api/packages/pyto/generic/ios_system/v3.0.0/"

// -- begin checksums --
let checksums = [
  "apple-universal-awk.xcframework.zip": "0386d314f18206378956f102f6b55cab8db68959e5232ddd752528ac87b8cbca",
  "apple-universal-curl_ios.xcframework.zip": "73bdffc7d56bb175010f135b235eab8e10a80d6c75cd5637e74ba43703a56778",
  "apple-universal-files.xcframework.zip": "e375292c0bccfbc87ee76d2ed8b284b38aece68a2b91ce8eec83d39508b7ea44",
  "apple-universal-ios_system.xcframework.zip": "4815e66a062c490985c1755848205015d99ec6c85c95e33cf6d23627403c0f10",
  "apple-universal-shell.xcframework.zip": "a54eea6631f9c98f959dd3b585ebc5fe0936631b85c5b6341e9bef3b0ee2c278",
  "apple-universal-tar.xcframework.zip": "217a0afe9bfae7c06bae8b81a4ad06dc69820bef7a2e80a63b684c806e7555b1",
  "apple-universal-text.xcframework.zip": "09b1cae69372d1cc3ebded7e82985a657b3b1889b9e0d597e35d1e52801d1c70",
]
// -- end checksums --

let projectRoot = URL(filePath: #file).deletingLastPathComponent()
let local = (try? FileManager.default.contentsOfDirectory(at: projectRoot, includingPropertiesForKeys: nil))?.contains(where: { $0.path.hasSuffix(".xcframework") }) == true

func binaryTarget(name: String) -> Target {
    if local {
        .binaryTarget(
            name: name,
            path: "apple-universal-\(name).xcframework.zip"
        )
    } else {
        .binaryTarget(
            name: name,
            url: "\(remotePackageSite)/apple-universal-\(name).xcframework.zip",
            checksum: checksums["apple-universal-\(name).xcframework.zip"]!
        )
    }
}

let package = Package(
    name: "ios_system",
    platforms: [
        .macCatalyst(.v14), .iOS(.v14), .tvOS(.v14), .watchOS(.v6)
    ],
    products: [
        // Products define the executables and libraries a package produces, and make them visible to other packages.
        .library(
            name: "ios_system",
            targets: ["ios_system"])
    ],
    dependencies: [],
    targets: [
        "awk",
        "curl_ios",
        "files",
        "ios_system",
        "shell",
        "tar",
        "text"
    ].map({ binaryTarget(name: $0) })
)
