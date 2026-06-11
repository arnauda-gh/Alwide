{ pkgs ? import <nixpkgs> {} }:

let
  # Use the flake's devShell if possible
  lock = builtins.fromJSON (builtins.readFile ./flake.lock);
  nixpkgs = fetchTarball {
    url = "https://github.com/NixOS/nixpkgs/archive/${lock.nodes.nixpkgs.locked.rev}.tar.gz";
    sha256 = lock.nodes.nixpkgs.locked.narHash;
  };
  # If you have nix-command and flakes enabled, you can just run `nix develop`
  # This shell.nix is a fallback for legacy nix-shell
in
pkgs.mkShell {
  nativeBuildInputs = [
    pkgs.pkg-config
    pkgs.clang
    pkgs.gnumake
    pkgs.rustup
    pkgs.tree-sitter
  ];

  buildInputs = [
    pkgs.ncurses
  ];

  shellHook = ''
    export CC=clang
    export ALWIDE_ASSETS_PATH=$(pwd)/assets
  '';
}
