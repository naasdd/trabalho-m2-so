// Simulador de escalonamento multiprocessado com paginacao sob demanda
// Trabalho M2 - Sistemas Operacionais - UNIVALI
//
// Exemplo:
//   ./simulador --arquivo cenarios/cenario_A_t0.json --cpus 4 --sched rr --quantum 10 --subst lru

#include <iostream>
#include <string>

#include "saida.h"
#include "simulador.h"
using namespace std;

void mostrarAjuda()
{
    cout << "Uso: ./simulador --arquivo <carga.json> [opcoes]\n\n"
         << "  --arquivo <caminho>    arquivo JSON com os processos (obrigatorio)\n"
         << "  --cpus <n>             numero de CPUs (padrao 1)\n"
         << "  --sched <nome>         fcfs, sjf ou rr (padrao fcfs)\n"
         << "  --quantum <n>          quantum do rr (padrao 10)\n"
         << "  --subst <nome>         fifo ou lru (padrao lru)\n"
         << "  --quadros <n>          quadros de memoria fisica (padrao 256)\n"
         << "  --custo-troca <n>      custo da troca de contexto (padrao 1)\n"
         << "  --log                  gera o log de estados (secao 9.3)\n"
         << "\n Extras:\n"
         << "  --custo-fault <n>      tempo de um page fault (padrao 20)\n"
         << "  --sem-memoria          desliga a paginacao (exemplo 7.1)\n"
         << "  --log-arquivo <nome>   onde salvar o log (padrao log.txt)\n"
         << "  --log-ate <t>          so registra o log ate o instante t\n"
         << "  --gantt <nome>         onde salvar o Gantt (padrao gantt.csv)\n"
         << "  --csv <nome>           acrescenta uma linha com as metricas nesse CSV\n"
         << "  --processos <nome>     salva as metricas de cada processo nesse CSV\n";
}

void erro(string mensagem)
{
    cout << "Erro: " << mensagem << "\n\n";
    mostrarAjuda();
    exit(1);
}

// Converte o texto para inteiro, ou para o programa com uma mensagem clara
int lerInteiro(string texto, string argumento)
{
    try
    {
        size_t usados = 0;
        int valor = stoi(texto, &usados);
        if (usados != texto.size())
            erro(argumento + " precisa ser um numero inteiro");
        return valor;
    }
    catch (...)
    {
        erro(argumento + " precisa ser um numero inteiro");
    }
    return 0;
}

// --- LEITURA DOS ARGUMENTOS ---

Configuracao lerArgumentos(int argc, char *argv[])
{
    Configuracao cfg;

    for (int i = 1; i < argc; i++)
    {
        string arg = argv[i];

        // argumentos sem valor
        if (arg == "--log")
        {
            cfg.gerarLog = true;
            continue;
        }
        if (arg == "--sem-memoria")
        {
            cfg.semMemoria = true;
            continue;
        }
        if (arg == "--ajuda" || arg == "--help")
        {
            mostrarAjuda();
            exit(0);
        }

        // daqui pra baixo todos precisam de um valor depois
        if (i + 1 >= argc)
            erro("faltou o valor de " + arg);
        string valor = argv[++i];

        if (arg == "--arquivo")
            cfg.arquivo = valor;
        else if (arg == "--cpus")
            cfg.cpus = lerInteiro(valor, arg);
        else if (arg == "--sched")
            cfg.escalonador = valor;
        else if (arg == "--quantum")
            cfg.quantum = lerInteiro(valor, arg);
        else if (arg == "--subst")
            cfg.substituicao = valor;
        else if (arg == "--quadros")
            cfg.quadros = lerInteiro(valor, arg);
        else if (arg == "--custo-troca")
            cfg.custoTroca = lerInteiro(valor, arg);
        else if (arg == "--custo-fault")
            cfg.custoFault = lerInteiro(valor, arg);
        else if (arg == "--log-arquivo")
            cfg.arquivoLog = valor;
        else if (arg == "--log-ate")
            cfg.logAte = lerInteiro(valor, arg);
        else if (arg == "--gantt")
            cfg.arquivoGantt = valor;
        else if (arg == "--csv")
            cfg.arquivoCsv = valor;
        else if (arg == "--processos")
            cfg.arquivoProcessos = valor;
        else
            erro("argumento desconhecido: " + arg);
    }

    // --- VALIDACAO ---
    if (cfg.arquivo == "")
        erro("faltou --arquivo");
    if (cfg.cpus < 1)
        erro("--cpus precisa ser pelo menos 1");
    if (cfg.escalonador != "fcfs" && cfg.escalonador != "sjf" && cfg.escalonador != "rr")
        erro("--sched precisa ser fcfs, sjf ou rr");
    if (cfg.quantum < 1)
        erro("--quantum precisa ser pelo menos 1");
    if (cfg.substituicao != "fifo" && cfg.substituicao != "lru")
        erro("--subst precisa ser fifo ou lru");
    if (cfg.quadros < 1)
        erro("--quadros precisa ser pelo menos 1");
    if (cfg.custoTroca < 0)
        erro("--custo-troca nao pode ser negativo");
    if (cfg.custoFault < 1)
        erro("--custo-fault precisa ser pelo menos 1");

    return cfg;
}

int main(int argc, char *argv[])
{
    Simulacao sim;
    sim.config = lerArgumentos(argc, argv);

    iniciarSimulacao(sim);
    simular(sim);

    imprimirResumo(sim);
    salvarGantt(sim);
    if (sim.config.arquivoCsv != "")
        salvarLinhaCsv(sim);
    if (sim.config.arquivoProcessos != "")
        salvarProcessos(sim);

    return 0;
}
