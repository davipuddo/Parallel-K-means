{
pkgs ? import <nixpkgs> { },
}:
pkgs.mkShell {
    buildInputs = with pkgs; [
        adaptivecppWithRocm
        gcc
        python313
        python313Packages.pillow
        python313Packages.joblib
        python313Packages.numpy
        python313Packages.pandas
        python313Packages.matplotlib
        python313Packages.python-dateutil
        python313Packages.scikit-learn
        python313Packages.scipy
        python313Packages.six
        python313Packages.sklearn-compat
        python313Packages.threadpoolctl
    ];

    shellHook = ''
    '';
}

