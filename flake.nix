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

                    clang_19
                    llvmPackages_19.clang-tools
                    bear

                    gdb
                    bochs
                    qemu_full
                    (pkgs.writeShellScriptBin "qemu-ovmf-x86-64" ''
                        ${pkgs.qemu_full}/bin/qemu-system-x86_64 \
                            -drive if=pflash,unit=0,format=raw,file=${pkgs.OVMF.fd}/FV/OVMF.fd,readonly=on \
                            "$@"
                    '')
                ];
            };
        }
    );
}
