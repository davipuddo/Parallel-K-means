import csv
import math
import sys
from collections import Counter, deque
from multiprocessing import Pool
from pathlib import Path

pasta = Path(__file__).resolve().parent
entrada = Path(sys.argv[1]) if len(sys.argv) > 1 else pasta / 'can-train-and-test' / 'mesclado'
saida = Path(sys.argv[2]) if len(sys.argv) > 2 else pasta / 'ctt' / 'mesclado'

cabecalho_sempayload = ['arbitration_id', 'dlc', 'dt_id', 'dt', 'mudou', 'flag_binaria']
cabecalho_janela_a = ['dlc', 'dt_id', 'dt', 'dt_relativo', 'novidade', 'dlc_diferente',
                      'taxa_relativa', 'irregularidade', 'flag_binaria']
hexadecimais = set('0123456789abcdefABCDEF')
limiar_periodo = 0.75
peso_periodo = 0.1
contagem_teto = 1000
periodos_janela = 10


class EstadoId:
    def __init__(self):
        self.tempo = None
        self.payload = None
        self.periodo = None
        self.intervalo = None
        self.contagem = 0
        self.dlcs = Counter()
        self.dlc_comum = None
        self.tempos = deque()


def escala_tempo(segundos):
    if segundos is None:
        return 1.0
    return min(1.0, math.log1p(max(0.0, segundos) * 1e6) / math.log1p(1e6))


def ler_linha(linha, caminho, numero):
    tempo, ident, dado, ataque = (campo.strip() for campo in linha)
    if (not ident or not set(ident) <= hexadecimais or int(ident, 16) > 0x7FF
            or not set(dado) <= hexadecimais or len(dado) % 2 or len(dado) > 16
            or ataque not in ('0', '1')):
        raise ValueError(f'{caminho}:{numero}: linha invalida {linha}')
    payload = tuple(int(dado[i:i + 2], 16) for i in range(0, len(dado), 2))
    return float(tempo), int(ident, 16), payload, int(ataque)


def dt_relativo(estado, intervalo):
    if estado.periodo is None:
        estado.periodo = max(intervalo, 1e-6)
        return 0.5
    razao = intervalo / estado.periodo
    if razao >= limiar_periodo:
        estado.periodo += peso_periodo * (min(intervalo, 4 * estado.periodo) - estado.periodo)
    return min(razao, 2.0) / 2.0


def atributos(estado, tempo, payload):
    dlc = len(payload)
    if estado.tempo is None:
        dt_id = relativo = taxa = 1.0
        irregularidade = 0.0
        mudou = 1
        dlc_diferente = 0
    else:
        intervalo = max(0.0, tempo - estado.tempo)
        dt_id = escala_tempo(intervalo)
        relativo = dt_relativo(estado, intervalo)
        mudou = 1 if payload != estado.payload else 0
        dlc_diferente = 1 if dlc != estado.dlc_comum else 0

        limite = tempo - periodos_janela * estado.periodo
        while estado.tempos and estado.tempos[0] < limite:
            estado.tempos.popleft()
        taxa = len(estado.tempos) / periodos_janela
        irregularidade = 0.0
        if estado.intervalo is not None:
            irregularidade = abs(intervalo - estado.intervalo) / estado.periodo
        estado.intervalo = intervalo
    estado.tempos.append(tempo)
    novidade = min(1.0, math.log1p(estado.contagem) / math.log1p(contagem_teto))

    estado.tempo = tempo
    estado.payload = payload
    estado.contagem += 1
    estado.dlcs[dlc] += 1
    if estado.dlc_comum is None or estado.dlcs[dlc] > estado.dlcs[estado.dlc_comum]:
        estado.dlc_comum = dlc
    return dlc, dt_id, relativo, mudou, dlc_diferente, novidade, min(taxa, 4.0) / 4.0, min(irregularidade, 2.0) / 2.0


def converter(item):
    origem, destino = item
    with open(f'{destino}_sempayload.csv', 'w', newline='') as arquivo_sempayload, \
         open(f'{destino}_janela_a.csv', 'w', newline='') as arquivo_janela_a:
        sempayload = csv.writer(arquivo_sempayload, lineterminator='\n')
        janela_a = csv.writer(arquivo_janela_a, lineterminator='\n')
        sempayload.writerow(cabecalho_sempayload)
        janela_a.writerow(cabecalho_janela_a)

        linhas = ataques = 0
        for caminho in sorted(origem.glob('*.csv'), key=lambda p: p.name.lower()):
            estados = {}
            ultimo_tempo = None
            with open(caminho, newline='') as arquivo:
                leitor = csv.reader(arquivo)
                next(leitor)
                for numero, linha in enumerate(leitor, start=2):
                    tempo, ident, payload, ataque = ler_linha(linha, caminho, numero)
                    estado = estados.setdefault(ident, EstadoId())
                    dlc, dt_id, relativo, mudou, dlc_diferente, novidade, taxa, irregularidade = \
                        atributos(estado, tempo, payload)
                    dt = escala_tempo(tempo - ultimo_tempo if ultimo_tempo is not None else None)
                    ultimo_tempo = tempo

                    comum = [f'{dlc / 8:.6f}', f'{dt_id:.6f}', f'{dt:.6f}']
                    sempayload.writerow([f'{ident / 2047:.6f}'] + comum + [mudou, ataque])
                    janela_a.writerow(comum + [f'{relativo:.6f}', f'{novidade:.6f}', dlc_diferente,
                                               f'{taxa:.6f}', f'{irregularidade:.6f}', ataque])
                    linhas += 1
                    ataques += ataque
    return destino.name, linhas, ataques


def main():
    saida.mkdir(parents=True, exist_ok=True)
    itens = [(entrada / 'train_01', saida / 'treino'), (entrada / 'test', saida / 'teste')]
    with Pool(len(itens)) as pool:
        for nome, linhas, ataques in pool.map(converter, itens):
            print(f'{nome}: {linhas} linhas, {100 * ataques / linhas:.2f}% maliciosas')


if __name__ == '__main__':
    main()
