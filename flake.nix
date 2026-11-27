{
  description = "Development environment for adaptive GPU points-to analysis";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }:
    let
      supportedSystems = [ "aarch64-darwin" "x86_64-darwin" "x86_64-linux" "aarch64-linux" ];
      forAllSystems = nixpkgs.lib.genAttrs supportedSystems;
    in {
      devShells = forAllSystems (system:
        let pkgs = import nixpkgs { inherit system; };
        in {
          default = pkgs.mkShell {
            packages = with pkgs; [
              bear
              clang
              clang-tools
              cmake
              ninja
            ];

            shellHook = ''
              echo "Adaptive GPU PTA development shell"
              echo "Build: cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build"
            '';
          };
        });
    };
}
