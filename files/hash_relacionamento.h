#ifndef HASH_RELACIONAMENTOS_H
#define HASH_RELACIONAMENTOS_H

#include "structs.h"

#define TAM_HASH     101
#define MAX_RELS_BLOCO 20

typedef struct {
    int          id_filme;              /* -1 se bloco livre */
    int          num_relacionamentos;
    Relacionamento rels[MAX_RELS_BLOCO];
    int          proximo_bloco;         /* -1 = fim da cadeia */
} BlocoHash;

void inicializar_arquivo_hash(void);
void inserir_relacionamento(Relacionamento rel);

/* Busca direta */
void buscar_pessoas_do_filme(int id_filme);
void buscar_filmes_da_pessoa(int id_pessoa);

/* Consultas analíticas — itens do enunciado */
void consulta_a_todos_juntos(void);                 /* (a) todos que trabalharam juntos */
void consulta_b_atores_diretores(void);             /* (b) atores e diretores juntos */
void consulta_c_atores_juntos(void);                /* (c) atores que atuaram juntos */
void consulta_d_atores_juntos_decada(void);         /* (d) atores juntos por década */
void consulta_e_ator_diretor_decada(void);          /* (e) ator+diretor por década */
void consulta_f_atores_mais_atuaram(void);          /* (f) atores que mais atuaram */
void consulta_g_atores_menos_atuaram(void);         /* (g) atores que menos atuaram */
void consulta_h_diretores_mais(void);               /* (h) diretores que mais dirigiram */
void consulta_i_diretores_menos(void);              /* (i) diretores que menos dirigiram */
void consulta_j_produtores_mais(void);              /* (j) produtores mais atuantes */
void consulta_k_produtores_menos(void);             /* (k) produtores menos atuantes */
void consulta_l_f_a_k_por_decada(void);             /* (l) f–k por década */
void consulta_m_filmes_continuacao(void);           /* (m) filmes que são continuações */
void consulta_n_atores_mesmo_ano(void);             /* (n) atores nascidos no mesmo ano */
void consulta_o_atores_que_dirigiram(void);         /* (o) atores que já dirigiram */
void consulta_p_atores_que_produziram(void);        /* (p) atores que já produziram */
void consulta_q_retirar_todos_do_filme(int id_filme); /* (q) remover todos de um filme */
void consulta_r_escreveu_dirigiu_produziu(void);    /* (r) escreveu+dirigiu+produziu */
void consulta_s_dirigiu_e_produziu(void);           /* (s) dirigiu e produziu */
void consulta_t_ator_nasceu_no_ano_do_filme(void);  /* (t) ator nasceu no ano do filme */

#endif /* HASH_RELACIONAMENTOS_H */

void limpar_relacionamentos_pessoa(int id_pessoa);
void limpar_relacionamentos_filme(int id_filme);
