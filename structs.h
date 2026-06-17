#ifndef STRUCTS_H
#define STRUCTS_H

#define NOME_MAX 100
#define MAX_ALTURA 64  /* log_t(N) nunca passa disso na prática */

/* ================= ENTIDADES ================= */
typedef struct {
    int  id_pessoa;
    char nome[NOME_MAX];
    int  ano_nascimento;
} Pessoa;

typedef struct {
    int  id_filme;
    char titulo[NOME_MAX];
    int  ano_lancamento;
} Filme;

typedef struct {
    int  id_pessoa;
    int  id_filme;
    char papel;                 /* 'A' Ator, 'D' Diretor, 'P' Produtor, 'W' Escritor, 'O' Outros */
    char nome_pessoa[NOME_MAX]; /* cache para evitar I/O extra nas consultas */
    char titulo_filme[NOME_MAX];
} Relacionamento;

typedef enum { TIPO_PESSOA, TIPO_FILME } TipoRegistro;

typedef struct {
    TipoRegistro tipo;
    char chave[NOME_MAX];   /* nome da pessoa OU título do filme */
    union {
        Pessoa pessoa;
        Filme  filme;
    } dados;
} Registro;

/* ================= NÓS DA ÁRVORE B+ ================= */
/*
 * grau mínimo t (escolhido pelo usuário):
 *   - nó interno: t-1  ≤ num_chaves ≤ 2t-1  (raiz: 1 ≤ num_chaves ≤ 2t-1)
 *   - folha     : t-1  ≤ num_registros ≤ 2t-1
 * Arrays alocados dinamicamente; serializados em disco com tamanho fixo por nó.
 */
typedef struct {
    int   num_chaves;
    char **chaves;   /* [2t-1] strings de NOME_MAX chars */
    int  *filhos;    /* [2t]   IDs de filhos (nó interno) ou de folhas (nivel folha) */
    int   eh_folha;  /* 1 se filhos apontam para NoFolha, 0 se para NoIndice */
} NoIndice;

typedef struct {
    int       num_registros;
    Registro *registros;         /* [2t-1] */
    int       id_proxima_folha;  /* lista encadeada de folhas */
} NoFolha;

#endif /* STRUCTS_H */
