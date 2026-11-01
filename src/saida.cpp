#include "saida.h"

#include <cstdio>
#include <fstream>
#include <iostream>

// ============================================================
// METRICAS
// ============================================================

Metricas calcularMetricas(Simulacao &sim, string classe)
{
    Metricas m;
    m.quantidade = 0;
    m.esperaMaxima = 0;
    m.pageFaults = 0;

    double somaEspera = 0, somaTurnaround = 0, somaResposta = 0, somaSlowdown = 0;

    for (int i = 0; i < (int)sim.processos.size(); i++)
    {
        Processo &p = sim.processos[i];
        if (classe != "" && p.classe != classe)
            continue;

        int turnaround = p.termino - p.chegada;
        int resposta = p.primeiroDespacho - p.chegada;

        m.quantidade++;
        somaEspera += p.tempoEspera;
        somaTurnaround += turnaround;
        somaResposta += resposta;
        somaSlowdown += (double)turnaround / (p.tempoCpuTotal + p.tempoEsTotal);
        m.pageFaults += p.pageFaults;
        if (p.tempoEspera > m.esperaMaxima)
            m.esperaMaxima = p.tempoEspera;
    }

    if (m.quantidade == 0)
    {
        m.esperaMedia = m.turnaroundMedio = m.respostaMedia = m.slowdownMedio = 0;
        return m;
    }

    m.esperaMedia = somaEspera / m.quantidade;
    m.turnaroundMedio = somaTurnaround / m.quantidade;
    m.respostaMedia = somaResposta / m.quantidade;
    m.slowdownMedio = somaSlowdown / m.quantidade;
    return m;
}

int totalDeTrocas(Simulacao &sim)
{
    int total = 0;
    for (int c = 0; c < (int)sim.cpus.size(); c++)
        total += sim.cpus[c].numTrocas;
    return total;
}

double porcentagem(double parte, double todo)
{
    if (todo == 0)
        return 0;
    return 100.0 * parte / todo;
}

// ============================================================
// RESUMO PADRONIZADO (secao 9.1)
// ============================================================

void imprimirResumo(Simulacao &sim)
{
    Configuracao &cfg = sim.config;
    int makespan = sim.tempo;
    Metricas todos = calcularMetricas(sim, "");

    printf("\n============== PARAMETROS ==============\n");
    printf("arquivo ........... %s\n", cfg.arquivo.c_str());
    printf("cpus .............. %d\n", cfg.cpus);
    if (cfg.escalonador == "rr")
        printf("escalonador ....... rr (quantum %d)\n", cfg.quantum);
    else
        printf("escalonador ....... %s\n", cfg.escalonador.c_str());
    if (cfg.semMemoria)
        printf("memoria ........... desligada (--sem-memoria)\n");
    else
        printf("memoria ........... %s, %d quadros, fault = %d\n",
               cfg.substituicao.c_str(), cfg.quadros, cfg.custoFault);
    printf("custo de troca .... %d\n", cfg.custoTroca);

    printf("\n============== METRICAS GLOBAIS ==============\n");
    printf("processos ......... %d\n", todos.quantidade);
    printf("espera media ...... %.2f\n", todos.esperaMedia);
    printf("espera maxima ..... %d\n", todos.esperaMaxima);
    printf("turnaround medio .. %.2f\n", todos.turnaroundMedio);
    printf("resposta media .... %.2f\n", todos.respostaMedia);
    printf("makespan .......... %d\n", makespan);
    printf("trocas de contexto  %d\n", totalDeTrocas(sim));
    printf("page faults ....... %d\n", sim.totalFaults);
    printf("taxa de faults .... %.2f%% (%d faults / %d referencias)\n",
           porcentagem(sim.totalFaults, sim.totalReferencias), sim.totalFaults, sim.totalReferencias);

    printf("\n============== POR CPU ==============\n");
    printf("CPU |   util      (%%)   |  troca     (%%)   |  ocioso     (%%)   | utilizacao | trocas\n");
    for (int c = 0; c < (int)sim.cpus.size(); c++)
    {
        Cpu &cpu = sim.cpus[c];
        printf("%3d | %7d (%6.2f%%) | %6d (%6.2f%%) | %7d (%6.2f%%) |  %7.2f%%  | %6d\n",
               c,
               cpu.tempoUtil, porcentagem(cpu.tempoUtil, makespan),
               cpu.tempoTroca, porcentagem(cpu.tempoTroca, makespan),
               cpu.tempoOcioso, porcentagem(cpu.tempoOcioso, makespan),
               porcentagem(cpu.tempoUtil + cpu.tempoTroca, makespan),
               cpu.numTrocas);
    }

    printf("\n============== POR CLASSE (para analise de justica) ==============\n");
    printf("classe | qtd  | espera media | espera max | turnaround medio | resposta media | slowdown | faults\n");
    string classes[2] = {"curto", "longo"};
    for (int i = 0; i < 2; i++)
    {
        Metricas m = calcularMetricas(sim, classes[i]);
        if (m.quantidade == 0)
            continue;
        printf("%-6s | %4d | %12.2f | %10d | %16.2f | %14.2f | %8.2f | %6d\n",
               classes[i].c_str(), m.quantidade, m.esperaMedia, m.esperaMaxima,
               m.turnaroundMedio, m.respostaMedia, m.slowdownMedio, m.pageFaults);
    }

    printf("\nGantt (0 a 200) salvo em %s\n", cfg.arquivoGantt.c_str());
    if (cfg.gerarLog)
        printf("Log de estados salvo em %s\n", cfg.arquivoLog.c_str());
}

// ============================================================
// GANTT (secao 9.2)
// ============================================================

void salvarGantt(Simulacao &sim)
{
    ofstream arquivo(sim.config.arquivoGantt);
    arquivo << "cpu,inicio,fim,evento\n";

    // uma faixa por CPU: primeiro todos os trechos da CPU 0, depois da 1...
    for (int c = 0; c < (int)sim.cpus.size(); c++)
    {
        for (int i = 0; i < (int)sim.gantt.size(); i++)
        {
            TrechoGantt &t = sim.gantt[i];
            if (t.cpu == c)
                arquivo << t.cpu << "," << t.inicio << "," << t.fim << "," << t.evento << "\n";
        }
    }
}

// ============================================================
// LINHA DE CSV (usada pelo script de experimentos)
// ============================================================

void salvarLinhaCsv(Simulacao &sim)
{
    Configuracao &cfg = sim.config;

    // se o arquivo ainda nao existe, escreve o cabecalho primeiro
    ifstream teste(cfg.arquivoCsv);
    bool arquivoNovo = !teste.good();
    teste.close();

    ofstream csv(cfg.arquivoCsv, ios::app);
    if (arquivoNovo)
    {
        csv << "arquivo,cpus,escalonador,quantum,substituicao,quadros,custo_troca,"
            << "espera_media,espera_max,turnaround_medio,resposta_media,makespan,"
            << "trocas,page_faults,taxa_faults,utilizacao_media,"
            << "util_por_cpu,troca_por_cpu,ocioso_por_cpu,trocas_por_cpu,"
            << "espera_media_curto,espera_max_curto,turnaround_curto,resposta_curto,slowdown_curto,faults_curto,"
            << "espera_media_longo,espera_max_longo,turnaround_longo,resposta_longo,slowdown_longo,faults_longo\n";
    }

    int makespan = sim.tempo;
    Metricas todos = calcularMetricas(sim, "");
    Metricas curto = calcularMetricas(sim, "curto");
    Metricas longo = calcularMetricas(sim, "longo");

    double somaUtilizacao = 0;
    string util = "", troca = "", ocioso = "", trocas = "";
    for (int c = 0; c < (int)sim.cpus.size(); c++)
    {
        Cpu &cpu = sim.cpus[c];
        somaUtilizacao += porcentagem(cpu.tempoUtil + cpu.tempoTroca, makespan);
        string separador = (c == 0) ? "" : "/";
        util += separador + to_string(cpu.tempoUtil);
        troca += separador + to_string(cpu.tempoTroca);
        ocioso += separador + to_string(cpu.tempoOcioso);
        trocas += separador + to_string(cpu.numTrocas);
    }

    string quantum = (cfg.escalonador == "rr") ? to_string(cfg.quantum) : "-";

    char linha[2000];
    snprintf(linha, sizeof(linha),
             "%s,%d,%s,%s,%s,%d,%d,"
             "%.2f,%d,%.2f,%.2f,%d,"
             "%d,%d,%.4f,%.2f,"
             "%s,%s,%s,%s,"
             "%.2f,%d,%.2f,%.2f,%.2f,%d,"
             "%.2f,%d,%.2f,%.2f,%.2f,%d\n",
             cfg.arquivo.c_str(), cfg.cpus, cfg.escalonador.c_str(), quantum.c_str(),
             cfg.substituicao.c_str(), cfg.quadros, cfg.custoTroca,
             todos.esperaMedia, todos.esperaMaxima, todos.turnaroundMedio, todos.respostaMedia, makespan,
             totalDeTrocas(sim), sim.totalFaults, porcentagem(sim.totalFaults, sim.totalReferencias),
             somaUtilizacao / sim.cpus.size(),
             util.c_str(), troca.c_str(), ocioso.c_str(), trocas.c_str(),
             curto.esperaMedia, curto.esperaMaxima, curto.turnaroundMedio, curto.respostaMedia, curto.slowdownMedio, curto.pageFaults,
             longo.esperaMedia, longo.esperaMaxima, longo.turnaroundMedio, longo.respostaMedia, longo.slowdownMedio, longo.pageFaults);
    csv << linha;
}

// ============================================================
// METRICAS DE CADA PROCESSO (usado nos testes e na analise)
// ============================================================

void salvarProcessos(Simulacao &sim)
{
    ofstream arquivo(sim.config.arquivoProcessos);
    arquivo << "pid,classe,chegada,primeiro_despacho,termino,espera,resposta,turnaround,cpu,es,page_faults\n";

    for (int i = 0; i < (int)sim.processos.size(); i++)
    {
        Processo &p = sim.processos[i];
        arquivo << p.pid << "," << p.classe << "," << p.chegada << ","
                << p.primeiroDespacho << "," << p.termino << ","
                << p.tempoEspera << "," << (p.primeiroDespacho - p.chegada) << ","
                << (p.termino - p.chegada) << ","
                << p.tempoCpuTotal << "," << p.tempoEsTotal << "," << p.pageFaults << "\n";
    }
}

// ============================================================
// LOG DE ESTADOS (secao 9.3)
// ============================================================

// Escreve o que aconteceu no instante atual e como ficaram os conjuntos.
// So escreve se algo mudou (se tem evento guardado)
void escreverLogDoInstante(Simulacao &sim)
{
    if (sim.eventosDoInstante.empty())
        return;

    ofstream &log = sim.log;
    log << "t=" << sim.tempo << "\n";

    for (int i = 0; i < (int)sim.eventosDoInstante.size(); i++)
        log << "  evento: " << sim.eventosDoInstante[i] << "\n";
    sim.eventosDoInstante.clear();

    log << "  prontos:    [";
    for (int i = 0; i < (int)sim.filaProntos.size(); i++)
    {
        if (i > 0)
            log << " ";
        log << "P" << sim.processos[sim.filaProntos[i]].pid;
    }
    log << "]\n";

    log << "  bloqueados: {";
    bool primeiro = true;
    for (int i = 0; i < (int)sim.processos.size(); i++)
    {
        Processo &p = sim.processos[i];
        if (p.estado != BLOQUEADO)
            continue;
        if (!primeiro)
            log << " ";
        log << "P" << p.pid << "(" << (p.bloqueadoPorFault ? "fault" : "e/s") << " ate " << p.tempoRetorno << ")";
        primeiro = false;
    }
    log << "}\n";

    log << "  cpus:      ";
    for (int c = 0; c < (int)sim.cpus.size(); c++)
    {
        Cpu &cpu = sim.cpus[c];
        log << " CPU" << c << "=";
        if (cpu.processo == -1)
            log << "livre";
        else
        {
            log << "P" << sim.processos[cpu.processo].pid;
            if (cpu.trocaRestante > 0)
                log << "(troca)";
        }
    }
    log << "\n";

    log << "  quadros ocupados: " << sim.memoria.ocupados << "/" << sim.config.quadros
        << " | terminados: " << sim.terminados << "/" << sim.processos.size() << "\n";
}
