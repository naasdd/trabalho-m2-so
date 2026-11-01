# Compila o simulador. No Windows (MinGW) use: mingw32-make
CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Iinclude

FONTES = src/main.cpp src/processo.cpp src/memoria.cpp src/simulador.cpp src/saida.cpp
CABECALHOS = src/processo.h src/memoria.h src/simulador.h src/saida.h

simulador: $(FONTES) $(CABECALHOS)
	$(CXX) $(CXXFLAGS) $(FONTES) -o simulador

# roda os exemplos do enunciado e confere os resultados
teste: simulador
	python3 testes/rodar_testes.py

# roda todas as execucoes da secao 11 e gera os CSVs em resultados/
experimentos: simulador
	python3 scripts/experimentos.py

# gera os graficos e as figuras do Gantt a partir dos CSVs
graficos:
	python3 scripts/graficos.py

limpar:
	rm -f simulador simulador.exe gantt.csv log.txt
