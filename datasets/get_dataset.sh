# Rebuilds every dataset from scratch. Produces, in this directory:
#
#   treino_sempayload.csv    10M-row feature CSVs from the CAN captures
#   teste_sempayload.csv
#   treino_balanced_big.csv  80M-row benchmark set that src/seq.c reads
#
# Budget ~30 min and ~20 GB free: the clone, the intermediate ctt/ CSVs, and
# the 3.9 GB benchmark file.

set -euo pipefail
cd "$(dirname "$(readlink -f "$0")")"

PY=$(command -v python3 || command -v python) || {
    echo "error: no python3/python on PATH (try running inside nix-shell)" >&2
    exit 1
}

# 1. Raw CAN captures.
if [ -d can-train-and-test ]; then
    echo "==> can-train-and-test already cloned, skipping"
else
    echo "==> cloning can-train-and-test"
    git clone https://bitbucket.org/brooke-lampe/can-train-and-test.git
fi
cp -r can-train-and-test/* .

# 2. Deduplicate the captures, split them into train/test symlink trees.
echo "==> montar_mesclado_ctt.py"
"$PY" montar_mesclado_ctt.py

# 3. Raw CAN frames -> the 6-column feature CSVs, into ctt/mesclado.
echo "==> gerar_ctt.py"
"$PY" gerar_ctt.py

# 4. Random undersampling down to 10M rows each, into ctt/mesclado_rus_10m.
echo "==> rus_ctt.py"
"$PY" rus_ctt.py ctt/mesclado ctt/mesclado_rus_10m 0 10000000

# 5. Promote the 10M sets to this directory, where the C code looks for them.
echo "==> installing the 10M sets"
cp ctt/mesclado_rus_10m/treino_sempayload.csv .
cp ctt/mesclado_rus_10m/teste_sempayload.csv .

# 6. Balance the classes, z-score the features, and inflate to 80M rows, so the
#    sequential k-means runs ~15 s instead of ~1.7 s and its accuracy is not
#    just the class prior. Takes ~5 min and writes 3.9 GB. Run it again with
#    --input teste_sempayload.csv --output teste_balanced_big.csv for a matching
#    test set (see the note in that script about reusing the training stats).
echo "==> make_benchmark_dataset.py"
"$PY" make_benchmark_dataset.py

echo
echo "Done. src/seq.c reads treino_balanced_big.csv by default:"
echo "    cd ../src && make seq && ./seq"
