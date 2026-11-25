================================================================================
K-MEANS PARALELO - INSTRUÇÕES DE COMPILAÇÃO E EXECUÇÃO
================================================================================

Este projeto contém três implementações do algoritmo k-means, usadas para
comparar o desempenho sequencial com o paralelo:

    src/seq.c    versão sequencial
    src/cpu.c    versão paralela em CPU, com OpenMP
    src/gpu.cc   versão paralela em GPU, com SYCL (memória USM, sem buffers)

As três leem o mesmo CSV, executam 30 iterações com K = 2 e imprimem o tempo do
k-means (medido com relógio monotônico) e a acurácia. O carregamento do CSV
fica fora da região cronometrada.


--------------------------------------------------------------------------------
1. REQUISITOS
--------------------------------------------------------------------------------

O arquivo shell.nix na raiz do projeto já fornece o compilador C (gcc) e o
AdaptiveCpp com suporte a ROCm (acpp). Para entrar no ambiente com Nix:

    nix-shell

Alternativamente, AdaptiveCpp pode ser instalado pela AUR ou pelo [repositorio](https://github.com/AdaptiveCpp/AdaptiveCpp)

Necessário para cada parte:

    seq e cpu    gcc com suporte a OpenMP
    gpu          acpp (AdaptiveCpp) e uma GPU compatível, com drivers instalados
    datasets     python3 com numpy e pandas, além de git

Espaço e memória:

    ~20 GB de disco livre para reconstruir os datasets do zero
    ~4,5 GB de RAM para rodar com as 80 milhões de linhas
    ~3,5 GB de VRAM para a versão GPU


--------------------------------------------------------------------------------
2. OBTENDO OS DATASETS
--------------------------------------------------------------------------------

Se o arquivo datasets/treino_balanced_big.csv ainda não existir, gere-o com:

    cd datasets
    ./get_dataset.sh

O script faz todo o processo, da captura bruta até o dataset final:

    1. clona o repositório can-train-and-test (capturas CAN brutas)
    2. monta as árvores de treino e teste               (montar_mesclado_ctt.py)
    3. extrai as 6 colunas de atributos                 (gerar_ctt.py)
    4. reduz para 10 milhões de linhas por conjunto     (rus_ctt.py)
    5. instala os conjuntos de 10M em datasets/
    6. balanceia as classes, normaliza (z-score) e infla para 80 milhões de
       linhas                                     (make_benchmark_dataset.py)

Leve em conta que isso demora cerca de 30-60 minutos e o arquivo final tem 3,93 GB.
O script pode ser executado novamente sem problemas: ele não clona de novo se o
repositório já estiver presente.

Observação: Scripts para tratamento do dataset foram em grande parte reaproveitados de um projeto de pesquisa.

Para gerar o dataset de benchmark com outro tamanho (por exemplo 20 milhões de
linhas, se faltar memória):

    python3 make_benchmark_dataset.py --rows 20000000


--------------------------------------------------------------------------------
3. COMPILAÇÃO
--------------------------------------------------------------------------------

Dentro do diretório src:

    cd src
    make              # compila as três versões (seq, cpu e gpu)

Ou individualmente:

    make seq          # gcc -O3 -o seq seq.c
    make cpu          # gcc -O3 -fopenmp -o cpu cpu.c
    make gpu          # acpp -O3 -o gpu gpu.cc

Para remover os binários:

    make clean


--------------------------------------------------------------------------------
4. EXECUÇÃO
--------------------------------------------------------------------------------

Os três programas aceitam os mesmos argumentos, ambos opcionais:

    ./seq [caminho_do_csv] [máximo_de_linhas]

    caminho_do_csv      padrão: ../datasets/treino_balanced_big.csv
    máximo_de_linhas    padrão: 0, que significa "ler todas as linhas"

Para escolher o número de threads da versão OpenMP, use a variável de ambiente
OMP_NUM_THREADS:

    OMP_NUM_THREADS=8 ./cpu

Sem essa variável, o OpenMP usa todas as threads disponíveis, o que não é o
melhor resultado nesta máquina (veja a seção 6).

Saída esperada:

    Loaded instances: 80000000
    Cluster 0: 19244077 instances -> class 1
    Cluster 1: 60755923 instances -> class 0
    Time: 11.895945 s
    Accuracy: 60.3436%

A versão GPU imprime também o dispositivo utilizado, antes dos clusters:

    Device: AMD Radeon RX 7700 XT


--------------------------------------------------------------------------------
5. VERIFICANDO A CORRETUDE
--------------------------------------------------------------------------------

As três versões são determinísticas e produzem exatamente o mesmo resultado:
acurácia de 60,3436% e clusters de 19.244.077 e 60.755.923 instâncias. Essa
igualdade é a forma mais simples de validar uma alteração no código paralelo:

    ./seq | grep -E "Cluster|Accuracy" > /tmp/ref.txt
    ./gpu | grep -E "Cluster|Accuracy" | diff /tmp/ref.txt -

Se o diff acusar diferença, a versão paralela está errada (atenção: a versão
OpenMP pode trocar os identificadores dos clusters entre 0 e 1, o que não muda
a partição nem a acurácia).


--------------------------------------------------------------------------------
6. RESULTADOS MEDIDOS
--------------------------------------------------------------------------------

Hardware utilizado:

    CPU: AMD Ryzen 7 8700F (8 núcleos, 16 threads), DDR5 dois canais
    GPU: AMD Radeon RX 7700 XT (gfx1101, 12,9 GB)
    AdaptiveCpp 25.02.0

Dataset de 80.000.000 de linhas, 5 atributos, K = 2, 30 iterações:

    versão                         tempo     spedup
    seq                           11,896s     1,00x
    cpu (10 threads, o melhor)     4,481s     2,65x
    cpu (16 threads, o maximo)     5,053s     2,35x
    gpu                            0,820s    14,51x

O detalhamento por número de threads da versão OpenMP está no comentário no
início de src/cpu.c. Em resumo: o ganho para de crescer em torno de 2,6x com 5
threads, porque o algoritmo é limitado pela banda de memória e não pelo
processamento. Acima de 13 threads o tempo aumenta, já que as threads extras
são SMT compartilhando o mesmo núcleo físico.

Na GPU, do tempo total de 0,820s, cerca de 0,256s são a alocação e o envio dos
3,2 GB pelo PCIe, 0,542s são as 30 iterações e 0,023s é a volta dos resultados.


--------------------------------------------------------------------------------
7. OBSERVAÇÕES E PROBLEMAS CONHECIDOS
--------------------------------------------------------------------------------

Primeira execução da versão GPU

    O acpp compila os kernels em tempo de execução (JIT) e emite um aviso na
    primeira execução. A partir da segunda, o cache já está pronto e o tempo
    cai cerca de 0,09s. Para medir desempenho, descarte a primeira execução.

Memória

    Ler as 80 milhões de linhas exige cerca de 4,5 GB de RAM. Em máquinas com
    menos memória, limite o número de linhas pelo segundo argumento, por
    exemplo ./seq ../datasets/treino_balanced_big.csv 20000000.


--------------------------------------------------------------------------------
8. ESTRUTURA DOS ARQUIVOS
--------------------------------------------------------------------------------

    src/seq.c                        k-means sequencial
    src/cpu.c                        k-means com OpenMP
    src/gpu.cc                       k-means com SYCL/USM
    src/Makefile                     regras de compilação

    datasets/get_dataset.sh          pipeline completo dos datasets
    datasets/montar_mesclado_ctt.py  monta as árvores de treino e teste
    datasets/gerar_ctt.py            extrai os atributos das capturas CAN
    datasets/rus_ctt.py              reduz para 10M de linhas por conjunto
    datasets/make_benchmark_dataset.py  balanceia, normaliza e infla para 80M

    shell.nix                        ambiente de compilação para Nix (gcc e acpp)
