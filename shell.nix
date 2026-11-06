with import <nixpkgs> {};
mkShell {
  # PlatformIO / ESP-IDF python deps have no wheels for the newest python yet,
  # create ~/.platformio/penv with this one: python3.12 -m venv ~/.platformio/penv
  packages = [ python312 ];
  NIX_LD_LIBRARY_PATH = lib.makeLibraryPath [
    stdenv.cc.cc
    libusb1
    zlib
    #openssl
    # ...
  ];
  NIX_LD = lib.fileContents "${stdenv.cc}/nix-support/dynamic-linker";
}
