#include "simulador.h"

#include <iostream>

#include "saida.h"

// ============================================================
// PREPARACAO
// ============================================================

void iniciarSimulacao(Simulacao &sim)
{
    sim.processos = lerArquivo(sim.config.arquivo);

    for (int i = 0; i < sim.config.cpus; i++)
    {
        Cpu cpu;
        cpu.processo = -1;
        cpu.ultimoPid = -1;
        cpu.trocaRestante = 0;
        cpu.quantumUsado = 0;
        cpu.executouNoTick = false;
        cpu.estourouQuantum = false;
        cpu.tempoUtil = 0;
        cpu.tempoTroca = 0;
        cpu.tempoOcioso = 0;
        cpu.numTrocas = 0;
        sim.cpus.push_back(cpu);
    }

    iniciarMemoria(sim.memoria, sim.config.quadros, sim.config.substituicao);

    for (int i = 0; i < (int)sim.processos.size(); i++)
        sim.totalReferencias += sim.processos[i].tempoCpuTotal;

    if (sim.config.gerarLog)
        sim.log.open(sim.config.arquivoLog);
}

// ============================================================
// AJUDANTES
// ============================================================

string nome(Processo &processo)
{
    return "P" + to_string(processo.pid);
}

// Guarda um evento para o log (so se --log foi pedido)
void registrarEvento(Simulacao &sim, string texto)
{
    if (!sim.config.gerarLog)
        return;
    if (sim.config.logAte != -1 && sim.tempo > sim.config.logAte)
        return;
    sim.eventosDoInstante.push_back(texto);
}

// Guarda o que a CPU fez neste tick no Gantt (so a janela de 0 a 200).
// comecaNovoTrecho = true nao deixa juntar com o trecho anterior
// (usado no inicio de cada troca: duas trocas seguidas sao para processos diferentes)
void registrarGantt(Simulacao &sim, int cpu, string evento, bool comecaNovoTrecho)
{
    if (sim.tempo >= 200)
        return;

    // se a CPU continua fazendo a mesma coisa, so estica o ultimo trecho
    for (int i = (int)sim.gantt.size() - 1; i >= 0 && !comecaNovoTrecho; i--)
    {
        if (sim.gantt[i].cpu == cpu)
        {
            if (sim.gantt[i].evento == evento && sim.gantt[i].fim == sim.tempo)
            {
                sim.gantt[i].fim = sim.tempo + 1;
                return;
            }
            break;
        }
    }

    TrechoGantt trecho;
    trecho.cpu = cpu;
    trecho.inicio = sim.tempo;
    trecho.fim = sim.tempo + 1;
    trecho.evento = evento;
    sim.gantt.push_back(trecho);
}

void colocarNaFila(Simulacao &sim, int indice)
{
    Processo &processo = sim.processos[indice];
    processo.estado = PRONTO;
    processo.entrouNaFila = sim.tempo;
    sim.filaProntos.push_back(indice);
}

// Poe a pagina atual do processo no quadro e devolve um texto para o log
string carregarNoQuadro(Simulacao &sim, int quadro, Processo &processo)
{
    string texto = " -> quadro " + to_string(quadro);

    Quadro &antigo = sim.memoria.quadros[quadro];
    if (antigo.ocupado)
        texto += " (vitima: P" + to_string(antigo.pid) + " pag " + to_string(antigo.pagina) + ")";

    carregarPagina(sim.memoria, quadro, processo.pid, paginaAtual(processo), sim.tempo);
    return texto;
}

// ============================================================
// PASSO 1: CHEGADAS E RETORNOS (em ordem de pid)
// ============================================================

// Fault que nao achou vitima: na volta tenta pegar um quadro de novo
void tentarPegarQuadroDeNovo(Simulacao &sim, int indice)
{
    Processo &processo = sim.processos[indice];
    string texto = "NOVA TENTATIVA " + nome(processo) + " pag " + to_string(paginaAtual(processo));

    int quadro = arrumarQuadro(sim.memoria);
    if (quadro == -1)
    {
        // continua sem vitima: espera mais um tempo de fault e tenta de novo
        processo.tempoRetorno = sim.tempo + sim.config.custoFault;
        texto += ": sem vitima de novo (tenta em " + to_string(processo.tempoRetorno) + ")";
    }
    else
    {
        // conseguiu quadro: agora sim a pagina vem do disco
        texto += carregarNoQuadro(sim, quadro, processo);
        processo.esperandoQuadro = false;
        processo.tempoRetorno = sim.tempo + sim.config.custoFault;
        texto += " (volta em " + to_string(processo.tempoRetorno) + ")";
    }
    registrarEvento(sim, texto);
}

void tratarChegadasERetornos(Simulacao &sim)
{
    for (int i = 0; i < (int)sim.processos.size(); i++)
    {
        Processo &processo = sim.processos[i];

        if (processo.estado == NOVO && processo.chegada == sim.tempo)
        {
            colocarNaFila(sim, i);
            registrarEvento(sim, "CHEGADA " + nome(processo));
        }
        else if (processo.estado == BLOQUEADO && processo.tempoRetorno == sim.tempo)
        {
            if (processo.esperandoQuadro)
            {
                tentarPegarQuadroDeNovo(sim, i);
            }
            else
            {
                colocarNaFila(sim, i);
                if (processo.bloqueadoPorFault)
                    registrarEvento(sim, "RETORNO DE FAULT " + nome(processo));
                else
                    registrarEvento(sim, "RETORNO DE E/S " + nome(processo));
            }
        }
    }
}

// ============================================================
// PASSO 2: FIM DO QUANTUM (so no RR)
// ============================================================

void tratarFimDoQuantum(Simulacao &sim)
{
    for (int c = 0; c < (int)sim.cpus.size(); c++)
    {
        Cpu &cpu = sim.cpus[c];
        if (!cpu.estourouQuantum)
            continue;

        cpu.estourouQuantum = false;
        Processo &processo = sim.processos[cpu.processo];

        if (sim.filaProntos.empty())
        {
            // ninguem esperando: continua na CPU, sem troca, com quantum novo
            cpu.quantumUsado = 0;
            registrarEvento(sim, "NOVO QUANTUM " + nome(processo) + " na CPU" + to_string(c) + " (fila vazia)");
        }
        else
        {
            // tem alguem esperando: vai para o fim da fila
            colocarNaFila(sim, cpu.processo);
            cpu.processo = -1;
            registrarEvento(sim, "PREEMPCAO " + nome(processo) + " sai da CPU" + to_string(c) + " (fim do quantum)");
        }
    }
}

// ============================================================
// PASSO 3: DESPACHOS
// ============================================================

// Devolve a POSICAO na fila do processo que o escalonador escolheu
int escolherDaFila(Simulacao &sim)
{
    // FCFS e RR: o primeiro da fila
    if (sim.config.escalonador != "sjf")
        return 0;

    // SJF: o que tem o menor restante do burst de CPU atual.
    // Empate: o que entrou primeiro (por isso "<" e nao "<=")
    int melhor = 0;
    for (int pos = 1; pos < (int)sim.filaProntos.size(); pos++)
    {
        Processo &candidato = sim.processos[sim.filaProntos[pos]];
        Processo &atual = sim.processos[sim.filaProntos[melhor]];
        if (restanteDoBurst(candidato) < restanteDoBurst(atual))
            melhor = pos;
    }
    return melhor;
}

void despachar(Simulacao &sim, int c)
{
    Cpu &cpu = sim.cpus[c];

    int posicao = escolherDaFila(sim);
    int indice = sim.filaProntos[posicao];
    sim.filaProntos.erase(sim.filaProntos.begin() + posicao);

    Processo &processo = sim.processos[indice];
    processo.estado = EXECUTANDO;
    processo.tempoEspera += sim.tempo - processo.entrouNaFila;
    if (processo.primeiroDespacho == -1)
        processo.primeiroDespacho = sim.tempo;

    string texto = "DESPACHO " + nome(processo) + " -> CPU" + to_string(c);

    // Convencao A: so paga troca se for um processo diferente do ultimo desta CPU.
    // A metrica conta quantas vezes o custo foi cobrado (com custo 0 nao conta)
    if (cpu.ultimoPid != processo.pid && sim.config.custoTroca > 0)
    {
        cpu.trocaRestante = sim.config.custoTroca;
        cpu.numTrocas++;
        texto += " (troca de contexto)";
    }
    else if (cpu.ultimoPid == processo.pid)
    {
        texto += " (mesmo processo, sem troca)";
    }

    cpu.ultimoPid = processo.pid;
    cpu.processo = indice;
    cpu.quantumUsado = 0;
    cpu.estourouQuantum = false;
    registrarEvento(sim, texto);
}

void tratarPageFault(Simulacao &sim, int c)
{
    Cpu &cpu = sim.cpus[c];
    Processo &processo = sim.processos[cpu.processo];

    processo.pageFaults++;
    sim.totalFaults++;

    // o processo bloqueia e a CPU fica livre na hora
    processo.estado = BLOQUEADO;
    processo.bloqueadoPorFault = true;
    processo.tempoRetorno = sim.tempo + sim.config.custoFault;
    cpu.processo = -1;

    string texto = "PAGE FAULT " + nome(processo) + " pag " + to_string(paginaAtual(processo));

    int quadro = arrumarQuadro(sim.memoria);
    if (quadro == -1)
    {
        // todos os quadros protegidos: fica bloqueado e tenta de novo depois
        processo.esperandoQuadro = true;
        texto += ": SEM VITIMA (tenta de novo em " + to_string(processo.tempoRetorno) + ")";
    }
    else
    {
        texto += carregarNoQuadro(sim, quadro, processo);
        texto += " (volta em " + to_string(processo.tempoRetorno) + ")";
    }
    registrarEvento(sim, texto);
}

// Referencia a pagina da proxima unidade. Devolve true se foi acerto
bool acessarPagina(Simulacao &sim, int c)
{
    Processo &processo = sim.processos[sim.cpus[c].processo];

    int quadro = procurarPagina(sim.memoria, processo.pid, paginaAtual(processo));
    if (quadro == -1)
        return false;

    usarPagina(sim.memoria, quadro, sim.tempo);
    return true;
}

// Deixa a CPU pronta para executar: despacha alguem se estiver livre e
// confere a memoria. Se der page fault, a CPU fica livre e tenta outro
// processo no mesmo instante (como no exemplo 7.2, em t=6)
void prepararCpu(Simulacao &sim, int c)
{
    Cpu &cpu = sim.cpus[c];

    while (true)
    {
        if (cpu.processo == -1)
        {
            if (sim.filaProntos.empty())
                return; // ninguem para executar: CPU ociosa
            despachar(sim, c);
        }

        if (cpu.trocaRestante > 0)
            return; // ainda vai trocar contexto: nao acessa memoria agora

        if (sim.config.semMemoria)
            return;

        if (acessarPagina(sim, c))
            return; // acerto: executa normalmente

        tratarPageFault(sim, c);
    }
}

void despacharProcessos(Simulacao &sim)
{
    // CPU de menor indice escolhe primeiro
    for (int c = 0; c < (int)sim.cpus.size(); c++)
        prepararCpu(sim, c);
}

// ============================================================
// PASSO 4: CADA CPU ANDA UMA UNIDADE DE TEMPO
// ============================================================

void executarUmTick(Simulacao &sim)
{
    for (int c = 0; c < (int)sim.cpus.size(); c++)
    {
        Cpu &cpu = sim.cpus[c];
        cpu.executouNoTick = false;

        if (cpu.processo == -1)
        {
            cpu.tempoOcioso++;
            registrarGantt(sim, c, "OCIOSA", false);
        }
        else if (cpu.trocaRestante > 0)
        {
            bool inicioDaTroca = (cpu.trocaRestante == sim.config.custoTroca);
            cpu.trocaRestante--;
            cpu.tempoTroca++;
            registrarGantt(sim, c, "TROCA", inicioDaTroca);
        }
        else
        {
            Processo &processo = sim.processos[cpu.processo];
            processo.feitoNoBurst++;
            cpu.tempoUtil++;
            cpu.quantumUsado++;
            cpu.executouNoTick = true;
            registrarGantt(sim, c, nome(processo), false);
        }
    }
}

// ============================================================
// PASSO 5: FIM DO TICK (vale para o instante tempo + 1)
// ============================================================

void terminarProcesso(Simulacao &sim, Processo &processo, int agora)
{
    processo.estado = TERMINADO;
    processo.termino = agora;
    sim.terminados++;
    if (!sim.config.semMemoria)
        liberarQuadrosDoProcesso(sim.memoria, processo.pid);
    registrarEvento(sim, "TERMINO " + nome(processo));
}

void finalizarTick(Simulacao &sim)
{
    int agora = sim.tempo + 1;

    for (int c = 0; c < (int)sim.cpus.size(); c++)
    {
        Cpu &cpu = sim.cpus[c];
        if (!cpu.executouNoTick)
            continue;

        Processo &processo = sim.processos[cpu.processo];

        if (restanteDoBurst(processo) == 0)
        {
            // acabou o burst de CPU: libera a CPU na hora
            cpu.processo = -1;
            processo.burstAtual++;
            processo.feitoNoBurst = 0;

            if (processo.burstAtual == (int)processo.bursts.size())
            {
                terminarProcesso(sim, processo, agora);
            }
            else
            {
                // o proximo burst e de E/S
                Burst &es = processo.bursts[processo.burstAtual];
                processo.estado = BLOQUEADO;
                processo.bloqueadoPorFault = false;
                processo.tempoRetorno = agora + es.duracao;
                processo.burstAtual++; // na volta ja esta no proximo burst de CPU
                registrarEvento(sim, "FIM DE BURST " + nome(processo) + " -> E/S ate " + to_string(processo.tempoRetorno));
            }
        }
        else if (sim.config.escalonador == "rr" && cpu.quantumUsado >= sim.config.quantum)
        {
            // decide no proximo instante (passo 2), depois das chegadas
            cpu.estourouQuantum = true;
        }
    }
}

// ============================================================
// LOOP PRINCIPAL
// ============================================================

void simular(Simulacao &sim)
{
    int total = sim.processos.size();

    while (sim.terminados < total)
    {
        tratarChegadasERetornos(sim); // 1. chegadas e retornos entram na fila (ordem de pid)
        tratarFimDoQuantum(sim);      // 2. quem estourou o quantum vai pro fim da fila
        despacharProcessos(sim);      // 3. CPUs livres pegam processo (CPU 0 primeiro)
        escreverLogDoInstante(sim);   //    registra o estado deste instante (se --log)
        executarUmTick(sim);          // 4. cada CPU anda 1 unidade de tempo
        finalizarTick(sim);           // 5. fim de burst / fim de quantum
        sim.tempo++;

        if (sim.tempo > 100000000)
        {
            cout << "Erro: a simulacao passou de 100 milhoes de unidades de tempo" << endl;
            exit(1);
        }
    }

    escreverLogDoInstante(sim); // eventos do ultimo instante (termino do ultimo processo)
}
