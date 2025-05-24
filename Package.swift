// swift-tools-version:5.5
import PackageDescription

let remotePackageSite = "https://gatites.no.binarios.cl/git/api/packages/pyto/generic/ios_system/v3.0.0/"

// -- begin checksums --
let checksums = [
  "apple-universal-awk.xcframework.zip": "1511f081132ad7990802449ee9fe354c8b0b7936d4e03bd27e1476fc9d478dbd",
  "apple-universal-curl_ios.xcframework.zip": "d828ba2509e69be382635144af0e134ef0d70c276d0d92b8473398fec4f66591",
  "apple-universal-files.xcframework.zip": "2f748037ccdfa3839c185ef832059551bbe061ebfffb58f2a47ded807193b7af",
  "apple-universal-ios_system.xcframework.zip": "3f14a39f20dc186a0070c5675f666f9efeedfc6a82329737fdcc78651846fbed",
  "apple-universal-network_ios.xcframework.zip": "97dd98f4434135349f3d22562aec5f081f29556a93dc1a88f684b2909ec4ec29",
  "apple-universal-shell.xcframework.zip": "31aae801363056baeef6682692d3928360047aa87662e78aca7a71322d4c2cbc",
  "apple-universal-tar.xcframework.zip": "7c84a4edef5aee0db63e40a61d04010f1510ca41c415391916f9d0cb2e34686c",
  "apple-universal-text.xcframework.zip": "177eb39d81da0579b5f93467ba7caddf4866bc0dbf573e191145c5aad4610923",
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
        "text",
        "network_ios"
    ].map({ binaryTarget(name: $0) })
)
