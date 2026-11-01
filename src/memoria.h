#ifndef MEMORIA_H
#define MEMORIA_H

#include <string>
#include <vector>
using namespace std;

// Um quadro da memoria fisica. Guarda so QUAL pagina esta nele (nao modela bytes)
struct Quadro
{
    bool ocupado;
    int pid;        // dono da pagina
    int pagina;     // numero da pagina dentro do processo
    int tempoCarga; // quando a pagina foi carregada (usado pelo FIFO)
    int ultimoUso;  // ultima vez que a pagina foi referenciada (usado pelo LRU)
    bool protegido; // pagina de um fault que ainda nao foi usada: nao pode ser vitima
};

struct Memoria
{
    vector<Quadro> quadros;
    string politica; // "fifo" ou "lru"
    int ocupados;    // quantos quadros estao em uso
};

void iniciarMemoria(Memoria &memoria, int numQuadros, string politica);

// Devolve o indice do quadro onde esta a pagina (pid, pagina), ou -1 se nao esta
int procurarPagina(Memoria &memoria, int pid, int pagina);

// Devolve um quadro para colocar uma pagina nova: um livre, ou a vitima
// escolhida pela politica. Devolve -1 se todos estao protegidos (sem vitima)
int arrumarQuadro(Memoria &memoria);

// Coloca a pagina no quadro (ja protegida, porque veio de um page fault)
void carregarPagina(Memoria &memoria, int indice, int pid, int pagina, int tempo);

// Marca que a pagina do quadro foi usada agora (acerto)
void usarPagina(Memoria &memoria, int indice, int tempo);

// Libera todos os quadros de um processo que terminou
void liberarQuadrosDoProcesso(Memoria &memoria, int pid);

#endif
