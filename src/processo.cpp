#include "processo.h"

#include <algorithm>
#include <fstream>
#include <iostream>

#include "json.hpp"
using json = nlohmann::json;

// --- LEITURA DO ARQUIVO JSON ---

// usada pelo sort para ordenar por pid
bool pidMenor(const Processo &a, const Processo &b)
{
    return a.pid < b.pid;
}

vector<Processo> lerArquivo(string caminho)
{
    ifstream arquivo(caminho);
    if (!arquivo.is_open())
    {
        cout << "Erro: nao consegui abrir o arquivo " << caminho << endl;
        exit(1);
    }

    json dados = json::parse(arquivo);
    vector<Processo> processos;

    for (auto &item : dados)
    {
        Processo processo;
        processo.pid = item["pid"];
        processo.chegada = item["chegada"];
        processo.classe = item["classe"];
        processo.paginas = item["paginas"];
        processo.tempoCpuTotal = 0;
        processo.tempoEsTotal = 0;

        for (auto &etapa : item["bursts"])
        {
            Burst burst;
            if (etapa.contains("cpu"))
            {
                burst.ehCpu = true;
                burst.duracao = etapa["cpu"];
                if (etapa.contains("refs"))
                    burst.refs = etapa["refs"].get<vector<int>>();
                processo.tempoCpuTotal += burst.duracao;
            }
            else
            {
                burst.ehCpu = false;
                burst.duracao = etapa["es"];
                processo.tempoEsTotal += burst.duracao;
            }
            processo.bursts.push_back(burst);
        }

        // estado inicial: ainda nao chegou
        processo.estado = NOVO;
        processo.burstAtual = 0;
        processo.feitoNoBurst = 0;
        processo.tempoRetorno = -1;
        processo.bloqueadoPorFault = false;
        processo.esperandoQuadro = false;
        processo.entrouNaFila = -1;
        processo.tempoEspera = 0;
        processo.primeiroDespacho = -1;
        processo.termino = -1;
        processo.pageFaults = 0;

        processos.push_back(processo);
    }

    // A regra 6.2 manda tratar eventos do mesmo instante em ordem de pid,
    // entao deixamos o vetor ordenado por pid
    sort(processos.begin(), processos.end(), pidMenor);

    return processos;
}

// --- AJUDANTES ---

int paginaAtual(Processo &processo)
{
    Burst &burst = processo.bursts[processo.burstAtual];
    return burst.refs[processo.feitoNoBurst];
}

int restanteDoBurst(Processo &processo)
{
    Burst &burst = processo.bursts[processo.burstAtual];
    return burst.duracao - processo.feitoNoBurst;
}
