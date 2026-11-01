#include "memoria.h"

void iniciarMemoria(Memoria &memoria, int numQuadros, string politica)
{
    memoria.politica = politica;
    memoria.ocupados = 0;
    memoria.quadros.clear();

    for (int i = 0; i < numQuadros; i++)
    {
        Quadro quadro;
        quadro.ocupado = false;
        quadro.pid = -1;
        quadro.pagina = -1;
        quadro.tempoCarga = 0;
        quadro.ultimoUso = 0;
        quadro.protegido = false;
        memoria.quadros.push_back(quadro);
    }
}

// Busca linear: com 256 quadros e ~42 mil referencias isso e bem rapido
int procurarPagina(Memoria &memoria, int pid, int pagina)
{
    for (int i = 0; i < (int)memoria.quadros.size(); i++)
    {
        Quadro &quadro = memoria.quadros[i];
        if (quadro.ocupado && quadro.pid == pid && quadro.pagina == pagina)
            return i;
    }
    return -1;
}

// --- ESCOLHA DO QUADRO ---

int arrumarQuadro(Memoria &memoria)
{
    // 1. se tem quadro livre, usa ele
    for (int i = 0; i < (int)memoria.quadros.size(); i++)
    {
        if (!memoria.quadros[i].ocupado)
            return i;
    }

    // 2. memoria cheia: escolhe a vitima entre as paginas NAO protegidas
    //    FIFO = carregada ha mais tempo (menor tempoCarga)
    //    LRU  = referenciada ha mais tempo (menor ultimoUso)
    //    Empate: fica o quadro de menor indice (por isso o "<" e nao "<=")
    int vitima = -1;
    int menorTempo = 0;

    for (int i = 0; i < (int)memoria.quadros.size(); i++)
    {
        Quadro &quadro = memoria.quadros[i];
        if (quadro.protegido)
            continue;

        int tempo;
        if (memoria.politica == "fifo")
            tempo = quadro.tempoCarga;
        else
            tempo = quadro.ultimoUso;

        if (vitima == -1 || tempo < menorTempo)
        {
            vitima = i;
            menorTempo = tempo;
        }
    }

    // vitima == -1 aqui significa que todos estao protegidos
    return vitima;
}

void carregarPagina(Memoria &memoria, int indice, int pid, int pagina, int tempo)
{
    Quadro &quadro = memoria.quadros[indice];
    if (!quadro.ocupado)
        memoria.ocupados++;

    quadro.ocupado = true;
    quadro.pid = pid;
    quadro.pagina = pagina;
    quadro.tempoCarga = tempo;
    quadro.ultimoUso = tempo;
    quadro.protegido = true; // so perde a protecao quando o processo usar a pagina
}

void usarPagina(Memoria &memoria, int indice, int tempo)
{
    Quadro &quadro = memoria.quadros[indice];
    quadro.ultimoUso = tempo;
    quadro.protegido = false;
}

void liberarQuadrosDoProcesso(Memoria &memoria, int pid)
{
    for (int i = 0; i < (int)memoria.quadros.size(); i++)
    {
        Quadro &quadro = memoria.quadros[i];
        if (quadro.ocupado && quadro.pid == pid)
        {
            quadro.ocupado = false;
            quadro.pid = -1;
            quadro.pagina = -1;
            quadro.protegido = false;
            memoria.ocupados--;
        }
    }
}
