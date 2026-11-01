#ifndef SAIDA_H
#define SAIDA_H

#include "simulador.h"

// Metricas de um grupo de processos (todos, so os curtos ou so os longos)
struct Metricas
{
    int quantidade;
    double esperaMedia;
    int esperaMaxima;
    double turnaroundMedio;
    double respostaMedia;
    double slowdownMedio; // turnaround / (CPU + E/S): 1.0 = nunca esperou nada
    int pageFaults;
};

// classe = "" calcula para todos os processos
Metricas calcularMetricas(Simulacao &sim, string classe);

void imprimirResumo(Simulacao &sim);
void salvarGantt(Simulacao &sim);
void salvarLinhaCsv(Simulacao &sim);
void salvarProcessos(Simulacao &sim);
void escreverLogDoInstante(Simulacao &sim);

#endif
