{
    inputs = {
        nixpkgs.url = "github:NixOS/nixpkgs/nixos-24.11";
        flake-utils.url = "github:numtide/flake-utils";
        chariot.url = "git+https://git.thenest.dev/wux/chariot";
    };

    outputs = { nixpkgs, flake-utils, ... } @ inputs: flake-utils.lib.eachDefaultSystem (system:
        let pkgs = import nixpkgs { inherit system; }; in {
            devShells.default = pkgs.mkShell {
                shellHook = "export DEVSHELL_PS1_PREFIX='elysium-os'";
                nativeBuildInputs = with pkgs; [
                    inputs.chariot.defaultPackage.${system}
                    wget # Required by Chariot

                    gdb
                    qemu_full
                ];
            };
        }
    );
}
