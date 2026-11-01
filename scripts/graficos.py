# Gera os graficos e as figuras do Gantt a partir dos CSVs de resultados/
# Uso (na raiz do projeto, depois de rodar experimentos.py):  python3 scripts/graficos.py
# Precisa do matplotlib:  pip3 install matplotlib

import csv
import os

import matplotlib
matplotlib.use("Agg")  # so salva PNG, nao abre janela
import matplotlib.pyplot as plt

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RESULTADOS = os.path.join(RAIZ, "resultados")
FIGURAS = os.path.join(RESULTADOS, "figuras")

ESCALONADORES = ["fcfs", "sjf", "rr2", "rr5", "rr10", "rr20", "rr50"]
QUANTUMS = [2, 5, 10, 20, 50]
CORES = {"A-1": "#1f77b4", "A-4": "#aec7e8", "B-1": "#d62728", "B-4": "#ff9896"}


def ler_csv(nome):
    with open(os.path.join(RESULTADOS, nome), newline="") as arquivo:
        return list(csv.DictReader(arquivo))


def cenario(linha):
    return "A" if "cenario_A" in linha["arquivo"] else "B"


def escalonador(linha):
    if linha["escalonador"] == "rr":
        return "rr" + linha["quantum"]
    return linha["escalonador"]


def buscar(linhas, cen, cpus, esc):
    for l in linhas:
        if cenario(l) == cen and int(l["cpus"]) == cpus and escalonador(l) == esc:
            return l
    return None


def salvar(nome):
    plt.tight_layout()
    plt.savefig(os.path.join(FIGURAS, nome), dpi=150)
    plt.close()
    print("  " + nome)


# ------------------------------------------------------------
# 1. Espera media por escalonador
# ------------------------------------------------------------
def grafico_espera(base):
    fig, eixos = plt.subplots(1, 2, figsize=(12, 4.5))
    largura = 0.38
    for i, cpus in enumerate([1, 4]):
        eixo = eixos[i]
        for j, cen in enumerate(["A", "B"]):
            valores = [float(buscar(base, cen, cpus, e)["espera_media"]) for e in ESCALONADORES]
            posicoes = [k + (j - 0.5) * largura for k in range(len(ESCALONADORES))]
            eixo.bar(posicoes, valores, largura, label=f"Cenario {cen}", color=CORES[f"{cen}-{cpus}"] if cpus == 1 else CORES[f"{cen}-4"])
        eixo.set_xticks(range(len(ESCALONADORES)))
        eixo.set_xticklabels(ESCALONADORES)
        eixo.set_title(f"Espera media - {cpus} CPU(s)")
        eixo.set_ylabel("unidades de tempo")
        eixo.legend()
        eixo.grid(axis="y", alpha=0.3)
    salvar("1_espera_por_escalonador.png")


# ------------------------------------------------------------
# 2. Efeito do quantum no RR (espera, resposta e trocas)
# ------------------------------------------------------------
def grafico_quantum(base):
    metricas = [("espera_media", "Espera media"), ("resposta_media", "Resposta media"), ("trocas", "Trocas de contexto")]
    fig, eixos = plt.subplots(1, 3, figsize=(15, 4.5))
    for i, (campo, titulo) in enumerate(metricas):
        eixo = eixos[i]
        for cen in ["A", "B"]:
            for cpus in [1, 4]:
                valores = [float(buscar(base, cen, cpus, f"rr{q}")[campo]) for q in QUANTUMS]
                eixo.plot(QUANTUMS, valores, marker="o", label=f"Cenario {cen}, {cpus} CPU(s)", color=CORES[f"{cen}-{cpus}"])
        eixo.set_xscale("log")
        eixo.set_xticks(QUANTUMS)
        eixo.set_xticklabels(QUANTUMS)
        eixo.set_yscale("log")
        eixo.set_xlabel("quantum")
        eixo.set_title(titulo + " x quantum (RR)")
        eixo.grid(alpha=0.3)
    eixos[0].legend()
    salvar("2_efeito_do_quantum.png")


# ------------------------------------------------------------
# 3. Speedup por escalonador
# ------------------------------------------------------------
def grafico_speedup():
    speedup = ler_csv("speedup.csv")
    fig, eixo = plt.subplots(figsize=(10, 4.5))
    largura = 0.38
    for j, cen in enumerate(["A", "B"]):
        valores = [float(l["speedup"]) for e in ESCALONADORES for l in speedup if l["cenario"] == cen and l["escalonador"] == e]
        posicoes = [k + (j - 0.5) * largura for k in range(len(ESCALONADORES))]
        barras = eixo.bar(posicoes, valores, largura, label=f"Cenario {cen}", color=CORES[f"{cen}-1"])
        for barra, valor in zip(barras, valores):
            eixo.text(barra.get_x() + barra.get_width() / 2, valor + 0.05, f"{valor:.2f}", ha="center", fontsize=8)
    eixo.axhline(4, color="black", linestyle="--", label="ideal (4)")
    eixo.set_xticks(range(len(ESCALONADORES)))
    eixo.set_xticklabels(ESCALONADORES)
    eixo.set_ylabel("speedup (makespan 1 CPU / 4 CPUs)")
    eixo.set_title("Speedup por escalonador")
    eixo.legend()
    eixo.grid(axis="y", alpha=0.3)
    salvar("3_speedup.png")


# ------------------------------------------------------------
# 4. FIFO x LRU
# ------------------------------------------------------------
def grafico_fifo_lru():
    linhas = ler_csv("fifo_lru.csv")
    configuracoes = []
    for cen in ["A", "B"]:
        for cpus in [1, 4]:
            for esc in ["fcfs", "rr10"]:
                configuracoes.append((cen, cpus, esc))

    fig, eixo = plt.subplots(figsize=(12, 4.5))
    largura = 0.38
    for j, politica in enumerate(["fifo", "lru"]):
        valores = []
        for cen, cpus, esc in configuracoes:
            for l in linhas:
                if cenario(l) == cen and int(l["cpus"]) == cpus and escalonador(l) == esc and l["substituicao"] == politica:
                    valores.append(int(l["page_faults"]))
        posicoes = [k + (j - 0.5) * largura for k in range(len(configuracoes))]
        eixo.bar(posicoes, valores, largura, label=politica.upper(), color=["#9467bd", "#2ca02c"][j])
    eixo.set_xticks(range(len(configuracoes)))
    eixo.set_xticklabels([f"{c} {n}CPU\n{e}" for c, n, e in configuracoes])
    eixo.set_ylabel("page faults")
    eixo.set_title("FIFO x LRU (256 quadros)")
    eixo.legend()
    eixo.grid(axis="y", alpha=0.3)
    salvar("4_fifo_vs_lru.png")


# ------------------------------------------------------------
# 5. Variacao de quadros (thrashing)
# ------------------------------------------------------------
def grafico_quadros():
    linhas = ler_csv("quadros.csv")
    fig, eixos = plt.subplots(1, 3, figsize=(15, 4.5))
    campos = [("page_faults", "Page faults"), ("makespan", "Makespan"), ("espera_media", "Espera media")]
    for i, (campo, titulo) in enumerate(campos):
        eixo = eixos[i]
        for cen in ["A", "B"]:
            for cpus in [1, 4]:
                pontos = [(int(l["quadros"]), float(l[campo])) for l in linhas if cenario(l) == cen and int(l["cpus"]) == cpus]
                pontos.sort()
                eixo.plot([p[0] for p in pontos], [p[1] for p in pontos], marker="o",
                          label=f"Cenario {cen}, {cpus} CPU(s)", color=CORES[f"{cen}-{cpus}"])
        eixo.set_xscale("log", base=2)
        eixo.set_xticks([32, 64, 128, 256, 512, 1024, 2048])
        eixo.set_xticklabels([32, 64, 128, 256, 512, 1024, 2048])
        eixo.set_xlabel("quadros")
        eixo.set_title(titulo + " x quadros (RR q=10, LRU)")
        eixo.grid(alpha=0.3)
    eixos[0].legend()
    salvar("5_variacao_quadros.png")


# ------------------------------------------------------------
# 6. Justica: curtos x longos
# ------------------------------------------------------------
def grafico_justica(base):
    fig, eixos = plt.subplots(1, 2, figsize=(13, 4.5))
    largura = 0.38
    for i, (cen, cpus) in enumerate([("A", 1), ("B", 4)]):
        eixo = eixos[i]
        for j, classe in enumerate(["curto", "longo"]):
            valores = [float(buscar(base, cen, cpus, e)["slowdown_" + classe]) for e in ESCALONADORES]
            posicoes = [k + (j - 0.5) * largura for k in range(len(ESCALONADORES))]
            eixo.bar(posicoes, valores, largura, label=f"processos {classe}s", color=["#ff7f0e", "#1f77b4"][j])
        eixo.set_xticks(range(len(ESCALONADORES)))
        eixo.set_xticklabels(ESCALONADORES)
        eixo.set_ylabel("slowdown medio (turnaround / tempo de servico)")
        eixo.set_title(f"Justica - cenario {cen}, {cpus} CPU(s)")
        eixo.legend()
        eixo.grid(axis="y", alpha=0.3)
    salvar("6_justica_curtos_longos.png")


# ------------------------------------------------------------
# 7. Custo da troca de contexto
# ------------------------------------------------------------
def grafico_custo_troca():
    linhas = ler_csv("custo_troca.csv")
    fig, eixos = plt.subplots(1, 2, figsize=(12, 4.5))
    for cen in ["A", "B"]:
        for esc, estilo in [("fcfs", "-"), ("rr2", "--"), ("rr10", ":")]:
            pontos = sorted((int(l["custo_troca"]), l) for l in linhas if cenario(l) == cen and escalonador(l) == esc)
            custos = [p[0] for p in pontos]
            makespans = [int(p[1]["makespan"]) for p in pontos]
            fracao_troca = []
            for _, l in pontos:
                troca = sum(int(t) for t in l["troca_por_cpu"].split("/"))
                fracao_troca.append(100 * troca / (int(l["makespan"]) * int(l["cpus"])))
            cor = CORES[f"{cen}-1"]
            eixos[0].plot(custos, makespans, estilo, marker="o", color=cor, label=f"Cenario {cen} {esc}")
            eixos[1].plot(custos, fracao_troca, estilo, marker="o", color=cor, label=f"Cenario {cen} {esc}")
    eixos[0].set_title("Makespan x custo de troca (4 CPUs)")
    eixos[1].set_title("% do tempo das CPUs gasto em troca (4 CPUs)")
    for eixo in eixos:
        eixo.set_xlabel("custo da troca de contexto")
        eixo.grid(alpha=0.3)
        eixo.legend(fontsize=8)
    salvar("7_custo_troca.png")


# ------------------------------------------------------------
# Gantt (janela 0 a 200)
# ------------------------------------------------------------
def cor_do_processo(pid):
    paleta = plt.get_cmap("tab20")
    return paleta(pid % 20)


def figura_gantt(nome_execucao, titulo):
    caminho = os.path.join(RESULTADOS, "gantt", nome_execucao + ".csv")
    with open(caminho, newline="") as arquivo:
        trechos = list(csv.DictReader(arquivo))

    num_cpus = max(int(t["cpu"]) for t in trechos) + 1
    fig, eixo = plt.subplots(figsize=(16, 1.1 + 0.8 * num_cpus))

    for t in trechos:
        cpu = int(t["cpu"])
        inicio = int(t["inicio"])
        duracao = int(t["fim"]) - inicio
        evento = t["evento"]
        if evento == "TROCA":
            cor = "#404040"
        elif evento == "OCIOSA":
            cor = "#f0f0f0"
        else:
            cor = cor_do_processo(int(evento[1:]))
        eixo.broken_barh([(inicio, duracao)], (cpu - 0.4, 0.8), facecolors=cor, edgecolor="white", linewidth=0.4)
        if evento.startswith("P") and duracao >= 4:
            eixo.text(inicio + duracao / 2, cpu, evento, ha="center", va="center", fontsize=7)

    eixo.set_yticks(range(num_cpus))
    eixo.set_yticklabels([f"CPU {c}" for c in range(num_cpus)])
    eixo.invert_yaxis()
    eixo.set_xlim(0, 200)
    eixo.set_xticks(range(0, 201, 10))
    eixo.set_xlabel("tempo")
    eixo.set_title(titulo + "   (cinza escuro = TROCA, cinza claro = OCIOSA)")
    eixo.grid(axis="x", alpha=0.3)
    salvar(f"gantt_{nome_execucao}.png")


def main():
    os.makedirs(FIGURAS, exist_ok=True)
    base = ler_csv("base.csv")

    print("Gerando graficos em resultados/figuras/")
    grafico_espera(base)
    grafico_quantum(base)
    grafico_speedup()
    grafico_fifo_lru()
    grafico_quadros()
    grafico_justica(base)
    grafico_custo_troca()

    for cen in ["A", "B"]:
        for cpus in [1, 4]:
            for esc in ["fcfs", "rr10"]:
                nome = f"{cen}_{cpus}cpu_{esc}"
                figura_gantt(nome, f"Gantt - cenario {cen}, {cpus} CPU(s), {esc.upper()}, LRU 256 quadros")


if __name__ == "__main__":
    main()
