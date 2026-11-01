# Guia de estudos: Trabalho M2 de SO

> **Objetivo:** em mais ou menos 1 hora, sair sabendo explicar o que o trabalho pede, como o simulador funciona
> e onde cada regra está no código, para a defesa de 04/11.
>
> **Como ler:** com o código aberto do lado. Cada seção aponta o arquivo e a linha (os links funcionam no GitHub
> e no VS Code). Leia em ordem: primeiro a ideia, depois o código.

| Parte | Assunto | Tempo |
|---|---|---|
| 1 | O que o professor pediu | 5 min |
| 2 | Os conceitos, cada um ligado ao código | 10 min |
| 3 | Mapa do código | 3 min |
| 4 | Code review guiado, arquivo por arquivo | 25 min |
| 5 | Simulando o exemplo 7.2 à mão, passo a passo no código | 7 min |
| 6 | Testes, experimentos e números para decorar | 5 min |
| 7 | Perguntas prováveis da defesa | 5 min |
| 8 | Como operar o sistema no terminal | 5 min |

---

## 1. O que o professor pediu (5 min)

O enunciado está em [`docs/enunciado-m2.pdf`](enunciado-m2.pdf). Resumindo:

**Construir um simulador de SO** que recebe **1000 processos** (arquivo JSON) e simula:

- **escalonamento de CPU:** FCFS, SJF (não preemptivo) e Round Robin com quantum escolhido;
- **1 ou 4 CPUs** com **uma fila de prontos só**, compartilhada;
- **memória paginada:** cada unidade de CPU acessa uma página; se ela não está na memória dá **page fault**
  (o processo bloqueia por 20 unidades) e, com a memória cheia, sai uma vítima escolhida por **FIFO ou LRU**;
- **troca de contexto** com custo (padrão 1 unidade).

E depois **rodar experimentos e analisar** (é aí que está metade da nota).

**O que importa na defesa** (seção 8 do enunciado): o professor vai pedir para **trocar parâmetros na hora**.
Se precisar mexer no código, o requisito conta como não cumprido. No nosso simulador tudo é argumento:

```bash
./simulador --arquivo cenarios/cenario_B_espacadas.json --cpus 4 --sched rr --quantum 10 --subst lru --quadros 256 --custo-troca 1 --log
```

**Critérios de nota (AVA):** simulador correto e organizado (30%), desenho experimental (20%),
análise crítica (30%), memória paginada e métricas corretas (20%).

**Os dois arquivos de carga** (em `cenarios/`):

| Arquivo | O que tem |
|---|---|
| `cenario_A_t0.json` | 1000 processos, **todos chegando em t = 0** |
| `cenario_B_espacadas.json` | os mesmos 1000 processos, com **chegadas espalhadas** (carga de ~95% para 4 CPUs) |

Formato de cada processo:

```json
{"pid": 17, "chegada": 342, "classe": "curto", "paginas": 6,
 "bursts": [ {"cpu": 4, "refs": [0, 1, 1, 2]}, {"es": 12}, {"cpu": 3, "refs": [2, 3, 3]} ]}
```

- `cpu: 4` = precisa de 4 unidades de CPU; `refs` = a página acessada em cada uma dessas 4 unidades.
- `es: 12` = fica 12 unidades esperando E/S (não usa CPU).
- `classe` é só um rótulo para a análise. **O escalonador não pode usar** (e o nosso não usa).

> **Pegadinha dos "dois 4":** `"cpu": 4` no arquivo é a duração do burst. `--cpus 4` na linha de comando é o
> número de núcleos da máquina. Um processo nunca roda em dois núcleos ao mesmo tempo.

---

## 2. Os conceitos, cada um ligado ao código (10 min)

### Estados do processo

Todo processo está sempre em um destes estados ([processo.h:16](../src/processo.h#L16)):

| Estado | Quando | Onde muda no código |
|---|---|---|
| `NOVO` | ainda não chegou (`chegada > tempo`) | começa assim em `lerArquivo` |
| `PRONTO` | está na fila de prontos | `colocarNaFila` |
| `EXECUTANDO` | está numa CPU (**inclusive durante a troca de contexto**) | `despachar` |
| `BLOQUEADO` | em E/S ou esperando page fault | `tratarPageFault` e `finalizarTick` |
| `TERMINADO` | acabou o último burst | `terminarProcesso` |

**Os bloqueados não formam fila:** cada um tem seu próprio `tempoRetorno` e volta sozinho quando o relógio
chega lá. Não existe fila de disco.

### Escalonadores

- **FCFS:** pega o **primeiro da fila**. Problema clássico: o **efeito comboio**, quando um processo longo na
  frente faz todos os curtos esperarem.
- **SJF:** pega quem tem o **menor restante do burst de CPU atual**. Minimiza a espera média em teoria, mas
  pode deixar os longos esperando para sempre (**starvation**). Ele usa o campo `cpu` do arquivo; num SO real
  esse valor teria que ser estimado (isso está nas Limitações do relatório).
- **RR:** cada processo roda no máximo `quantum` unidades. Quando o quantum acaba, se tem alguém na fila o
  processo vai pro fim dela; se a fila está vazia, ele continua **sem pagar troca** e ganha um quantum novo.

Tudo isso está em `escolherDaFila` ([simulador.cpp:203](../src/simulador.cpp#L203)) e `tratarFimDoQuantum`
([simulador.cpp:171](../src/simulador.cpp#L171)).

### Troca de contexto (Convenção A do enunciado)

Quando uma CPU recebe um processo **diferente do último que ela executou**, ela gasta `--custo-troca`
unidades "trocando" antes de executar. Nesse tempo a CPU está ocupada, mas ninguém avança. Se for o mesmo
processo (por exemplo, voltou de E/S e ninguém usou a CPU nesse meio tempo), não paga nada.

Cada CPU soma três parcelas: **útil + troca + ocioso = makespan**. Está em `despachar`
([simulador.cpp:240](../src/simulador.cpp#L240)) e `executarUmTick` ([simulador.cpp:342](../src/simulador.cpp#L342)).

### Paginação sob demanda

- **Página** = pedaço do processo. **Quadro** = espaço na memória física que cabe uma página.
- Uma página é identificada pelo par **(pid, número)**: a página 0 do P1 é diferente da página 0 do P2.
- A memória começa **vazia**. A cada unidade de CPU o processo acessa `refs[i]`:
  - **acerto:** a página está num quadro, executa normal;
  - **page fault:** não está. O processo **bloqueia por 20**, a CPU fica **livre na hora** e a unidade de
    CPU **não é gasta**. Quando ele voltar, repete a mesma referência, e aí é acerto.
- Do fault até o processo usar a página, ela fica **protegida** (não pode ser vítima). Senão a página poderia
  ser expulsa antes de ser usada, e o processo daria fault de novo para sempre.
- **Memória cheia:** escolhe uma vítima entre as **não protegidas**, de **qualquer** processo
  (substituição global):
  - **FIFO:** a que foi **carregada** há mais tempo;
  - **LRU:** a que foi **usada** há mais tempo.
- **Sem vítima** (todas protegidas): o processo fica bloqueado e tenta de novo depois de 20.
- **Thrashing:** memória pequena demais, o sistema passa mais tempo trazendo páginas do que executando.

Tudo em [memoria.cpp](../src/memoria.cpp) e em `tratarPageFault` ([simulador.cpp:258](../src/simulador.cpp#L258)).

### Métricas (seção 10.1)

| Métrica | Fórmula | No código |
|---|---|---|
| Espera | soma de **todos** os intervalos na fila de prontos (não conta E/S, fault nem troca) | `tempoEspera += tempo - entrouNaFila` ([simulador.cpp:232](../src/simulador.cpp#L232)) |
| Turnaround | término − chegada | `calcularMetricas` ([saida.cpp:11](../src/saida.cpp#L11)) |
| Resposta | primeiro despacho − chegada | idem |
| Makespan | instante em que o último processo termina | `sim.tempo` no fim do loop |
| Speedup | makespan 1 CPU ÷ makespan 4 CPUs | `scripts/experimentos.py` (precisa de duas execuções) |
| Eficiência | speedup ÷ 4 | idem |
| Utilização | (útil + troca) ÷ makespan | `imprimirResumo` |
| Taxa de faults | faults ÷ total de referências (42.050) | `imprimirResumo` |

---

## 3. Mapa do código (3 min)

```
src/
  main.cpp        lê os argumentos (--cpus, --sched...) e chama o resto
  processo.h/cpp  structs Processo e Burst + leitura do JSON
  memoria.h/cpp   quadros, procurar página, escolher vítima (FIFO/LRU)
  simulador.h/cpp O CORAÇÃO: loop do relógio, escalonadores, page fault
  saida.h/cpp     métricas, resumo na tela, Gantt, log, CSVs
include/json.hpp  biblioteca nlohmann/json (não é nossa, só lê o JSON)
testes/           exemplos do enunciado + rodar_testes.py
scripts/          experimentos.py (roda tudo) e graficos.py (faz as figuras)
resultados/       CSVs, tabelas e figuras que vão no relatório
```

O fluxo do programa ([main.cpp:143](../src/main.cpp#L143)):

```cpp
int main(int argc, char *argv[])
{
    Simulacao sim;
    sim.config = lerArgumentos(argc, argv); // 1. le --cpus, --sched etc.

    iniciarSimulacao(sim);                  // 2. le o JSON, cria as CPUs e a memoria
    simular(sim);                           // 3. roda o relogio ate todo mundo terminar

    imprimirResumo(sim);                    // 4. mostra as metricas
    salvarGantt(sim);                       //    salva gantt.csv
    ...
}
```

---

## 4. Code review guiado (25 min)

### 4.1 As structs do processo: [processo.h](../src/processo.h)

```cpp
struct Burst
{
    bool ehCpu;       // true = burst de CPU, false = E/S
    int duracao;      // unidades de tempo (de CPU ou de E/S)
    vector<int> refs; // paginas acessadas, uma por unidade de CPU
};
```

O `Processo` ([processo.h:25](../src/processo.h#L25)) tem três grupos de campos:

1. **o que vem do arquivo:** `pid`, `chegada`, `classe`, `paginas`, `bursts`;
2. **o estado durante a simulação:**
   - `burstAtual`: em qual burst ele está;
   - `feitoNoBurst`: quantas unidades do burst de CPU atual já rodou;
   - `tempoRetorno`: quando sai do bloqueio;
   - `esperandoQuadro`: deu fault e não achou vítima;
   - `entrouNaFila`: quando entrou na fila pela última vez;
3. **as métricas:** `tempoEspera`, `primeiroDespacho`, `termino`, `pageFaults`.

Duas funções pequenas que aparecem o tempo todo ([processo.cpp:85](../src/processo.cpp#L85)):

```cpp
int paginaAtual(Processo &processo)       // pagina da PROXIMA unidade de CPU
{
    Burst &burst = processo.bursts[processo.burstAtual];
    return burst.refs[processo.feitoNoBurst];
}

int restanteDoBurst(Processo &processo)   // quanto falta do burst (usado pelo SJF)
{
    Burst &burst = processo.bursts[processo.burstAtual];
    return burst.duracao - processo.feitoNoBurst;
}
```

**Por que `feitoNoBurst` serve de índice para `refs`?** Porque a i-ésima unidade executada acessa `refs[i]`.
Se ele já fez 2 unidades, a próxima acessa `refs[2]`. E num page fault o `feitoNoBurst` **não aumenta**, então
na volta ele acessa a mesma página de novo.

### 4.2 Leitura do JSON: [processo.cpp:18](../src/processo.cpp#L18)

```cpp
json dados = json::parse(arquivo);
for (auto &item : dados)
{
    Processo processo;
    processo.pid = item["pid"];
    ...
    for (auto &etapa : item["bursts"])
    {
        if (etapa.contains("cpu")) { /* burst de CPU: le "cpu" e "refs" */ }
        else                       { /* burst de E/S: le "es" */ }
    }
}
sort(processos.begin(), processos.end(), pidMenor);
```

**Por que ordenar por pid?** A regra 6.2 diz que eventos no mesmo instante são tratados **em ordem de pid**.
Com o vetor ordenado, basta percorrer do começo ao fim.

### 4.3 A memória: [memoria.h](../src/memoria.h) e [memoria.cpp](../src/memoria.cpp)

```cpp
struct Quadro
{
    bool ocupado;
    int pid;        // dono da pagina
    int pagina;     // numero da pagina dentro do processo
    int tempoCarga; // quando foi carregada  -> FIFO
    int ultimoUso;  // ultima referencia     -> LRU
    bool protegido; // pagina de um fault que ainda nao foi usada
};
```

A memória é um `vector<Quadro>` com `--quadros` posições. **Por que essa estrutura?** (o enunciado manda
justificar no relatório): é a tradução direta das regras e é rápida o bastante. Procurar uma página custa no
máximo 256 comparações, e as ~42 mil referências da carga rodam em fração de segundo.

**A função mais importante, `arrumarQuadro`** ([memoria.cpp:36](../src/memoria.cpp#L36)):

```cpp
// 1. se tem quadro livre, usa ele
for (...) if (!memoria.quadros[i].ocupado) return i;

// 2. memoria cheia: vitima entre as NAO protegidas
for (...)
{
    if (quadro.protegido) continue;

    int tempo;
    if (memoria.politica == "fifo") tempo = quadro.tempoCarga;   // carregada ha mais tempo
    else                            tempo = quadro.ultimoUso;    // usada ha mais tempo

    if (vitima == -1 || tempo < menorTempo) { vitima = i; menorTempo = tempo; }
}
return vitima;   // -1 = todas protegidas (sem vitima)
```

**A única diferença entre FIFO e LRU é qual campo comparar.** O `<` (e não `<=`) faz o empate ficar com o
quadro de menor índice, e isso deixa os resultados sempre iguais (determinísticos).

As outras funções:

- `carregarPagina` ([memoria.cpp:75](../src/memoria.cpp#L75)): coloca a página no quadro, com
  `tempoCarga = ultimoUso = agora` e `protegido = true`;
- `usarPagina` ([memoria.cpp:89](../src/memoria.cpp#L89)): no acerto, faz `ultimoUso = agora` e
  `protegido = false`. **É isso que faz o LRU funcionar**;
- `liberarQuadrosDoProcesso`: quando o processo termina.

### 4.4 O estado da simulação: [simulador.h](../src/simulador.h)

```cpp
struct Cpu
{
    int processo;         // indice do processo (-1 = livre)
    int ultimoPid;        // ultimo despachado aqui -> decide se paga troca
    int trocaRestante;    // quantas unidades de troca ainda faltam
    int quantumUsado;     // unidades executadas desde o despacho
    bool executouNoTick;  // rodou uma unidade do processo neste tick?
    bool estourouQuantum; // quantum acabou: decide no proximo instante
    int tempoUtil, tempoTroca, tempoOcioso, numTrocas;
};
```

A `Simulacao` junta tudo:

- `config` (os argumentos);
- `processos`;
- `filaProntos`: uma `deque<int>` com os índices na ordem de entrada;
- `cpus`;
- `memoria`;
- `tempo` (o relógio);
- os contadores.

Quase toda função recebe `Simulacao &sim`.

### 4.5 O coração: o loop em [simulador.cpp:433](../src/simulador.cpp#L433)

```cpp
while (sim.terminados < total)
{
    tratarChegadasERetornos(sim); // 1. chegadas e retornos entram na fila (ordem de pid)
    tratarFimDoQuantum(sim);      // 2. quem estourou o quantum vai pro fim da fila
    despacharProcessos(sim);      // 3. CPUs livres pegam processo (CPU 0 primeiro)
    escreverLogDoInstante(sim);   //    registra o estado deste instante (se --log)
    executarUmTick(sim);          // 4. cada CPU anda 1 unidade de tempo
    finalizarTick(sim);           // 5. fim de burst / fim de quantum
    sim.tempo++;
}
```

**Decore esses 5 passos.** Os passos 1, 2 e 3 são **exatamente** a "ordem de eventos no mesmo instante" da
seção 6.2 do enunciado. Agora cada um deles.

#### Passo 1: `tratarChegadasERetornos` ([simulador.cpp:138](../src/simulador.cpp#L138))

Percorre todos os processos (já ordenados por pid):

- se é `NOVO` e `chegada == tempo`: entra na fila;
- se é `BLOQUEADO` e `tempoRetorno == tempo`:
  - se estava `esperandoQuadro` (fault sem vítima): chama `tentarPegarQuadroDeNovo`;
  - senão (voltou de E/S ou de fault): entra no **fim** da fila.

`colocarNaFila` ([simulador.cpp:89](../src/simulador.cpp#L89)) marca `entrouNaFila = tempo`, que depois vai
ser usado para calcular a espera.

#### Passo 2: `tratarFimDoQuantum` ([simulador.cpp:171](../src/simulador.cpp#L171))

```cpp
if (!cpu.estourouQuantum) continue;
cpu.estourouQuantum = false;

if (sim.filaProntos.empty())
    cpu.quantumUsado = 0;            // ninguem esperando: continua, sem troca, quantum novo
else
{
    colocarNaFila(sim, cpu.processo); // tem gente esperando: vai pro fim da fila
    cpu.processo = -1;
}
```

**Por que o quantum é decidido aqui e não na hora que acaba?** Porque o enunciado manda primeiro colocar na
fila as chegadas e os retornos daquele instante (passo 1) e **só depois** o processo preemptado (passo 2).
Então quem chegou no mesmo instante fica **na frente** dele. O teste do 7.1 com RR verifica exatamente isso:
o P3 chega em t=2 e fica na frente do P1 preemptado em t=3.

#### Passo 3: `despacharProcessos` → `prepararCpu` ([simulador.cpp:305](../src/simulador.cpp#L305))

Para cada CPU, **da 0 para a 3** (a de menor índice escolhe primeiro):

```cpp
while (true)
{
    if (cpu.processo == -1)                    // CPU livre?
    {
        if (sim.filaProntos.empty()) return;   // ninguem: fica ociosa
        despachar(sim, c);                     // pega alguem da fila
    }
    if (cpu.trocaRestante > 0) return;         // vai trocar contexto: nao acessa memoria ainda
    if (sim.config.semMemoria) return;
    if (acessarPagina(sim, c)) return;         // acerto: vai executar
    tratarPageFault(sim, c);                   // fault: CPU fica livre e o while tenta OUTRO processo
}
```

**Por que um `while`?** Porque depois de um page fault a CPU fica livre **no mesmo instante** e já pode pegar
outro processo. No exemplo 7.3 os três processos dão fault em t=0, um atrás do outro, na mesma CPU e no
mesmo instante. Sem o `while`, a CPU ficaria parada até o próximo tick.

**`despachar`** ([simulador.cpp:222](../src/simulador.cpp#L222)):

```cpp
int posicao = escolherDaFila(sim);                    // FCFS/RR: 0. SJF: o menor restante
...
processo.tempoEspera += sim.tempo - processo.entrouNaFila;   // fecha o intervalo na fila
if (processo.primeiroDespacho == -1)
    processo.primeiroDespacho = sim.tempo;            // para o tempo de resposta

if (cpu.ultimoPid != processo.pid && sim.config.custoTroca > 0)
{
    cpu.trocaRestante = sim.config.custoTroca;        // processo diferente: paga troca
    cpu.numTrocas++;
}
cpu.ultimoPid = processo.pid;
cpu.quantumUsado = 0;
```

**`escolherDaFila`** ([simulador.cpp:203](../src/simulador.cpp#L203)): no SJF percorre a fila comparando
`restanteDoBurst`. Usa `<` estrito, então no empate fica quem está mais na frente, ou seja, quem entrou
primeiro, como o enunciado pede.

**`tratarPageFault`** ([simulador.cpp:258](../src/simulador.cpp#L258)):

```cpp
processo.pageFaults++;
processo.estado = BLOQUEADO;
processo.tempoRetorno = sim.tempo + sim.config.custoFault;   // volta daqui a 20
cpu.processo = -1;                                           // CPU livre na hora

int quadro = arrumarQuadro(sim.memoria);
if (quadro == -1)
    processo.esperandoQuadro = true;                         // sem vitima: tenta de novo depois
else
    carregarNoQuadro(sim, quadro, processo);                 // reserva o quadro (ja protegido)
```

Note que `feitoNoBurst` **não muda**: a unidade não foi gasta.

#### Passo 4: `executarUmTick` ([simulador.cpp:342](../src/simulador.cpp#L342))

Cada CPU faz **uma** de três coisas:

| Situação | O que acontece | Conta como |
|---|---|---|
| `processo == -1` | nada | `tempoOcioso++`, Gantt "OCIOSA" |
| `trocaRestante > 0` | `trocaRestante--` | `tempoTroca++`, Gantt "TROCA" |
| senão | `feitoNoBurst++`, `quantumUsado++` | `tempoUtil++`, Gantt "P17" |

Como cada tick cai em exatamente uma das três linhas, **útil + troca + ocioso = makespan** sempre.

#### Passo 5: `finalizarTick` ([simulador.cpp:387](../src/simulador.cpp#L387))

Vale para o instante `tempo + 1`, porque a unidade que acabou de rodar ocupa o intervalo de `tempo` a `tempo+1`:

```cpp
if (restanteDoBurst(processo) == 0)       // acabou o burst de CPU
{
    cpu.processo = -1;                    // libera a CPU na hora
    processo.burstAtual++;
    if (acabaram os bursts) terminarProcesso(...);   // termino = tempo+1, libera os quadros
    else                                   // o proximo e E/S
    {
        processo.estado = BLOQUEADO;
        processo.tempoRetorno = agora + es.duracao;
        processo.burstAtual++;             // na volta ja aponta pro proximo burst de CPU
    }
}
else if (rr && cpu.quantumUsado >= quantum)
    cpu.estourouQuantum = true;           // decide no passo 2 do proximo instante
```

#### Gantt: `registrarGantt` ([simulador.cpp:62](../src/simulador.cpp#L62))

Só grava enquanto `tempo < 200`. Se a CPU continua fazendo a mesma coisa, estica o último trecho em vez de
criar outro. A exceção é a **troca**: duas trocas seguidas são para processos diferentes, então cada uma vira
um trecho separado (`comecaNovoTrecho`).

### 4.6 Saídas: [saida.cpp](../src/saida.cpp)

- `calcularMetricas` ([saida.cpp:11](../src/saida.cpp#L11)): soma e divide. Aceita uma classe (`"curto"`,
  `"longo"` ou `""` para todos), e é daí que vêm as métricas por classe usadas na análise de justiça.
  - **slowdown** = turnaround ÷ (CPU + E/S). Vale 1 para quem nunca esperou nada.
- `imprimirResumo`: o resumo padronizado da seção 9.1.
- `escreverLogDoInstante` ([saida.cpp:245](../src/saida.cpp#L245)): só escreve se aconteceu algum evento.
  Mostra os eventos, a fila, os bloqueados com o instante de retorno, as CPUs e os quadros ocupados.

### 4.7 Argumentos: [main.cpp:62](../src/main.cpp#L62)

Um `for` pelos argumentos com `if (arg == "--cpus")` etc. Depois valida tudo (escalonador válido, quantum ≥ 1,
...) e, se algo estiver errado, mostra a ajuda. `--log` e `--sem-memoria` não têm valor; os outros têm.

Extras que **não** estão no enunciado e por quê:

- `--custo-fault`: o exemplo 7.2 usa fault de 5;
- `--sem-memoria`: o exemplo 7.1 não tem memória;
- `--gantt`, `--log-arquivo`, `--log-ate`, `--csv`, `--processos`: onde salvar as saídas (os scripts usam).

---

## 5. Simulando o exemplo 7.2 à mão, no código (7 min)

Configuração: 1 CPU, FCFS, troca 0, fault 5. P1 chega em 0 com refs [0,1], P2 chega em 1 com refs [0,1],
P3 chega em 2 com refs [0]. Rode você mesmo e abra o `log.txt`:

```bash
./simulador --arquivo testes/ex_7_2.json --custo-troca 0 --custo-fault 5 --log
```

| t | Passo | O que o código faz |
|---|---|---|
| 0 | 1 | P1 `NOVO` com chegada 0 → `colocarNaFila` |
| 0 | 3 | CPU livre → `despachar(P1)` (troca 0, então não troca). `acessarPagina` (P1,0) → não está → `tratarPageFault`: P1 bloqueado até 5, quadro 0 reservado e protegido, CPU livre. O `while` tenta outro, mas a fila está vazia |
| 0 | 4 | CPU ociosa |
| 1, 2 | | Mesma coisa com P2 (até 6) e P3 (até 7) |
| 5 | 1 | P1 `tempoRetorno == 5` → volta pra fila |
| 5 | 3 | `despachar(P1)` → (P1,0) **acerto** (`usarPagina` tira a proteção) |
| 5 | 4 | P1 executa a 1ª unidade (`feitoNoBurst` = 1) |
| 6 | 1 | P2 volta pra fila |
| 6 | 3 | A CPU tem P1 → `acessarPagina` (P1,1) → **fault**, P1 bloqueado até 11, CPU livre → `while` → `despachar(P2)` → (P2,0) acerto |
| 6 | 4 | P2 executa |
| 7 | | P3 volta; P2 dá fault em (P2,1) até 12; P3 é despachado e executa |
| 7 | 5 | P3 acabou o burst → `terminarProcesso` com término **8** |
| 8 a 10 | | CPU ociosa (todo mundo bloqueado) |
| 11 | | P1 volta, executa, termina em **12** |
| 12 | | P2 volta, executa, termina em **13** |

Resultado: turnarounds 12, 12, 6 e espera 0 para todos. É exatamente a tabela do enunciado.

---

## 6. Testes, experimentos e números para decorar (5 min)

### Testes: `make teste`

O script [testes/rodar_testes.py](../testes/rodar_testes.py) roda 15 casos e compara com valores calculados à mão:

- os exemplos 5.4, 5.5, 7.1 (FCFS, SJF e RR q=2), 7.2 e 7.3 (FCFS e SJF);
- os nossos: 4 CPUs, FIFO × LRU, sem vítima, empate no SJF, RR com fila vazia e volta de E/S sem troca.

Se o professor perguntar "como vocês sabem que está certo?": **mostre o `make teste`**. E diga que, para ter
certeza de que os testes pegam erro, nós quebramos 6 regras de propósito e cada uma fez algum teste falhar.

### Experimentos: `make experimentos` e `make graficos`

São 112 execuções em ~15 segundos:

- as 28 base (A e B × 1 e 4 CPUs × FCFS, SJF, RR 2/5/10/20/50);
- FIFO × LRU em 8 configurações;
- variação de quadros (32 a 2048);
- variação do custo de troca (0, 1, 2, 5).

Tudo vai para `resultados/`.

### Números para ter na cabeça

| Fato | Número | Execução |
|---|---|---|
| Taxa de faults com 256 quadros e 1 CPU | ~50% (21.580 de 42.050) | A-1-FCFS |
| Tempo gasto em troca de contexto | 36,8% do makespan | A-1-FCFS |
| Faults sem vítima no cenário A | 13.842 | A-1-FCFS (log) |
| SJF tem espera **maior** mas turnaround **41% menor** que FCFS | 14.706 × 11.505 / 14.941 × 25.301 | A-1 |
| SJF sofre 2,9× menos faults que FCFS | 7.421 × 21.580 | A-1 |
| Efeito comboio: espera dos curtos FCFS × SJF | 324 × 42 | B-4 |
| Starvation: resposta dos longos × curtos no SJF | 311,85 × 7,21 (43×) | B-4-SJF |
| Speedup superlinear | 4,85 | B, FCFS |
| Speedup no cenário A | 3,85 a 3,97 | A |
| Quantum 2 → 50: trocas | 30.903 → 11.085 | B-4-RR |
| Thrashing com 32 quadros | utilização cai para 70% | A-4-RR10 |
| LRU melhor que FIFO | em 6 de 8 configurações | fifo_lru.csv |

**As três histórias que mais impressionam** (estão explicadas no relatório, seção 6):

1. **SJF "pior" na espera mas melhor no turnaround.** A espera só conta tempo na fila. No FCFS os processos
   ficam muito tempo bloqueados em faults, que não contam como espera. O SJF dá menos faults porque quem volta
   de um fault tem burst restante menor, é escolhido logo e acha as páginas ainda na memória. Ou seja, o
   escalonador muda a localidade.
2. **Speedup acima de 4.** Com 1 CPU a fila é enorme e as páginas são expulsas antes do reuso, o que gera mais
   faults e mais trocas, ou seja, **mais trabalho total**. Com 4 CPUs há menos trabalho extra. As duas
   máquinas fazem 42.050 unidades úteis, mas uma gasta 65.693 unidades ocupadas e a outra 52.828.
3. **Memória domina tudo.** Com metade das referências dando fault, o processo quase nunca roda unidades
   seguidas, e por isso o quantum do RR quase não faz diferença com 1 CPU.

---

## 7. Perguntas prováveis da defesa (5 min)

**"Roda com 4 CPUs, SJF, FIFO e 128 quadros."**

```bash
./simulador --arquivo cenarios/cenario_A_t0.json --cpus 4 --sched sjf --subst fifo --quadros 128
```

**"Mostra o log."** Para não gerar centenas de MB, use `--log --log-ate 100` e abra o `log.txt`.

**"Onde está a ordem dos eventos?"** No loop `simular` ([simulador.cpp:433](../src/simulador.cpp#L433)):
passos 1, 2 e 3.

**"Como vocês calculam a espera?"** No `despachar`: `tempoEspera += tempo - entrouNaFila`. O `entrouNaFila` é
marcado toda vez que o processo entra na fila, então soma **todos** os intervalos. Fault, E/S e troca não
contam porque o processo não está na fila nesses momentos.

**"E se a CPU 0 e a CPU 1 estiverem livres?"** O `for` de `despacharProcessos` vai da CPU 0 para a 3, então
a 0 escolhe primeiro.

**"Quando paga troca de contexto?"** Quando o processo despachado é diferente de `cpu.ultimoPid`. Se for o
mesmo (RR com fila vazia ou volta de E/S sem ninguém ter usado a CPU), não paga.

**"O que acontece no page fault?"** O processo bloqueia por 20, a unidade não é gasta (`feitoNoBurst` não
muda), a CPU fica livre na hora e tenta outro processo (o `while` do `prepararCpu`). Um quadro é reservado e
fica protegido até o processo usar a página.

**"Qual a diferença entre FIFO e LRU no código?"** Só o campo comparado em `arrumarQuadro`: `tempoCarga`
(FIFO) ou `ultimoUso` (LRU). O `ultimoUso` é atualizado a cada acerto em `usarPagina`.

**"Por que proteger a página?"** Para ela não ser expulsa entre o fault e o uso. Senão o processo voltaria,
daria fault de novo e poderia nunca andar.

**"E se todos os quadros estiverem protegidos?"** `arrumarQuadro` devolve -1, o processo fica
`esperandoQuadro` e tenta de novo depois de 20 (`tentarPegarQuadroDeNovo`). Quando conseguir, aí sim espera
mais 20 para a página chegar. O fault conta uma vez só. Essa é uma suposição nossa (o enunciado não detalha),
documentada no README e no relatório.

**"O SJF usa a classe?"** Não. Usa `restanteDoBurst`, que vem do campo `cpu`. A classe só aparece em
`calcularMetricas`, para a análise.

**"Por que busca linear na memória?"** Com 256 quadros são no máximo 256 comparações por referência, e cada
execução de 1000 processos leva menos de 0,4 s. Uma hash deixaria mais complexo sem mudar resultado.

**"Por que o RR com quantum 50 dá quase igual ao FCFS?"** Porque quase nenhum processo roda 50 unidades sem
dar fault ou acabar o burst: o quantum quase nunca estoura.

**"Como saber que útil + troca + ocioso dá o makespan?"** Cada tick de cada CPU cai em exatamente um dos três
casos do `executarUmTick`. Os testes conferem isso, e nas 28 execuções a soma bate.

**"Quais as limitações?"**

- O SJF conhece o burst (no real teria que estimar).
- O fault tem tempo fixo e não existe fila de disco.
- A troca tem custo fixo.
- A substituição é só global.
- Não há prioridade nem envelhecimento.
- Uma única carga por cenário.

---

## 8. Como operar o sistema no terminal (5 min)

### Compilar (uma vez, ou depois de mexer no código)

```bash
cd ~/Documents/GitHub/trabalho-m2-so
make
```

Isso gera o executável `simulador`. No Windows, use `mingw32-make`.

### Rodar uma simulação

Só com o arquivo, o resto fica no padrão (1 CPU, FCFS, LRU, 256 quadros, troca 1):

```bash
./simulador --arquivo cenarios/cenario_A_t0.json
```

Com tudo explícito:

```bash
./simulador --arquivo cenarios/cenario_B_espacadas.json --cpus 4 --sched rr --quantum 10 --subst lru --quadros 256 --custo-troca 1
```

| Argumento | O que muda | Valores |
|---|---|---|
| `--arquivo` | qual carga | `cenarios/cenario_A_t0.json` ou `cenarios/cenario_B_espacadas.json` |
| `--cpus` | quantos núcleos | `1` ou `4` |
| `--sched` | escalonador | `fcfs`, `sjf`, `rr` |
| `--quantum` | fatia do RR (os outros escalonadores ignoram) | ex.: `2`, `10`, `50` |
| `--subst` | substituição de página | `fifo` ou `lru` |
| `--quadros` | tamanho da memória | ex.: `32`, `256`, `1024` |
| `--custo-troca` | custo da troca de contexto | ex.: `0`, `1`, `5` |
| `--log` | gera o `log.txt` | (sem valor) |

A ordem dos argumentos não importa. Se algum estiver errado, o programa mostra o erro e a ajuda.
Para ver a ajuda: `./simulador --ajuda`.

### Ler a saída

A tela mostra 4 blocos:

- **PARAMETROS:** o que foi escolhido. Mostre isso ao professor para provar que o parâmetro mudou.
- **METRICAS GLOBAIS:** espera média e máxima, turnaround, resposta, makespan, trocas, page faults e taxa de faults.
- **POR CPU:** útil, troca e ocioso de cada CPU.
- **POR CLASSE:** curtos × longos (justiça e starvation).

O Gantt de 0 a 200 é salvo em `gantt.csv` (veja com `cat gantt.csv`).

### Log de estados

Use um exemplo pequeno, que gera um log fácil de ler:

```bash
./simulador --arquivo testes/ex_7_2.json --custo-troca 0 --custo-fault 5 --log
cat log.txt
```

Com 1000 processos, limite o log até um instante (o completo tem centenas de MB):

```bash
./simulador --arquivo cenarios/cenario_A_t0.json --log --log-ate 100
head -60 log.txt
```

### Provar que está correto

```bash
make teste
```

Tem que aparecer "15 de 15 testes passaram".

### Speedup (precisa de duas execuções)

```bash
./simulador --arquivo cenarios/cenario_B_espacadas.json --cpus 1 --sched fcfs | grep makespan
./simulador --arquivo cenarios/cenario_B_espacadas.json --cpus 4 --sched fcfs | grep makespan
```

Divida os dois makespans: 65795 ÷ 13567 = 4,85. A tabela completa está em `resultados/speedup.csv`.

### Refazer todos os experimentos

```bash
make experimentos   # ~15 s, refaz os CSVs em resultados/
make graficos       # refaz as figuras (precisa: pip3 install matplotlib)
```

### Roteiro para a defesa

1. `make` e depois `make teste`: mostra que compila e que bate com o enunciado.
2. Rode o que o professor pedir, trocando só os argumentos.
3. Se ele duvidar de alguma regra, rode o 7.2 com `--log` e mostre o `log.txt` instante a instante (seção 5 deste guia).
