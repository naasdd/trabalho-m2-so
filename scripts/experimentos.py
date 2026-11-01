# Roda todas as execucoes da secao 11 do enunciado e salva os resultados em resultados/
# Uso (na raiz do projeto):  python3 scripts/experimentos.py

import csv
import os
import shutil
import subprocess
import sys

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RESULTADOS = os.path.join(RAIZ, "resultados")

if os.path.exists(os.path.join(RAIZ, "simulador.exe")):
    SIMULADOR = os.path.join(RAIZ, "simulador.exe")
else:
    SIMULADOR = os.path.join(RAIZ, "simulador")

CENARIOS = {
    "A": "cenarios/cenario_A_t0.json",
    "B": "cenarios/cenario_B_espacadas.json",
}
CPUS = [1, 4]
QUANTUMS = [2, 5, 10, 20, 50]

# (nome, argumentos) de cada escalonador das execucoes base
ESCALONADORES = [("fcfs", ["--sched", "fcfs"]), ("sjf", ["--sched", "sjf"])]
for q in QUANTUMS:
    ESCALONADORES.append((f"rr{q}", ["--sched", "rr", "--quantum", str(q)]))


def rodar(csv_saida, cenario, cpus, args_extras, nome_execucao=None):
    comando = [SIMULADOR, "--arquivo", CENARIOS[cenario], "--cpus", str(cpus)] + args_extras
    comando += ["--csv", os.path.join(RESULTADOS, csv_saida)]

    # nas execucoes base guardamos tambem o Gantt e as metricas de cada processo
    if nome_execucao:
        comando += ["--gantt", os.path.join(RESULTADOS, "gantt", nome_execucao + ".csv")]
        comando += ["--processos", os.path.join(RESULTADOS, "processos", nome_execucao + ".csv")]
    else:
        comando += ["--gantt", os.path.join(RESULTADOS, "gantt_temp.csv")]

    resultado = subprocess.run(comando, cwd=RAIZ, capture_output=True, text=True)
    if resultado.returncode != 0:
        print("ERRO ao rodar:", " ".join(comando))
        print(resultado.stdout, resultado.stderr)
        sys.exit(1)


def ler_csv(nome):
    with open(os.path.join(RESULTADOS, nome), newline="") as arquivo:
        return list(csv.DictReader(arquivo))


def nome_cenario(caminho):
    return "A" if "cenario_A" in caminho else "B"


def nome_escalonador(linha):
    if linha["escalonador"] == "rr":
        return "rr" + linha["quantum"]
    return linha["escalonador"]


# ------------------------------------------------------------
# 1. Execucoes base: 2 cenarios x 2 configuracoes x 7 escalonadores = 28 (LRU, 256 quadros)
# ------------------------------------------------------------
def execucoes_base():
    print("Execucoes base (28)...")
    for cenario in CENARIOS:
        for cpus in CPUS:
            for nome, args in ESCALONADORES:
                execucao = f"{cenario}_{cpus}cpu_{nome}"
                rodar("base.csv", cenario, cpus, args + ["--subst", "lru", "--quadros", "256"], execucao)


# ------------------------------------------------------------
# 2. FIFO x LRU: 2 cenarios x 2 configuracoes x (FCFS e RR q=10) = 8 configuracoes
# ------------------------------------------------------------
def fifo_vs_lru():
    print("FIFO x LRU (8 configuracoes x 2 politicas)...")
    for cenario in CENARIOS:
        for cpus in CPUS:
            for nome, args in [("fcfs", ["--sched", "fcfs"]), ("rr10", ["--sched", "rr", "--quantum", "10"])]:
                for politica in ["fifo", "lru"]:
                    rodar("fifo_lru.csv", cenario, cpus, args + ["--subst", politica])


# ------------------------------------------------------------
# 3. Variacao de quadros (opcional no enunciado): mostra o thrashing
# ------------------------------------------------------------
def variacao_quadros():
    print("Variacao de quadros...")
    for cenario in CENARIOS:
        for cpus in CPUS:
            for quadros in [32, 64, 128, 256, 512, 1024, 2048]:
                rodar("quadros.csv", cenario, cpus,
                      ["--sched", "rr", "--quantum", "10", "--subst", "lru", "--quadros", str(quadros)])


# ------------------------------------------------------------
# 4. Variacao do custo de troca (objetivo 4 do enunciado)
# ------------------------------------------------------------
def variacao_custo_troca():
    print("Variacao do custo de troca...")
    for cenario in CENARIOS:
        for nome, args in [("fcfs", ["--sched", "fcfs"]), ("rr2", ["--sched", "rr", "--quantum", "2"]),
                           ("rr10", ["--sched", "rr", "--quantum", "10"])]:
            for custo in [0, 1, 2, 5]:
                rodar("custo_troca.csv", cenario, 4, args + ["--custo-troca", str(custo)])


# ------------------------------------------------------------
# 5. Speedup e eficiencia (makespan 1 CPU / makespan 4 CPUs)
# ------------------------------------------------------------
def calcular_speedup():
    print("Speedup...")
    base = ler_csv("base.csv")
    makespan = {}
    for linha in base:
        chave = (nome_cenario(linha["arquivo"]), nome_escalonador(linha), int(linha["cpus"]))
        makespan[chave] = int(linha["makespan"])

    with open(os.path.join(RESULTADOS, "speedup.csv"), "w", newline="") as arquivo:
        escritor = csv.writer(arquivo)
        escritor.writerow(["cenario", "escalonador", "makespan_1cpu", "makespan_4cpus", "speedup", "eficiencia"])
        for cenario in CENARIOS:
            for nome, _ in ESCALONADORES:
                m1 = makespan[(cenario, nome, 1)]
                m4 = makespan[(cenario, nome, 4)]
                speedup = m1 / m4
                escritor.writerow([cenario, nome, m1, m4, f"{speedup:.3f}", f"{speedup / 4:.3f}"])


# ------------------------------------------------------------
# 6. Tabelas resumo por cenario (uma linha por execucao, uma coluna por metrica)
# ------------------------------------------------------------
def tabelas_resumo():
    print("Tabelas resumo...")
    base = ler_csv("base.csv")
    speedup = {(l["cenario"], l["escalonador"]): l for l in ler_csv("speedup.csv")}

    for cenario in CENARIOS:
        linhas = [l for l in base if nome_cenario(l["arquivo"]) == cenario]
        caminho = os.path.join(RESULTADOS, f"tabela_cenario_{cenario}.md")
        with open(caminho, "w") as arquivo:
            arquivo.write(f"# Cenario {cenario} (LRU, 256 quadros, custo de troca 1)\n\n")
            arquivo.write("| CPUs | Escalonador | Espera media | Espera max | Turnaround medio | Resposta media "
                          "| Makespan | Speedup | Eficiencia | Trocas | Trocas por CPU | Utilizacao por CPU (%) "
                          "| Ocioso por CPU | Page faults | Taxa de faults (%) |\n")
            arquivo.write("|" + "---|" * 15 + "\n")
            for l in linhas:
                makespan = int(l["makespan"])
                utilizacoes = []
                for util, troca in zip(l["util_por_cpu"].split("/"), l["troca_por_cpu"].split("/")):
                    utilizacoes.append(f"{100 * (int(util) + int(troca)) / makespan:.1f}")
                s = speedup[(cenario, nome_escalonador(l))]
                arquivo.write(f"| {l['cpus']} | {nome_escalonador(l)} | {l['espera_media']} | {l['espera_max']} "
                              f"| {l['turnaround_medio']} | {l['resposta_media']} | {l['makespan']} "
                              f"| {s['speedup']} | {s['eficiencia']} | {l['trocas']} | {l['trocas_por_cpu']} "
                              f"| {'/'.join(utilizacoes)} | {l['ocioso_por_cpu']} | {l['page_faults']} "
                              f"| {float(l['taxa_faults']):.2f} |\n")


def main():
    if not os.path.exists(SIMULADOR):
        print("Compile primeiro com: make")
        sys.exit(1)

    # comeca do zero, porque o simulador acrescenta linhas nos CSVs
    for pasta in ["gantt", "processos"]:
        shutil.rmtree(os.path.join(RESULTADOS, pasta), ignore_errors=True)
        os.makedirs(os.path.join(RESULTADOS, pasta))
    for nome in ["base.csv", "fifo_lru.csv", "quadros.csv", "custo_troca.csv"]:
        caminho = os.path.join(RESULTADOS, nome)
        if os.path.exists(caminho):
            os.remove(caminho)

    execucoes_base()
    fifo_vs_lru()
    variacao_quadros()
    variacao_custo_troca()
    calcular_speedup()
    tabelas_resumo()

    os.remove(os.path.join(RESULTADOS, "gantt_temp.csv"))
    print("Pronto! Resultados em resultados/")


if __name__ == "__main__":
    main()
