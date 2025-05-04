{
    inputs = {
        nixpkgs.url = "github:NixOS/nixpkgs/nixos-24.11";
        flake-utils.url = "github:numtide/flake-utils";

        fabricate.url = "github:elysium-os/fabricate";
        fabricate.inputs.nixpkgs.follows = "nixpkgs";
    };

    outputs = { nixpkgs, flake-utils, ... } @ inputs: flake-utils.lib.eachDefaultSystem (system:
        let pkgs = import nixpkgs { inherit system; }; in {
            devShells.default = pkgs.mkShell {
                shellHook = "export NIX_SHELL_NAME='cronus'";
                nativeBuildInputs = with pkgs; [
                    inputs.fabricate.defaultPackage.${system}

                    ninja
                    llvmPackages_19.clangUseLLVM
                    llvmPackages_19.clang-tools
                    llvmPackages_19.bintools
                    nasm
                ];
            };
        }
    );
}
