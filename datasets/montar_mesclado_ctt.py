import hashlib
import sys
from pathlib import Path

pasta = Path(__file__).resolve().parent
origem = pasta / 'can-train-and-test'
saida = Path(sys.argv[1]) if len(sys.argv) > 1 else origem / 'mesclado'

conhecido = ('train_01', 'test_01_known_vehicle_known_attack', 'test_03_known_vehicle_unknown_attack')
desconhecido = ('test_02_unknown_vehicle_known_attack', 'test_04_unknown_vehicle_unknown_attack')
veiculos = {
    'set_02': {'traverse': conhecido, 'forester': desconhecido},
    'set_04': {'forester': conhecido, 'traverse': desconhecido},
}
partes = {'1': 'train_01', '2': 'train_01', '3': 'test', '4': 'test'}


def md5(caminho):
    resumo = hashlib.md5()
    with open(caminho, 'rb') as arquivo:
        for bloco in iter(lambda: arquivo.read(1 << 20), b''):
            resumo.update(bloco)
    return resumo.hexdigest()


def main():
    capturas = {}
    for conjunto, carros in veiculos.items():
        for carro, subconjuntos in carros.items():
            for subconjunto in subconjuntos:
                for caminho in sorted((origem / conjunto / subconjunto).glob('*.csv')):
                    nome = f'{carro}_{caminho.name}'
                    anterior = capturas.get(nome)
                    if anterior is None:
                        capturas[nome] = caminho
                    elif anterior.stat().st_size != caminho.stat().st_size or md5(anterior) != md5(caminho):
                        raise ValueError(f'{nome}: {anterior} e {caminho} diferem')

    for parte in set(partes.values()):
        destino = saida / parte
        destino.mkdir(parents=True, exist_ok=True)
        for antigo in destino.glob('*.csv'):
            antigo.unlink()

    contagem = {}
    for nome, caminho in sorted(capturas.items()):
        numero = caminho.stem.rsplit('-', 1)[-1]
        if numero not in partes:
            raise ValueError(f'{caminho}: numero de captura inesperado')
        parte = partes[numero]
        (saida / parte / nome).symlink_to(caminho)
        contagem[parte] = contagem.get(parte, 0) + 1
    for parte, total in sorted(contagem.items()):
        print(f'{parte}: {total} capturas')


if __name__ == '__main__':
    main()
