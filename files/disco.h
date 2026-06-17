#ifndef DISCO_H
#define DISCO_H

#include "structs.h"

/* Inicializa arquivos (cabeçalhos) */
void inicializar_disco(int t);

/* Cabeçalho */
int  ler_grau_t(void);
int  ler_raiz_id(void);
void salvar_raiz_id(int raiz_id);
int  ler_proximo_id_global(void);
void salvar_proximo_id_global(int id);

/* IDs de novos nós */
int gerar_novo_id_indice(void);
int gerar_novo_id_folha(void);

/* Tamanho fixo de um nó índice serializado (para calcular offsets) */
long tamanho_no_indice_disco(int t);

/* Nós internos (arquivo único indices.bin) */
void      salvar_no_indice(int id_no, NoIndice *no, int t);
NoIndice *ler_no_indice(int id_no, int t);
void      liberar_no_indice(NoIndice *no, int t);

/* Folhas (arquivo separado por folha: folha_N.bin) */
void    salvar_folha(int id_folha, NoFolha *folha, int t);
NoFolha*ler_folha(int id_folha, int t);
void    liberar_folha(NoFolha *folha);

#endif /* DISCO_H */
