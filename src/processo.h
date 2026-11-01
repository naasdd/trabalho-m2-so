#ifndef PROCESSO_H
#define PROCESSO_H

#include <string>
#include <vector>
using namespace std;

// Um trecho da vida do processo: ou usa CPU, ou espera E/S
struct Burst
{
    bool ehCpu;       // true = burst de CPU, false = E/S
    int duracao;      // unidades de tempo (de CPU ou de E/S)
    vector<int> refs; // paginas acessadas, uma por unidade de CPU (so no burst de CPU)
};

enum Estado
{
    NOVO,       // ainda nao chegou
    PRONTO,     // na fila de prontos
    EXECUTANDO, // em uma CPU (inclui a troca de contexto)
    BLOQUEADO,  // em E/S ou esperando page fault
    TERMINADO
};

struct Processo
{
    // --- dados que vem do arquivo ---
    int pid;
    int chegada;
    string classe; // "curto" ou "longo" (so para as metricas, o escalonador nao usa)
    int paginas;
    vector<Burst> bursts;

    // --- estado durante a simulacao ---
    Estado estado;
    int burstAtual;         // indice do burst em que o processo esta
    int feitoNoBurst;       // quantas unidades do burst de CPU atual ja executou
    int tempoRetorno;       // instante em que sai do bloqueio
    bool bloqueadoPorFault; // true = page fault, false = E/S (so para o log)
    bool esperandoQuadro;   // page fault sem vitima: tenta pegar um quadro de novo na volta
    int entrouNaFila;       // instante em que entrou na fila de prontos pela ultima vez

    // --- metricas ---
    int tempoEspera;      // soma de todos os intervalos na fila de prontos
    int primeiroDespacho; // -1 enquanto nunca foi despachado
    int termino;
    int pageFaults;
    int tempoCpuTotal; // soma dos bursts de CPU
    int tempoEsTotal;  // soma dos bursts de E/S
};

vector<Processo> lerArquivo(string caminho);

// Pagina que o processo vai acessar na proxima unidade de CPU
int paginaAtual(Processo &processo);

// Quanto falta do burst de CPU atual (usado pelo SJF)
int restanteDoBurst(Processo &processo);

#endif
