#ifndef SIMULADOR_H
#define SIMULADOR_H

#include <deque>
#include <fstream>
#include <string>
#include <vector>

#include "memoria.h"
#include "processo.h"
using namespace std;

// Tudo que vem da linha de comando
struct Configuracao
{
    string arquivo;
    int cpus = 1;
    string escalonador = "fcfs"; // fcfs, sjf ou rr
    int quantum = 10;            // so usado no rr
    string substituicao = "lru"; // fifo ou lru
    int quadros = 256;
    int custoTroca = 1;
    int custoFault = 20;
    bool semMemoria = false; // ignora paginacao (para reproduzir o exemplo 7.1)
    bool gerarLog = false;
    int logAte = -1; // so registra o log ate esse instante (-1 = ate o fim)
    string arquivoLog = "log.txt";
    string arquivoGantt = "gantt.csv";
    string arquivoCsv = "";       // se preenchido, acrescenta uma linha com as metricas
    string arquivoProcessos = ""; // se preenchido, salva as metricas de cada processo
};

struct Cpu
{
    int processo;         // indice do processo no vetor (-1 = livre)
    int ultimoPid;        // ultimo processo despachado nesta CPU (-1 = nenhum)
    int trocaRestante;    // unidades de troca de contexto que ainda faltam
    int quantumUsado;     // unidades executadas desde o despacho
    bool executouNoTick;  // se rodou uma unidade do processo neste tick
    bool estourouQuantum; // quantum acabou: decide no proximo instante se sai da CPU

    // as tres parcelas: util + troca + ocioso = makespan
    int tempoUtil;
    int tempoTroca;
    int tempoOcioso;
    int numTrocas;
};

// Um pedaco do diagrama de Gantt
struct TrechoGantt
{
    int cpu;
    int inicio;
    int fim;
    string evento; // "P17", "TROCA" ou "OCIOSA"
};

struct Simulacao
{
    Configuracao config;
    vector<Processo> processos;
    deque<int> filaProntos; // indices dos processos, na ordem de entrada
    vector<Cpu> cpus;
    Memoria memoria;

    int tempo = 0;
    int terminados = 0;
    int totalFaults = 0;
    int totalReferencias = 0; // soma de todas as refs da carga (para a taxa de faults)

    vector<TrechoGantt> gantt;
    ofstream log;
    vector<string> eventosDoInstante; // eventos que ainda nao foram para o log
    int instanteDosEventos = 0;
};

void iniciarSimulacao(Simulacao &sim);
void simular(Simulacao &sim);

#endif
