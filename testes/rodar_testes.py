# Roda os exemplos do enunciado no simulador e confere com os valores
# calculados a mao. Uso (na raiz do projeto):  python3 testes/rodar_testes.py

import csv
import os
import subprocess
import sys
import tempfile

PASTA_TESTES = os.path.dirname(os.path.abspath(__file__))
RAIZ = os.path.dirname(PASTA_TESTES)

if os.path.exists(os.path.join(RAIZ, "simulador.exe")):
    SIMULADOR = os.path.join(RAIZ, "simulador.exe")
else:
    SIMULADOR = os.path.join(RAIZ, "simulador")


# Cada teste: arquivo, argumentos, o que esperar de cada processo,
# o que esperar no geral e (opcional) as linhas do Gantt
TESTES = [
    {
        "nome": "7.1 FCFS (1 CPU, troca 1, sem memoria)",
        "arquivo": "ex_7_1.json",
        "args": ["--sched", "fcfs", "--custo-troca", "1", "--sem-memoria"],
        "processos": {
            1: {"espera": 0, "resposta": 0, "turnaround": 6},
            2: {"espera": 6, "resposta": 6, "turnaround": 10},
            3: {"espera": 8, "resposta": 8, "turnaround": 11},
        },
        "geral": {"makespan": 13, "espera_media": 4.67, "espera_max": 8, "turnaround_medio": 9.0,
                  "util_por_cpu": "10", "troca_por_cpu": "3", "ocioso_por_cpu": "0", "trocas": 3,
                  "utilizacao_media": 100.0},
        "gantt": [["0", "0", "1", "TROCA"], ["0", "1", "6", "P1"], ["0", "6", "7", "TROCA"],
                  ["0", "7", "10", "P2"], ["0", "10", "11", "TROCA"], ["0", "11", "13", "P3"]],
    },
    {
        "nome": "7.1 SJF (exercicio do enunciado)",
        "arquivo": "ex_7_1.json",
        "args": ["--sched", "sjf", "--custo-troca", "1", "--sem-memoria"],
        "processos": {
            1: {"espera": 7, "resposta": 7, "turnaround": 13},
            2: {"espera": 0, "resposta": 0, "turnaround": 4},
            3: {"espera": 2, "resposta": 2, "turnaround": 5},
        },
        "geral": {"makespan": 13, "util_por_cpu": "10", "troca_por_cpu": "3", "ocioso_por_cpu": "0"},
    },
    {
        "nome": "7.1 RR quantum 2 (exercicio do enunciado)",
        "arquivo": "ex_7_1.json",
        "args": ["--sched", "rr", "--quantum", "2", "--custo-troca", "1", "--sem-memoria"],
        "processos": {
            1: {"espera": 8, "resposta": 0, "turnaround": 16},
            2: {"espera": 9, "resposta": 3, "turnaround": 14},
            3: {"espera": 4, "resposta": 4, "turnaround": 7},
        },
        "geral": {"makespan": 16, "trocas": 6, "util_por_cpu": "10", "troca_por_cpu": "6", "ocioso_por_cpu": "0"},
    },
    {
        "nome": "7.2 page faults e fila (FCFS, troca 0, fault 5)",
        "arquivo": "ex_7_2.json",
        "args": ["--sched", "fcfs", "--custo-troca", "0", "--custo-fault", "5"],
        "processos": {
            1: {"termino": 12, "turnaround": 12, "espera": 0, "page_faults": 2},
            2: {"termino": 13, "turnaround": 12, "espera": 0, "page_faults": 2},
            3: {"termino": 8, "turnaround": 6, "espera": 0, "page_faults": 1},
        },
        "geral": {"makespan": 13, "page_faults": 5, "util_por_cpu": "5", "ocioso_por_cpu": "8"},
    },
    {
        "nome": "7.3 FCFS (troca 0, fault 20)",
        "arquivo": "ex_7_3.json",
        "args": ["--sched", "fcfs", "--custo-troca", "0", "--custo-fault", "20"],
        "processos": {
            1: {"espera": 0, "termino": 24},
            2: {"espera": 4, "termino": 27},
            3: {"espera": 7, "termino": 29},
        },
        "geral": {"espera_media": 3.67},
    },
    {
        "nome": "7.3 SJF (troca 0, fault 20)",
        "arquivo": "ex_7_3.json",
        "args": ["--sched", "sjf", "--custo-troca", "0", "--custo-fault", "20"],
        "processos": {
            1: {"espera": 5, "termino": 29},
            2: {"espera": 2, "termino": 25},
            3: {"espera": 0, "termino": 22},
        },
        "geral": {"espera_media": 2.33},
    },
    {
        "nome": "5.4 processo 17 sem page faults",
        "arquivo": "ex_5_processo17.json",
        "args": ["--custo-troca", "0", "--sem-memoria"],
        "processos": {17: {"termino": 361, "turnaround": 19, "espera": 0}},
        "geral": {},
    },
    {
        "nome": "5.5 processo 17 com page faults (fault 20)",
        "arquivo": "ex_5_processo17.json",
        "args": ["--custo-troca", "0", "--custo-fault", "20"],
        "processos": {17: {"termino": 441, "turnaround": 99, "espera": 0, "page_faults": 4}},
        "geral": {"page_faults": 4},
    },
    {
        "nome": "Volta de E/S na mesma CPU ociosa nao paga troca (troca 1)",
        "arquivo": "ex_5_processo17.json",
        "args": ["--custo-troca", "1", "--sem-memoria"],
        "processos": {17: {"termino": 362, "turnaround": 20, "espera": 0}},
        "geral": {"trocas": 1},
    },
    {
        "nome": "SJF empate: ganha quem entrou primeiro na fila",
        "arquivo": "sjf_empate.json",
        "args": ["--sched", "sjf", "--custo-troca", "0", "--sem-memoria"],
        "processos": {
            1: {"espera": 4}, 2: {"espera": 0}, 3: {"espera": 2},
        },
        "geral": {"trocas": 0},
    },
    {
        "nome": "RR com fila vazia: ganha novo quantum sem troca",
        "arquivo": "rr_sozinho.json",
        "args": ["--sched", "rr", "--quantum", "2", "--custo-troca", "1", "--sem-memoria"],
        "processos": {1: {"termino": 6, "espera": 0}},
        "geral": {"makespan": 6, "trocas": 1},
        "gantt": [["0", "0", "1", "TROCA"], ["0", "1", "6", "P1"]],
    },
    {
        "nome": "4 CPUs: menor indice primeiro e contas por CPU",
        "arquivo": "quatro_cpus.json",
        "args": ["--cpus", "4", "--custo-troca", "1", "--sem-memoria"],
        "processos": {
            1: {"espera": 0, "termino": 4}, 2: {"espera": 0, "termino": 4},
            3: {"espera": 0, "termino": 4}, 4: {"espera": 0, "termino": 4},
            5: {"espera": 4, "resposta": 4, "termino": 8},
        },
        "geral": {"makespan": 8, "util_por_cpu": "6/3/3/3", "troca_por_cpu": "2/1/1/1",
                  "ocioso_por_cpu": "0/4/4/4", "trocas_por_cpu": "2/1/1/1"},
        "gantt": [["0", "0", "1", "TROCA"], ["0", "1", "4", "P1"], ["0", "4", "5", "TROCA"],
                  ["0", "5", "8", "P5"],
                  ["1", "0", "1", "TROCA"], ["1", "1", "4", "P2"], ["1", "4", "8", "OCIOSA"],
                  ["2", "0", "1", "TROCA"], ["2", "1", "4", "P3"], ["2", "4", "8", "OCIOSA"],
                  ["3", "0", "1", "TROCA"], ["3", "1", "4", "P4"], ["3", "4", "8", "OCIOSA"]],
    },
    {
        "nome": "FIFO com 3 quadros: refs 0 1 2 0 3 0 4",
        "arquivo": "fifo_lru.json",
        "args": ["--subst", "fifo", "--quadros", "3", "--custo-troca", "0", "--custo-fault", "1"],
        "processos": {1: {"page_faults": 6}},
        "geral": {"page_faults": 6},
    },
    {
        "nome": "LRU com 3 quadros: refs 0 1 2 0 3 0 4",
        "arquivo": "fifo_lru.json",
        "args": ["--subst", "lru", "--quadros", "3", "--custo-troca", "0", "--custo-fault", "1"],
        "processos": {1: {"page_faults": 5}},
        "geral": {"page_faults": 5},
    },
    {
        "nome": "Sem vitima: 1 quadro, 2 processos (fault 5)",
        "arquivo": "sem_vitima.json",
        "args": ["--quadros", "1", "--custo-troca", "0", "--custo-fault", "5"],
        "processos": {
            1: {"termino": 6, "page_faults": 1},
            2: {"termino": 16, "page_faults": 1, "espera": 0},
        },
        "geral": {"page_faults": 2, "makespan": 16},
    },
]


def ler_csv(caminho):
    with open(caminho, newline="") as arquivo:
        return list(csv.DictReader(arquivo))


def igual(esperado, obtido):
    if isinstance(esperado, float):
        return abs(float(obtido) - esperado) < 0.01
    return str(esperado) == str(obtido)


def rodar_teste(teste, pasta):
    caminho_csv = os.path.join(pasta, "geral.csv")
    caminho_processos = os.path.join(pasta, "processos.csv")
    caminho_gantt = os.path.join(pasta, "gantt.csv")
    if os.path.exists(caminho_csv):
        os.remove(caminho_csv)

    comando = [SIMULADOR, "--arquivo", os.path.join(PASTA_TESTES, teste["arquivo"])] + teste["args"]
    comando += ["--csv", caminho_csv, "--processos", caminho_processos, "--gantt", caminho_gantt]
    resultado = subprocess.run(comando, capture_output=True, text=True)
    if resultado.returncode != 0:
        return ["o simulador deu erro: " + resultado.stdout + resultado.stderr]

    erros = []

    processos = {int(linha["pid"]): linha for linha in ler_csv(caminho_processos)}
    for pid, campos in teste["processos"].items():
        for campo, esperado in campos.items():
            obtido = processos[pid][campo]
            if not igual(esperado, obtido):
                erros.append(f"P{pid} {campo}: esperado {esperado}, obtido {obtido}")

    geral = ler_csv(caminho_csv)[0]
    for campo, esperado in teste["geral"].items():
        if not igual(esperado, geral[campo]):
            erros.append(f"{campo}: esperado {esperado}, obtido {geral[campo]}")

    if "gantt" in teste:
        with open(caminho_gantt, newline="") as arquivo:
            linhas = list(csv.reader(arquivo))[1:]
        if linhas != teste["gantt"]:
            erros.append(f"gantt diferente:\n      esperado {teste['gantt']}\n      obtido   {linhas}")

    return erros


def main():
    if not os.path.exists(SIMULADOR):
        print("Compile primeiro com: make")
        sys.exit(1)

    falhas = 0
    with tempfile.TemporaryDirectory() as pasta:
        for teste in TESTES:
            erros = rodar_teste(teste, pasta)
            if erros:
                falhas += 1
                print(f"[FALHOU] {teste['nome']}")
                for erro in erros:
                    print("    - " + erro)
            else:
                print(f"[  OK  ] {teste['nome']}")

    print()
    print(f"{len(TESTES) - falhas} de {len(TESTES)} testes passaram")
    sys.exit(1 if falhas else 0)


if __name__ == "__main__":
    main()
