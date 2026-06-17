#ifndef ARVORE_B_MAIS_H
#define ARVORE_B_MAIS_H

#include "structs.h"

typedef struct {
    int quantidade;
    int ids[100];
} ListaLivres;

/* Inicializa a árvore com o grau mínimo t */
void inicializar_arvore(int t);

/* Inserções */
void inserir_pessoa(Pessoa nova_pessoa);
void inserir_filme(Filme novo_filme);

/* Buscas */
Registro *buscar_por_nome(const char *nome);
Registro *buscar_por_id(int id);

/* Impressão em formato de árvore */
void imprimir_arvore(void);

/* Remoções */
void remover_registro(const char *nome);
void remover_por_id(int id);

/* Varredura completa das folhas (para consultas analíticas).
 * Chama callback(reg) para cada Registro encontrado. */
void varredura_folhas(void (*callback)(const Registro *));

#endif /* ARVORE_B_MAIS_H */
