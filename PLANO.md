# Plano — Trabalho M2 SO (Simulador de Escalonamento + Paginação)

**Entrega:** 03/11/2026 — o PDF do enunciado diz **19:00** e o AVA diz 23:59 → mirar em **19:00**.
**Defesa:** 04/11/2026 (professor senta do lado, pede pra rodar, muda parâmetros e pergunta do código).

## Critérios de avaliação (AVA)

| Peso | Critério | O que garante a nota |
|---|---|---|
| 30% | Simulador: corretude, aderência às regras, organização | Seguir a seção 6 à risca + **testes automáticos que reproduzem os exemplos do enunciado** + código legível |
| 20% | Desenho experimental: execuções, tabelas, gráficos, justificativas | As 28 execuções base + FIFO×LRU + variação de quadros, tudo gerado por script |
| 30% | Análise crítica — uso de ferramentas corretas | Cada afirmação citando execução + métrica; fenômenos (comboio, starvation, injustiça, thrashing) mostrados com números |
| 20% | Memória paginada e métricas corretas | Métricas exatamente como a seção 10.1 define + testes de FIFO/LRU/"sem vítima" |

---

## 1. Decisões técnicas

| Item | Decisão | Motivo |
|---|---|---|
| Linguagem | **C++17 puro** (sem API de SO) | Compila igual no Mac (clang) e no Windows (MinGW). Testado no Mac |
| Entrada | **JSON** com `nlohmann/json` (um único arquivo `json.hpp` dentro do repo) | O enunciado exige JSON e indica essa biblioteca. Vai dentro do repo → o professor não instala nada |
| Simulação | **Relógio discreto** (`tempo++` de 1 em 1) | Fácil de explicar; com ~42 mil unidades de CPU roda em segundos |
| Fila de prontos | `deque<int>` de índices de processo | FCFS/RR: pega da frente. SJF: percorre e pega o menor restante |
| Memória | `vector<Quadro>` de tamanho `--quadros`, busca linear | 256 posições → busca trivial. Justificativa pronta pro relatório (seção 6.5 exige) |
| Scripts (experimentos, testes, gráficos) | **Python 3** | Roda no Mac e no Windows. Precisa `pip3 install matplotlib` (só pros gráficos) |
| Estilo | `struct` + funções soltas, nomes em português, `using namespace std` | Mesmo estilo dos exercícios de ED |

---

## 2. Checklist de requisitos (enunciado inteiro, seção por seção)

### Seção 2.2 — Objetivos específicos
- [ ] 1. FCFS, SJF não preemptivo, RR com quantum variável (`--quantum`)
- [ ] 2. 1 e 4 CPUs com **uma única fila de prontos**
- [ ] 3. Memória paginada, page fault, FIFO e LRU
- [ ] 4. Custo de troca de contexto
- [ ] 5. Registrar conjuntos de processos (prontos, executando, bloqueados, terminados) → `--log`
- [ ] 6. Métricas de desempenho, justiça e uso de recursos
- [ ] 7. Gantt da janela 0–200
- [ ] 8. Investigar injustiça, starvation, efeito comboio, thrashing → relatório
- [ ] 9. Justificar com dados e teoria + limitações do modelo → relatório

### Seção 4 — Requisitos gerais
- [ ] Os 3 escalonadores no **mesmo programa**
- [ ] `--cpus 1` e `--cpus 4` com a mesma carga
- [ ] Tudo por argumento, **sem recompilar**
- [ ] Entrada JSON
- [ ] Saídas: resumo (9.1), Gantt (9.2), log opcional (9.3)
- [ ] Repositório com código, **instruções de execução** e **resultados reproduzíveis**
- [ ] Desempenho: cada execução em poucos segundos (compilar com `-O2`)

### Seção 5 — Carga
- [ ] Ler `pid, chegada, classe, paginas, bursts` (CPU com `refs`, E/S com `es`)
- [ ] `classe` **nunca** usada pelo escalonador (só nas métricas por classe)
- [ ] Nenhuma página na memória quando o processo chega

### Seção 6 — Regras do modelo (onde está a nota de "corretude")
- [ ] 6.1 Estados: Novo, Pronto, Executando (inclui a troca), Bloqueado, Terminado
- [ ] 6.1 Bloqueados **não formam fila**: cada um tem seu `tempoRetorno`
- [ ] 6.2 Quando várias CPUs estão livres, a de **menor índice** escolhe primeiro
- [ ] 6.2 O processo sai da fila no despacho (a troca de contexto não conta como espera)
- [ ] 6.2 **Ordem no mesmo instante:** (1) chegadas + retornos de E/S e de fault, em ordem de pid → (2) preempção por quantum vai pro fim da fila → (3) despachos
- [ ] 6.3 SJF: menor **restante do burst atual**; empate = quem entrou primeiro na fila; sem preempção
- [ ] 6.3 RR: no fim do quantum, se tem alguém na fila → fim da fila; senão continua **sem custo** e ganha um quantum novo
- [ ] 6.3 Volta de E/S ou de fault → entra na fila como "novo" (no fim)
- [ ] 6.3 Burst acaba antes do quantum → libera a CPU na hora
- [ ] 6.3 Fault no meio do quantum → o resto do quantum é perdido
- [ ] 6.4 Troca custa `--custo-troca` (padrão 1) quando o pid é diferente do **último que rodou naquela CPU** (inclusive no primeiro despacho)
- [ ] 6.4 Cada CPU acumula útil + troca + ocioso = **makespan**
- [ ] 6.5 `--quadros` (padrão 256), globais; página = par (pid, nº)
- [ ] 6.5 1 referência por unidade de CPU; fault → bloqueia 20 e a CPU fica livre **no mesmo instante**; a unidade não é consumida
- [ ] 6.5 Ao voltar do fault, repete a referência (agora acerto); até lá a página fica **protegida**
- [ ] 6.5 Memória cheia → vítima global entre as não protegidas; FIFO = carregada há mais tempo, LRU = referência mais antiga
- [ ] 6.5 Sem vítima → continua bloqueado e tenta de novo após 20; o fault conta **uma vez**
- [ ] 6.5 Durante a E/S as páginas ficam na memória (podem virar vítima); no término os quadros são liberados; a troca de contexto não mexe na memória

### Seção 7 — Exemplos de validação → viram **testes automáticos** (ver seção 4 deste plano)

### Seção 8 — Argumentos
- [ ] `--arquivo --cpus --sched --quantum --subst --quadros --custo-troca --log`
- [ ] Extras (necessários pra reproduzir os exemplos 7.1 e 7.2): `--custo-fault` (padrão 20) e `--sem-memoria`
- [ ] Mensagem de erro clara se um argumento estiver errado

### Seção 9 — Saídas
- [ ] 9.1 Resumo: parâmetros, métricas 10.1, tabela por CPU (útil/troca/ocioso + %), page faults + taxa
- [ ] 9.2 Gantt CSV `cpu,inicio,fim,evento` (Pn / TROCA / OCIOSA), janela 0–200, uma faixa por CPU, intervalos iguais seguidos juntados
- [ ] 9.2 Figura do Gantt legível (1 CPU e 4 CPUs) → script Python
- [ ] 9.3 Log a cada mudança: evento + pid, fila de prontos em ordem, bloqueados com instante de retorno, processo em cada CPU, quadros ocupados (+ quantos terminados)

### Seção 10 — Métricas obrigatórias
- [ ] Espera média e máxima (soma de **todos** os intervalos na fila de prontos)
- [ ] Turnaround médio (término − chegada)
- [ ] Resposta média (1º despacho − chegada)
- [ ] Makespan
- [ ] Speedup (makespan 1 CPU ÷ 4 CPUs) e eficiência (÷ 4) → calculados pelo script de experimentos
- [ ] Trocas de contexto (total e por CPU)
- [ ] Utilização e ocioso por CPU
- [ ] Page faults (total)

**Métricas extras (pra análise crítica):** espera/turnaround/resposta **por classe** (curto × longo), slowdown (turnaround ÷ tempo de serviço) por classe, page faults por classe. É isso que prova injustiça, comboio e starvation com números.

### Seção 11 — Experimentos
- [ ] Cenários A e B × 1 e 4 CPUs × {FCFS, SJF, RR q=2, 5, 10, 20, 50} = **28 execuções base** (LRU, 256 quadros)
- [ ] FIFO × LRU em pelo menos 4 configurações (vamos fazer **8**: A/B × 1/4 CPUs × FCFS e RR q=10)
- [ ] Variação de quadros (opcional → **fazemos**: 32, 64, 128, 256, 512, pra mostrar thrashing)
- [ ] Uma tabela resumo por cenário (uma linha por execução, uma coluna por métrica)
- [ ] Pelo menos 3 gráficos (vamos fazer 5: espera por escalonador, efeito do quantum, speedup, FIFO×LRU, faults×quadros)
- [ ] Os dois Gantt como figura
- [ ] Toda afirmação da análise cita a execução e a métrica

---

## 3. Interpretações dos pontos ambíguos (vão no relatório, seção "Suposições")

| # | Ponto | Nossa interpretação | Por quê |
|---|---|---|---|
| 1 | Quantums: a tabela diz "2, 10 e 50", a conta diz "5 quantums" (=28) | Rodar **2, 5, 10, 20, 50** | Cobre as duas leituras |
| 2 | Quando o quadro é reservado no fault | **No instante do fault** (quadro livre ou vítima), já protegido | "Quadro livre: a página é carregada nele" + a proteção só faz sentido se o quadro for reservado |
| 3 | "Sem vítima: tenta de novo após 20" | Bloqueia 20 **sem** quadro; na volta tenta de novo. Se conseguir quadro, aí sim carrega (+20). Conta 1 fault | É o mesmo fault repetido; a página só "chega" depois de um carregamento de 20 |
| 4 | Taxa de faults | faults ÷ total de referências da carga | Não depende das repetições pós-fault |
| 5 | Duas CPUs estourando o quantum no mesmo instante | Tratadas em ordem de índice de CPU | Mesma convenção do despacho |
| 6 | O quantum conta a troca? | Não, só unidades executadas | Quantum é tempo de execução do processo |
| 7 | Empate de vítima (FIFO/LRU) | Menor índice de quadro | Determinístico → resultados reproduzíveis |
| 8 | Contagem de trocas com `--custo-troca 0` | Não conta (0 trocas) | A seção 10.1 define como "quantas vezes o **custo** de troca foi cobrado" |

> **Sugestão pra ter certeza absoluta:** mandar um e-mail curto pro professor confirmando os itens 1 e 3, que são os únicos que mudam números de verdade. Mesmo sem resposta, eles ficam documentados como suposição.

---

## 4. Testes de validação (calculados à mão — o simulador TEM que bater)

Ficam em `testes/`, cada um com um JSON pequeno e os valores esperados. O script `testes/rodar_testes.py` roda todos e mostra OK/FALHOU.

| Teste | Config | Esperado |
|---|---|---|
| 7.1 FCFS | 1 CPU, troca 1, sem memória | espera 0/6/8, resposta 0/6/8, turnaround 6/10/11; útil 10, troca 3, ocioso 0; makespan 13 |
| 7.1 SJF | idem | ordem P2(1–4), P3(5–7), P1(8–13); espera P1=7, P2=0, P3=2; turnaround 13/4/5; makespan 13 |
| 7.1 RR q=2 | idem | espera P1=8, P2=9, P3=4; resposta 0/3/4; turnaround 16/14/7; 6 trocas; makespan 16 |
| 7.2 | 1 CPU FCFS, troca 0, fault 5, memória grande | término P1=12, P2=13, P3=8; turnaround 12/12/6; espera 0/0/0 |
| 7.3 FCFS | 1 CPU, troca 0, fault 20 | P1 20–24, P2 24–27, P3 27–29; espera 0/4/7 (média 3,67) |
| 7.3 SJF | idem | P3 20–22, P2 22–25, P1 25–29; espera 5/2/0 (média 2,33) |
| 5.5 | processo 17 sozinho, troca 0, fault 20 | termina em 441, turnaround 99, 4 faults |
| 4 CPUs | 5 processos em t=0, burst 3, troca 1 | P1–P4 nas CPUs 0–3; P5 na CPU 0 em t=4 (espera 4); makespan 8; CPU0 útil 6/troca 2/ocioso 0; CPUs 1–3 útil 3/troca 1/ocioso 4 |
| FIFO × LRU | 1 processo, refs [0,1,2,0,3,0,4], 3 quadros | FIFO = 6 faults, LRU = 5 faults |
| Sem vítima | 1 quadro, 2 processos em t=0, troca 0, fault 5 | P1 termina em 6; P2 tenta em 5 (falha), carrega em 10, termina em 16; 2 faults |

---

## 5. Estrutura do repositório

```
trabalho-m2-so/
├── README.md                ← como compilar, rodar, exemplos de comando
├── makefile                 ← make / make teste / make experimentos
├── src/
│   ├── main.cpp             ← lê os argumentos
│   ├── processo.h/.cpp      ← structs + lerArquivoJson()
│   ├── memoria.h/.cpp       ← quadros, procurar, carregar, vítima FIFO/LRU
│   ├── simulador.h/.cpp     ← loop do relógio + escalonadores
│   └── saida.h/.cpp         ← resumo, Gantt CSV, log
├── include/json.hpp         ← nlohmann/json (biblioteca, não é nosso código)
├── testes/                  ← exemplos do enunciado + rodar_testes.py
├── scripts/
│   ├── experimentos.py      ← roda todas as execuções → resultados/*.csv
│   └── graficos.py          ← gera os PNGs (gráficos + Gantt)
├── cenarios/                ← os 2 JSON do professor
├── resultados/              ← CSVs, Gantts, figuras (versionados = reproduzíveis)
└── docs/                    ← enunciado + relatório PDF
```

---

## 6. Ordem de execução

1. Esqueleto: structs, leitura do JSON, argumentos, makefile
2. Loop + FCFS + troca de contexto, **sem memória** → passar 7.1 FCFS e o teste de 4 CPUs
3. SJF e RR → passar 7.1 SJF e RR
4. Memória (fault, proteção, FIFO, LRU, sem vítima) → passar 7.2, 7.3, 5.5, FIFO×LRU, sem vítima
5. Saídas completas (resumo, Gantt, log) + métricas por classe
6. Rodar os cenários de 1000 processos, medir o tempo, conferir as invariantes (útil+troca+ocioso = makespan; espera ≥ 0; todos terminam)
7. `experimentos.py` + `graficos.py` → todos os CSVs e figuras
8. README
9. Relatório (análise com os números reais)
10. Teste final: clonar o repo numa pasta limpa, compilar e rodar do zero
