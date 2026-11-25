{
pkgs ? import <nixpkgs> { },
}:
pkgs.mkShell {
    buildInputs = with pkgs; [
        adaptivecppWithRocm
        gcc
    ];

    shellHook = ''
    '';
}

