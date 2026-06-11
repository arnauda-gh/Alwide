{
  description = "Alwide - A modern terminal-based code editor";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
        
        commonInputs = {
          nativeBuildInputs = [
            pkgs.pkg-config
            pkgs.clang
            pkgs.gnumake
            pkgs.rustc
            pkgs.cargo
            pkgs.rustPlatform.cargoSetupHook
            pkgs.tree-sitter
          ];
          buildInputs = [
            pkgs.ncurses
          ];
        };
      in
      {
        packages.default = pkgs.stdenv.mkDerivation {
          pname = "alwide";
          version = "0.1.0";

          src = ./.;

          nativeBuildInputs = commonInputs.nativeBuildInputs;
          buildInputs = commonInputs.buildInputs;

          buildPhase = ''
            export HOME=$TMPDIR
            make release
          '';

          installPhase = ''
            make install PREFIX=$out
          '';
        };

        devShells.default = pkgs.mkShell {
          name = "alwide-dev";
          
          nativeBuildInputs = commonInputs.nativeBuildInputs ++ [ pkgs.rustup ];
          buildInputs = commonInputs.buildInputs;

          shellHook = ''
            export CC=clang
            # For development, we might want to point to the local assets
            export ALWIDE_ASSETS_PATH=$(pwd)/assets
            echo "Welcome to Alwide development environment!"
          '';
        };
      }
    );
}
