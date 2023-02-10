with import <nixpkgs> {};
mkShell {
  NIX_LD_LIBRARY_PATH = lib.makeLibraryPath [
    stdenv.cc.cc
    libusb1
    zlib
    #openssl
    # ...
  ];
  NIX_LD = lib.fileContents "${stdenv.cc}/nix-support/dynamic-linker";
}
