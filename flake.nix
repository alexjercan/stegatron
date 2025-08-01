{
  description = "A basic flake for my C Projects";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = {
    self,
    nixpkgs,
    flake-utils,
  }: (
    flake-utils.lib.eachDefaultSystem
    (system: let
      pkgs = import nixpkgs {
        inherit system;

        config = {
          allowUnfree = true;
        };
      };
    in {
      packages = {
          default = {}; # TODO: Add package here
      };

      apps = {
        # TODO: Add app here
        # default = flake-utils.lib.mkApp {
        #   drv = ...;
        # };
      };

      devShells.default = pkgs.mkShell {
        nativeBuildInputs = with pkgs; [
          clang
          valgrind
        ];

        buildInputs = [];
      };
    })
  );
}
