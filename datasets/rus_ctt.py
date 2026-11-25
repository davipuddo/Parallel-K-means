import random
import sys
from multiprocessing import Pool
from pathlib import Path

pasta = Path(__file__).resolve().parent
entrada = Path(sys.argv[1]) if len(sys.argv) > 1 else pasta / 'ctt' / 'mesclado'
saida = Path(sys.argv[2]) if len(sys.argv) > 2 else pasta / 'ctt' / 'mesclado_rus'
remover = int(sys.argv[3]) if len(sys.argv) > 3 else 20_000_000
alvo = int(sys.argv[4]) if len(sys.argv) > 4 else None

formatos = ['janela_a', 'sempayload']
sementes = {'treino': 42, 'teste': 43}


def contar(caminho):
    total = normais = 0
    with open(caminho, 'rb') as arquivo:
        next(arquivo)
        for linha in arquivo:
            total += 1
            normais += linha.endswith(b',0\n')
    return total, normais


def reduzir(subconjunto):
    origens = [entrada / f'{subconjunto}_{formato}.csv' for formato in formatos]
    destinos = [saida / f'{subconjunto}_{formato}.csv' for formato in formatos]
    total, normais_restantes = contar(origens[0])
    remover_restantes = total - alvo if alvo is not None else remover
    if remover_restantes > normais_restantes:
        raise ValueError(f'{subconjunto}: so ha {normais_restantes} normais')

    sorteio = random.Random(sementes[subconjunto])
    linhas = ataques = 0

    leitores = [open(caminho, 'rb') for caminho in origens]
    escritores = [open(caminho, 'wb') for caminho in destinos]
    for leitor, escritor in zip(leitores, escritores):
        escritor.write(next(leitor))

    for registro in zip(*leitores, strict=True):
        rotulo = registro[0][-2:]
        if any(linha[-2:] != rotulo for linha in registro):
            raise ValueError(f'{subconjunto}: rotulos divergentes entre formatos')
        if rotulo == b'0\n':
            if sorteio.random() * normais_restantes < remover_restantes:
                remover_restantes -= 1
                normais_restantes -= 1
                continue
            normais_restantes -= 1
        else:
            ataques += 1
        for escritor, linha in zip(escritores, registro):
            escritor.write(linha)
        linhas += 1

    for arquivo in leitores + escritores:
        arquivo.close()
    return subconjunto, linhas, ataques, remover_restantes


def main():
    saida.mkdir(parents=True, exist_ok=True)
    with Pool(len(sementes)) as pool:
        for nome, linhas, ataques, faltando in pool.map(reduzir, sementes):
            print(f'{nome}: {linhas} linhas, {ataques} ataques, '
                  f'{100 * ataques / linhas:.3f}% maliciosas, faltou remover {faltando}')


if __name__ == '__main__':
    main()
