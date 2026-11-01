# Simulador de Escalonamento Multiprocessado com Paginação sob Demanda

Trabalho M2 de **Sistemas Operacionais** (UNIVALI). Simula FCFS, SJF e Round Robin em
máquinas com 1 ou 4 CPUs, com memória paginada (page fault, FIFO e LRU) e custo de troca de contexto.
O enunciado está em [`docs/enunciado-m2.pdf`](docs/enunciado-m2.pdf).

## Compilar

Precisa de um compilador C++17 (g++ ou clang). Funciona no macOS, no Linux e no Windows (MinGW).

```bash
make
```

No Windows: `mingw32-make`. Sem make: `g++ -std=c++17 -O2 -Iinclude src/*.cpp -o simulador`

## Rodar

```bash
./simulador --arquivo cenarios/cenario_A_t0.json --cpus 4 --sched rr --quantum 10 --subst lru
```

| Argumento | Valores | Padrão |
|---|---|---|
| `--arquivo` | caminho do JSON | (obrigatório) |
| `--cpus` | 1, 4 (ou qualquer n ≥ 1) | 1 |
| `--sched` | `fcfs`, `sjf`, `rr` | `fcfs` |
| `--quantum` | inteiro (só no RR) | 10 |
| `--subst` | `fifo`, `lru` | `lru` |
| `--quadros` | inteiro | 256 |
| `--custo-troca` | inteiro | 1 |
| `--log` | gera o log de estados em `log.txt` | desligado |

Extras (fora da lista do enunciado):

| Argumento | Para que serve |
|---|---|
| `--custo-fault <n>` | tempo do page fault (padrão 20). O exemplo 7.2 usa 5 |
| `--sem-memoria` | desliga a paginação (o exemplo 7.1 não tem memória) |
| `--gantt <arquivo>` | onde salvar o Gantt (padrão `gantt.csv`) |
| `--log-arquivo <arquivo>` | onde salvar o log (padrão `log.txt`) |
| `--log-ate <t>` | só registra o log até o instante `t` |
| `--csv <arquivo>` | acrescenta uma linha com todas as métricas (usado pelos experimentos) |
| `--processos <arquivo>` | salva as métricas de cada processo |

> **Sobre o log nos cenários de 1000 processos:** o log completo do cenário A chega a algumas
> centenas de MB, porque a lista de bloqueados tem até 1000 processos por instante.
> Para olhar só o começo, use por exemplo `--log --log-ate 200`.

## Saídas

- **Resumo** (na tela): parâmetros, métricas globais, tabela por CPU (útil / troca / ocioso),
  page faults e taxa de faults, e métricas por classe (curto × longo).
- **Gantt** (`gantt.csv`): janela de 0 a 200, no formato `cpu,inicio,fim,evento`
  (o evento é `Pn`, `TROCA` ou `OCIOSA`).
- **Log** (`log.txt`, com `--log`): a cada instante em que algo muda, mostra os eventos,
  a fila de prontos, os bloqueados (com o instante de retorno), o processo em cada CPU
  e os quadros ocupados.

## Testes (exemplos do enunciado)

```bash
make teste
```

Roda os exemplos 5.4, 5.5, 7.1 (FCFS, SJF e RR q=2), 7.2 e 7.3 (FCFS e SJF) e mais alguns casos
calculados à mão (4 CPUs, FIFO × LRU, sem vítima, empate no SJF, RR com fila vazia, volta de E/S
sem troca). Cada teste compara espera, resposta, turnaround, término, page faults, makespan,
as parcelas de cada CPU e o Gantt com os valores esperados.

## Experimentos (seção 11) e gráficos

```bash
make experimentos     # roda as 112 execucoes (~15 s) -> resultados/*.csv
pip3 install matplotlib
make graficos         # gera resultados/figuras/*.png
```

| Arquivo | Conteúdo |
|---|---|
| `resultados/base.csv` | as 28 execuções base (A e B × 1 e 4 CPUs × FCFS, SJF, RR q=2/5/10/20/50; LRU, 256 quadros) |
| `resultados/tabela_cenario_A.md`, `..._B.md` | tabela resumo por cenário |
| `resultados/speedup.csv` | speedup e eficiência por escalonador |
| `resultados/fifo_lru.csv` | FIFO × LRU em 8 configurações |
| `resultados/quadros.csv` | variação de quadros (32 a 2048) |
| `resultados/custo_troca.csv` | variação do custo de troca (0, 1, 2, 5) |
| `resultados/gantt/` | Gantt de cada execução base |
| `resultados/processos/` | métricas de cada processo em cada execução base |
| `resultados/figuras/` | gráficos e figuras do Gantt |

Os resultados são determinísticos: rodar de novo gera exatamente os mesmos números.

## Documentos

| Arquivo | Conteúdo |
|---|---|
| [`docs/Relatorio_M2_SO.pdf`](docs/Relatorio_M2_SO.pdf) | Relatório (ABNT) com fundamentação, implementação, resultados e análise |
| [`docs/GUIA_ESTUDOS.md`](docs/GUIA_ESTUDOS.md) | Guia de estudos: conceitos e code review guiado para a defesa |
| [`docs/enunciado-m2.pdf`](docs/enunciado-m2.pdf) | Enunciado do trabalho |

## Organização do código

| Arquivo | O que faz |
|---|---|
| `src/main.cpp` | lê e valida os argumentos |
| `src/processo.h/.cpp` | structs `Processo` e `Burst` e leitura do JSON |
| `src/memoria.h/.cpp` | quadros, busca de página, escolha de vítima (FIFO/LRU), proteção |
| `src/simulador.h/.cpp` | loop do relógio, escalonadores, despacho, page fault |
| `src/saida.h/.cpp` | métricas, resumo, Gantt, log e CSVs |
| `include/json.hpp` | biblioteca [nlohmann/json](https://github.com/nlohmann/json) v3.11.3 (não é código nosso) |

A cada unidade de tempo, o loop principal segue a ordem de eventos da seção 6.2 do enunciado:

1. chegadas e retornos de E/S e de page fault entram na fila (em ordem de pid);
2. quem estourou o quantum vai para o fim da fila;
3. as CPUs livres pegam processo da fila (a CPU 0 primeiro);
4. cada CPU anda uma unidade (executa, troca contexto ou fica ociosa);
5. fim de burst (vai para E/S ou termina) e fim de quantum valem para o instante seguinte.

## Suposições (pontos que o enunciado deixa em aberto)

1. **Quantums:** a tabela da seção 11 cita 2, 10 e 50, mas a contagem de 28 execuções pede 5 quantums.
   Rodamos 2, 5, 10, 20 e 50.
2. **Quando o quadro é reservado:** no instante do page fault (um quadro livre ou a vítima),
   e a página já fica protegida até o processo usá-la.
3. **Sem vítima:** o processo fica bloqueado por um tempo de fault sem quadro; na volta tenta de novo.
   Quando consegue um quadro, a página é carregada (mais um tempo de fault). O fault conta uma vez.
4. **Taxa de faults:** page faults ÷ total de referências da carga.
5. **Várias CPUs estourando o quantum no mesmo instante:** tratadas em ordem de índice de CPU.
6. **Quantum:** conta só as unidades executadas (a troca de contexto não gasta quantum).
7. **Empate na escolha da vítima:** fica o quadro de menor índice.
8. **Trocas de contexto:** contam as vezes em que o custo foi cobrado (com `--custo-troca 0`, são 0).

## Autores

Trabalho em trio — Sistemas Operacionais, UNIVALI.

| Nome | GitHub |
|---|---|
| José Gabriel Santos Gomes | [@naasdd](https://github.com/naasdd) |
| Matheus Pompeo | [@mapompeo](https://github.com/mapompeo) |
| Nathan Gustavo Padilha Reichert | |
