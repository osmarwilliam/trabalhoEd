#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hash_relacionamento.h"
#include "arvoreBmais.h"

#define ARQ_HASH "dados/hash_rels.bin"

/* -------------------------------------------------------
 * Layout do arquivo:
 *   [TAM_HASH blocos de BlocoHash]  -- tabela primária
 *   [int prox_id_overflow]          -- contador de overflow
 *   [blocos de overflow sequenciais]
 * ------------------------------------------------------- */

static long calcular_offset(int pos) {
    if (pos < TAM_HASH)
        return (long)pos * sizeof(BlocoHash);
    return (long)TAM_HASH * sizeof(BlocoHash)
           + sizeof(int)
           + (long)(pos - TAM_HASH) * sizeof(BlocoHash);
}

static int calcular_hash(int id_filme) {
    return ((id_filme % TAM_HASH) + TAM_HASH) % TAM_HASH;
}

void inicializar_arquivo_hash(void) {
    FILE *f = fopen(ARQ_HASH, "rb");
    if (f) { fclose(f); return; }

    f = fopen(ARQ_HASH, "wb");
    if (!f) { perror("inicializar_arquivo_hash"); return; }

    BlocoHash vazio;
    vazio.id_filme            = -1;
    vazio.num_relacionamentos = 0;
    vazio.proximo_bloco       = -1;
    memset(vazio.rels, 0, sizeof(vazio.rels));

    for (int i = 0; i < TAM_HASH; i++)
        fwrite(&vazio, sizeof(BlocoHash), 1, f);

    int prox = TAM_HASH;
    fwrite(&prox, sizeof(int), 1, f);
    fclose(f);
}

/* ---- lê/escreve bloco na posição pos ---- */
static int ler_bloco(FILE *f, int pos, BlocoHash *b) {
    if (fseek(f, calcular_offset(pos), SEEK_SET) != 0) return 0;
    return fread(b, sizeof(BlocoHash), 1, f) == 1;
}
static int escrever_bloco(FILE *f, int pos, const BlocoHash *b) {
    if (fseek(f, calcular_offset(pos), SEEK_SET) != 0) return 0;
    return fwrite(b, sizeof(BlocoHash), 1, f) == 1;
}
static int ler_prox_overflow(FILE *f) {
    int v = TAM_HASH;
    fseek(f, (long)TAM_HASH * sizeof(BlocoHash), SEEK_SET);
    fread(&v, sizeof(int), 1, f);
    return v;
}
static void salvar_prox_overflow(FILE *f, int v) {
    fseek(f, (long)TAM_HASH * sizeof(BlocoHash), SEEK_SET);
    fwrite(&v, sizeof(int), 1, f);
}

/* -------------------------------------------------------
 * Inserção
 * ------------------------------------------------------- */
void inserir_relacionamento(Relacionamento rel) {
    FILE *f = fopen(ARQ_HASH, "rb+");
    if (!f) { perror("inserir_relacionamento"); return; }

    int pos = calcular_hash(rel.id_filme);
    BlocoHash bloco;
    ler_bloco(f, pos, &bloco);

    /* Se o bloco primário está livre ou é do mesmo filme */
    if (bloco.id_filme == -1 || bloco.id_filme == rel.id_filme) {
        bloco.id_filme = rel.id_filme;
        if (bloco.num_relacionamentos < MAX_RELS_BLOCO) {
            bloco.rels[bloco.num_relacionamentos++] = rel;
            escrever_bloco(f, pos, &bloco);
            fclose(f);
            return;
        }
    }

    /* Percorre a cadeia de overflow */
    int pos_anterior = pos;
    while (bloco.proximo_bloco != -1) {
        pos_anterior = bloco.proximo_bloco;
        ler_bloco(f, pos_anterior, &bloco);
        if (bloco.id_filme == rel.id_filme && bloco.num_relacionamentos < MAX_RELS_BLOCO) {
            bloco.rels[bloco.num_relacionamentos++] = rel;
            escrever_bloco(f, pos_anterior, &bloco);
            fclose(f);
            return;
        }
    }

    /* Precisa de novo bloco overflow */
    int novo_pos = ler_prox_overflow(f);
    bloco.proximo_bloco = novo_pos;
    escrever_bloco(f, pos_anterior, &bloco);

    BlocoHash novo;
    memset(&novo, 0, sizeof(novo));
    novo.id_filme            = rel.id_filme;
    novo.num_relacionamentos = 1;
    novo.rels[0]             = rel;
    novo.proximo_bloco       = -1;
    escrever_bloco(f, novo_pos, &novo);
    salvar_prox_overflow(f, novo_pos + 1);
    fclose(f);
}

/* -------------------------------------------------------
 * Busca: pessoas de um filme
 * ------------------------------------------------------- */
void buscar_pessoas_do_filme(int id_filme) {
    FILE *f = fopen(ARQ_HASH, "rb");
    if (!f) { perror("buscar_pessoas_do_filme"); return; }

    int pos = calcular_hash(id_filme);
    BlocoHash bloco;
    int encontrou = 0;
    printf("Pessoas do filme ID %d:\n", id_filme);

    while (pos != -1) {
        if (!ler_bloco(f, pos, &bloco)) break;
        if (bloco.id_filme == id_filme) {
            for (int i = 0; i < bloco.num_relacionamentos; i++) {
                char papel_str[16];
                switch (bloco.rels[i].papel) {
                    case 'A': strcpy(papel_str, "Ator");      break;
                    case 'D': strcpy(papel_str, "Diretor");   break;
                    case 'P': strcpy(papel_str, "Produtor");  break;
                    case 'W': strcpy(papel_str, "Escritor");  break;
                    default:  strcpy(papel_str, "Outro");
                }
                printf("  %s (ID %d) — %s\n",
                       bloco.rels[i].nome_pessoa,
                       bloco.rels[i].id_pessoa,
                       papel_str);
                encontrou = 1;
            }
        }
        pos = bloco.proximo_bloco;
    }
    if (!encontrou) printf("  Nenhum relacionamento encontrado.\n");
    fclose(f);
}

/* -------------------------------------------------------
 * Busca: filmes de uma pessoa
 * ------------------------------------------------------- */
void buscar_filmes_da_pessoa(int id_pessoa) {
    FILE *f = fopen(ARQ_HASH, "rb");
    if (!f) { perror("buscar_filmes_da_pessoa"); return; }

    int prox_overflow = ler_prox_overflow(f);
    int total_blocos  = prox_overflow; /* inclui primários e overflow */
    int encontrou = 0;

    printf("Filmes da pessoa ID %d:\n", id_pessoa);
    for (int pos = 0; pos < total_blocos; pos++) {
        BlocoHash bloco;
        if (!ler_bloco(f, pos, &bloco)) continue;
        if (bloco.id_filme == -1) continue;
        for (int i = 0; i < bloco.num_relacionamentos; i++) {
            if (bloco.rels[i].id_pessoa == id_pessoa) {
                char papel_str[16];
                switch (bloco.rels[i].papel) {
                    case 'A': strcpy(papel_str, "Ator");      break;
                    case 'D': strcpy(papel_str, "Diretor");   break;
                    case 'P': strcpy(papel_str, "Produtor");  break;
                    case 'W': strcpy(papel_str, "Escritor");  break;
                    default:  strcpy(papel_str, "Outro");
                }
                printf("  \"%s\" (Filme ID %d) — %s\n",
                       bloco.rels[i].titulo_filme,
                       bloco.rels[i].id_filme,
                       papel_str);
                encontrou = 1;
            }
        }
    }
    if (!encontrou) printf("  Nenhum relacionamento encontrado.\n");
    fclose(f);
}

/* -------------------------------------------------------
 * Helpers internos para consultas analíticas
 * ------------------------------------------------------- */

/* Carrega TODOS os relacionamentos em memória para análise */
static Relacionamento *carregar_todos(int *total_out) {
    FILE *f = fopen(ARQ_HASH, "rb");
    if (!f) { *total_out = 0; return NULL; }

    int prox_overflow = ler_prox_overflow(f);
    int total_blocos  = prox_overflow;

    /* Estimativa conservadora */
    int cap   = total_blocos * MAX_RELS_BLOCO + 1;
    Relacionamento *todos = (Relacionamento *)malloc(cap * sizeof(Relacionamento));
    int count = 0;

    for (int pos = 0; pos < total_blocos; pos++) {
        BlocoHash bloco;
        if (!ler_bloco(f, pos, &bloco)) continue;
        if (bloco.id_filme == -1) continue;
        for (int i = 0; i < bloco.num_relacionamentos; i++) {
            if (count < cap) todos[count++] = bloco.rels[i];
        }
    }
    fclose(f);
    *total_out = count;
    return todos;
}

static int decada(int ano) { return (ano / 10) * 10; }

/* Conta participações por pessoa e papel */
typedef struct { int id; char nome[NOME_MAX]; int contagem; } ContadorPessoa;

static int cmp_contagem_desc(const void *a, const void *b) {
    return ((ContadorPessoa *)b)->contagem - ((ContadorPessoa *)a)->contagem;
}
static int cmp_contagem_asc(const void *a, const void *b) {
    return ((ContadorPessoa *)a)->contagem - ((ContadorPessoa *)b)->contagem;
}

static ContadorPessoa *contar_por_papel(Relacionamento *todos, int total,
                                         char papel, int *n_out) {
    ContadorPessoa *buf = (ContadorPessoa *)malloc(total * sizeof(ContadorPessoa));
    int n = 0;
    for (int i = 0; i < total; i++) {
        if (todos[i].papel != papel) continue;
        int achou = 0;
        for (int j = 0; j < n; j++) {
            if (buf[j].id == todos[i].id_pessoa) {
                buf[j].contagem++;
                achou = 1; break;
            }
        }
        if (!achou) {
            buf[n].id = todos[i].id_pessoa;
            strncpy(buf[n].nome, todos[i].nome_pessoa, NOME_MAX - 1);
            buf[n].contagem = 1;
            n++;
        }
    }
    *n_out = n;
    return buf;
}

/* Lista de títulos para uma pessoa/papel */
static void listar_filmes_de(Relacionamento *todos, int total,
                              int id_pessoa, char papel) {
    for (int i = 0; i < total; i++) {
        if (todos[i].id_pessoa == id_pessoa && todos[i].papel == papel)
            printf("      \"%s\"\n", todos[i].titulo_filme);
    }
}

/* -------------------------------------------------------
 * (a) Todos que trabalharam juntos num mesmo filme
 * ------------------------------------------------------- */
void consulta_a_todos_juntos(void) {
    int total; Relacionamento *todos = carregar_todos(&total);
    if (!todos) return;

    printf("\n=== (a) Pessoas que trabalharam juntas ===\n");
    /* Para cada par de pessoas no mesmo filme */
    for (int i = 0; i < total; i++) {
        for (int j = i + 1; j < total; j++) {
            if (todos[i].id_filme == todos[j].id_filme &&
                todos[i].id_pessoa != todos[j].id_pessoa) {
                printf("  \"%s\" e \"%s\" — \"%s\"\n",
                       todos[i].nome_pessoa, todos[j].nome_pessoa,
                       todos[i].titulo_filme);
            }
        }
    }
    free(todos);
}

/* -------------------------------------------------------
 * (b) Atores e diretores que trabalharam juntos
 * ------------------------------------------------------- */
void consulta_b_atores_diretores(void) {
    int total; Relacionamento *todos = carregar_todos(&total);
    if (!todos) return;

    printf("\n=== (b) Atores e diretores que trabalharam juntos ===\n");
    for (int i = 0; i < total; i++) {
        if (todos[i].papel != 'A') continue;
        for (int j = 0; j < total; j++) {
            if (todos[j].papel != 'D') continue;
            if (todos[i].id_filme == todos[j].id_filme) {
                printf("  Ator: %-30s  Diretor: %-30s  Filme: \"%s\"\n",
                       todos[i].nome_pessoa, todos[j].nome_pessoa,
                       todos[i].titulo_filme);
            }
        }
    }
    free(todos);
}

/* -------------------------------------------------------
 * (c) Atores que atuaram juntos
 * ------------------------------------------------------- */
void consulta_c_atores_juntos(void) {
    int total; Relacionamento *todos = carregar_todos(&total);
    if (!todos) return;

    printf("\n=== (c) Atores que atuaram juntos ===\n");
    for (int i = 0; i < total; i++) {
        if (todos[i].papel != 'A') continue;
        for (int j = i + 1; j < total; j++) {
            if (todos[j].papel != 'A') continue;
            if (todos[i].id_filme == todos[j].id_filme &&
                todos[i].id_pessoa != todos[j].id_pessoa) {
                printf("  %-30s e %-30s — \"%s\"\n",
                       todos[i].nome_pessoa, todos[j].nome_pessoa,
                       todos[i].titulo_filme);
            }
        }
    }
    free(todos);
}

/* -------------------------------------------------------
 * (d) Atores que mais atuaram juntos por década
 * ------------------------------------------------------- */
void consulta_d_atores_juntos_decada(void) {
    int total; Relacionamento *todos = carregar_todos(&total);
    if (!todos) return;

    printf("\n=== (d) Duplas de atores que mais atuaram juntas por decada ===\n");

    /* Coleta décadas presentes */
    int decadas[200]; int nd = 0;
    for (int i = 0; i < total; i++) {
        /* pega o ano do filme via árvore */
        Registro *r = buscar_por_nome(todos[i].titulo_filme);
        if (!r) continue;
        int ano = r->dados.filme.ano_lancamento; free(r);
        int d = decada(ano);
        int achou = 0;
        for (int k = 0; k < nd; k++) if (decadas[k] == d) { achou = 1; break; }
        if (!achou && nd < 200) decadas[nd++] = d;
    }

    for (int di = 0; di < nd; di++) {
        int dec = decadas[di];
        printf("\n  Decada de %d:\n", dec);

        /* Conta pares de atores nesta década */
        typedef struct { int id1, id2; char n1[NOME_MAX], n2[NOME_MAX]; int cnt; } Par;
        Par *pares = (Par *)calloc(total * total / 2 + 1, sizeof(Par));
        int np = 0;

        for (int i = 0; i < total; i++) {
            if (todos[i].papel != 'A') continue;
            Registro *r = buscar_por_nome(todos[i].titulo_filme);
            if (!r) continue;
            int ano = r->dados.filme.ano_lancamento; free(r);
            if (decada(ano) != dec) continue;

            for (int j = i + 1; j < total; j++) {
                if (todos[j].papel != 'A') continue;
                if (todos[j].id_filme != todos[i].id_filme) continue;
                if (todos[j].id_pessoa == todos[i].id_pessoa) continue;

                int id1 = todos[i].id_pessoa < todos[j].id_pessoa ? todos[i].id_pessoa : todos[j].id_pessoa;
                int id2 = todos[i].id_pessoa < todos[j].id_pessoa ? todos[j].id_pessoa : todos[i].id_pessoa;

                int found = 0;
                for (int k = 0; k < np; k++) {
                    if (pares[k].id1 == id1 && pares[k].id2 == id2) {
                        pares[k].cnt++; found = 1; break;
                    }
                }
                if (!found) {
                    pares[np].id1 = id1; pares[np].id2 = id2;
                    strncpy(pares[np].n1, id1 == todos[i].id_pessoa ? todos[i].nome_pessoa : todos[j].nome_pessoa, NOME_MAX);
                    strncpy(pares[np].n2, id2 == todos[i].id_pessoa ? todos[i].nome_pessoa : todos[j].nome_pessoa, NOME_MAX);
                    pares[np].cnt = 1; np++;
                }
            }
        }
        /* Encontra o máximo */
        int max_cnt = 0;
        for (int k = 0; k < np; k++) if (pares[k].cnt > max_cnt) max_cnt = pares[k].cnt;
        for (int k = 0; k < np; k++) {
            if (pares[k].cnt == max_cnt)
                printf("    %s e %s (%d filme(s) juntos)\n", pares[k].n1, pares[k].n2, pares[k].cnt);
        }
        free(pares);
    }
    free(todos);
}

/* -------------------------------------------------------
 * (e) Atores e diretores que trabalharam juntos por década
 * ------------------------------------------------------- */
void consulta_e_ator_diretor_decada(void) {
    int total; Relacionamento *todos = carregar_todos(&total);
    if (!todos) return;

    printf("\n=== (e) Atores e diretores que trabalharam juntos por decada ===\n");

    int decadas[200]; int nd = 0;
    for (int i = 0; i < total; i++) {
        Registro *r = buscar_por_nome(todos[i].titulo_filme);
        if (!r) continue;
        int d = decada(r->dados.filme.ano_lancamento); free(r);
        int achou = 0;
        for (int k = 0; k < nd; k++) if (decadas[k] == d) { achou = 1; break; }
        if (!achou && nd < 200) decadas[nd++] = d;
    }

    for (int di = 0; di < nd; di++) {
        int dec = decadas[di];
        printf("\n  Decada de %d:\n", dec);
        for (int i = 0; i < total; i++) {
            if (todos[i].papel != 'A') continue;
            Registro *ri = buscar_por_nome(todos[i].titulo_filme);
            if (!ri) continue;
            int ano = ri->dados.filme.ano_lancamento; free(ri);
            if (decada(ano) != dec) continue;
            for (int j = 0; j < total; j++) {
                if (todos[j].papel != 'D') continue;
                if (todos[j].id_filme != todos[i].id_filme) continue;
                printf("    Ator: %-28s  Diretor: %-28s  Filme: \"%s\"\n",
                       todos[i].nome_pessoa, todos[j].nome_pessoa, todos[i].titulo_filme);
            }
        }
    }
    free(todos);
}

/* -------------------------------------------------------
 * Helpers (f)–(k): ranking por papel + lista de filmes
 * ------------------------------------------------------- */
static void ranking_por_papel(Relacionamento *todos, int total, char papel,
                               const char *titulo_papel, int mais) {
    int n;
    ContadorPessoa *cnt = contar_por_papel(todos, total, papel, &n);
    if (!n) { free(cnt); return; }

    qsort(cnt, n, sizeof(ContadorPessoa),
          mais ? cmp_contagem_desc : cmp_contagem_asc);

    int limite = cnt[0].contagem; /* valor de corte */
    printf("\n");
    for (int i = 0; i < n; i++) {
        if (cnt[i].contagem != limite && i > 0) {
            /* Para "mais": mostra apenas quem tem o máximo
               Para "menos": mostra apenas quem tem o mínimo */
            if (mais && cnt[i].contagem < limite) break;
            if (!mais && cnt[i].contagem > cnt[0].contagem) break;
        }
        printf("  %s: %s (%d filme(s))\n", titulo_papel, cnt[i].nome, cnt[i].contagem);
        listar_filmes_de(todos, total, cnt[i].id, papel);
    }
    /* Mostra todos que têm a mesma contagem do primeiro */
    free(cnt);
}

/* (f) */
void consulta_f_atores_mais_atuaram(void) {
    int total; Relacionamento *todos = carregar_todos(&total);
    printf("\n=== (f) Atores que mais atuaram ===");
    ranking_por_papel(todos, total, 'A', "Ator", 1);
    free(todos);
}
/* (g) */
void consulta_g_atores_menos_atuaram(void) {
    int total; Relacionamento *todos = carregar_todos(&total);
    printf("\n=== (g) Atores que menos atuaram ===");
    ranking_por_papel(todos, total, 'A', "Ator", 0);
    free(todos);
}
/* (h) */
void consulta_h_diretores_mais(void) {
    int total; Relacionamento *todos = carregar_todos(&total);
    printf("\n=== (h) Diretores que mais dirigiram ===");
    ranking_por_papel(todos, total, 'D', "Diretor", 1);
    free(todos);
}
/* (i) */
void consulta_i_diretores_menos(void) {
    int total; Relacionamento *todos = carregar_todos(&total);
    printf("\n=== (i) Diretores que menos dirigiram ===");
    ranking_por_papel(todos, total, 'D', "Diretor", 0);
    free(todos);
}
/* (j) */
void consulta_j_produtores_mais(void) {
    int total; Relacionamento *todos = carregar_todos(&total);
    printf("\n=== (j) Produtores mais atuantes ===");
    ranking_por_papel(todos, total, 'P', "Produtor", 1);
    free(todos);
}
/* (k) */
void consulta_k_produtores_menos(void) {
    int total; Relacionamento *todos = carregar_todos(&total);
    printf("\n=== (k) Produtores menos atuantes ===");
    ranking_por_papel(todos, total, 'P', "Produtor", 0);
    free(todos);
}

/* -------------------------------------------------------
 * (l) Consultas (f)–(k) por década
 * ------------------------------------------------------- */
void consulta_l_f_a_k_por_decada(void) {
    int total; Relacionamento *todos = carregar_todos(&total);
    if (!todos) return;

    /* Coleta décadas */
    int decadas[200]; int nd = 0;
    for (int i = 0; i < total; i++) {
        Registro *r = buscar_por_nome(todos[i].titulo_filme);
        if (!r) continue;
        int d = decada(r->dados.filme.ano_lancamento); free(r);
        int achou = 0;
        for (int k = 0; k < nd; k++) if (decadas[k] == d) { achou = 1; break; }
        if (!achou && nd < 200) decadas[nd++] = d;
    }

    for (int di = 0; di < nd; di++) {
        int dec = decadas[di];
        printf("\n========== Decada de %d ==========\n", dec);

        /* Filtra relacionamentos desta década */
        Relacionamento *sub = (Relacionamento *)malloc(total * sizeof(Relacionamento));
        int ns = 0;
        for (int i = 0; i < total; i++) {
            Registro *r = buscar_por_nome(todos[i].titulo_filme);
            if (!r) continue;
            int ano = r->dados.filme.ano_lancamento; free(r);
            if (decada(ano) == dec) sub[ns++] = todos[i];
        }

        printf("--- Atores ---");
        ranking_por_papel(sub, ns, 'A', "Ator (mais)", 1);
        ranking_por_papel(sub, ns, 'A', "Ator (menos)", 0);
        printf("--- Diretores ---");
        ranking_por_papel(sub, ns, 'D', "Diretor (mais)", 1);
        ranking_por_papel(sub, ns, 'D', "Diretor (menos)", 0);
        printf("--- Produtores ---");
        ranking_por_papel(sub, ns, 'P', "Produtor (mais)", 1);
        ranking_por_papel(sub, ns, 'P', "Produtor (menos)", 0);
        free(sub);
    }
    free(todos);
}

/* -------------------------------------------------------
 * (m) Filmes que são continuações
 * Heurística: título de um filme é prefixo de outro
 * (ex: "The Matrix" é prefixo de "The Matrix Reloaded")
 * ------------------------------------------------------- */
typedef struct { char titulo[NOME_MAX]; int ano; } InfoFilme;
static InfoFilme g_filmes_m[500];
static int       g_nf_m = 0;

static void coletar_filme_cb(const Registro *r) {
    if (r->tipo == TIPO_FILME && g_nf_m < 500) {
        strncpy(g_filmes_m[g_nf_m].titulo, r->dados.filme.titulo, NOME_MAX - 1);
        g_filmes_m[g_nf_m].ano = r->dados.filme.ano_lancamento;
        g_nf_m++;
    }
}

void consulta_m_filmes_continuacao(void) {
    printf("\n=== (m) Filmes que sao continuacoes ===\n");
    g_nf_m = 0;
    varredura_folhas(coletar_filme_cb);

    int encontrou = 0;
    for (int i = 0; i < g_nf_m; i++) {
        size_t li = strlen(g_filmes_m[i].titulo);
        for (int j = 0; j < g_nf_m; j++) {
            if (i == j) continue;
            if (strncmp(g_filmes_m[i].titulo, g_filmes_m[j].titulo, li) == 0
                && g_filmes_m[j].titulo[li] == ' ') {
                printf("  \"%s\" (%d) eh continuacao de: \"%s\" (%d)\n",
                       g_filmes_m[j].titulo, g_filmes_m[j].ano,
                       g_filmes_m[i].titulo, g_filmes_m[i].ano);
                encontrou = 1;
            }
        }
    }
    if (!encontrou) printf("  Nenhuma continuacao detectada.\n");
}

/* -------------------------------------------------------
 * (n) Atores que nasceram no mesmo ano
 * ------------------------------------------------------- */
void consulta_n_atores_mesmo_ano(void) {
    printf("\n=== (n) Atores que nasceram no mesmo ano ===\n");

    typedef struct { char nome[NOME_MAX]; int ano; int id; } InfoPessoa;
    InfoPessoa pessoas[500]; int np = 0;

    /* Coleta todos via hash (atores têm papel 'A') */
    int total; Relacionamento *todos = carregar_todos(&total);
    if (!todos) return;

    for (int i = 0; i < total; i++) {
        if (todos[i].papel != 'A') continue;
        int ja = 0;
        for (int k = 0; k < np; k++) if (pessoas[k].id == todos[i].id_pessoa) { ja = 1; break; }
        if (!ja && np < 500) {
            Registro *r = buscar_por_id(todos[i].id_pessoa);
            if (r && r->tipo == TIPO_PESSOA && r->dados.pessoa.ano_nascimento > 0) {
                pessoas[np].id  = todos[i].id_pessoa;
                pessoas[np].ano = r->dados.pessoa.ano_nascimento;
                strncpy(pessoas[np].nome, todos[i].nome_pessoa, NOME_MAX);
                np++;
            }
            if (r) free(r);
        }
    }

    int imprimiu_algum = 0;
    for (int i = 0; i < np; i++) {
        for (int j = i + 1; j < np; j++) {
            if (pessoas[i].ano == pessoas[j].ano) {
                printf("  %s e %s — nasceram em %d\n",
                       pessoas[i].nome, pessoas[j].nome, pessoas[i].ano);
                imprimiu_algum = 1;
            }
        }
    }
    if (!imprimiu_algum) printf("  Nenhum par encontrado.\n");
    free(todos);
}

/* -------------------------------------------------------
 * (o) Atores que já dirigiram
 * ------------------------------------------------------- */
void consulta_o_atores_que_dirigiram(void) {
    int total; Relacionamento *todos = carregar_todos(&total);
    if (!todos) return;

    printf("\n=== (o) Atores que ja dirigiram ===\n");
    int imprimiu = 0;
    for (int i = 0; i < total; i++) {
        if (todos[i].papel != 'A') continue;
        for (int j = 0; j < total; j++) {
            if (todos[j].papel != 'D') continue;
            if (todos[j].id_pessoa == todos[i].id_pessoa) {
                printf("  %s (tambem dirigiu \"%s\")\n",
                       todos[i].nome_pessoa, todos[j].titulo_filme);
                imprimiu = 1;
                break;
            }
        }
    }
    if (!imprimiu) printf("  Nenhum encontrado.\n");
    free(todos);
}

/* -------------------------------------------------------
 * (p) Atores que já produziram
 * ------------------------------------------------------- */
void consulta_p_atores_que_produziram(void) {
    int total; Relacionamento *todos = carregar_todos(&total);
    if (!todos) return;

    printf("\n=== (p) Atores que ja produziram ===\n");
    int imprimiu = 0;
    for (int i = 0; i < total; i++) {
        if (todos[i].papel != 'A') continue;
        for (int j = 0; j < total; j++) {
            if (todos[j].papel != 'P') continue;
            if (todos[j].id_pessoa == todos[i].id_pessoa) {
                printf("  %s (tambem produziu \"%s\")\n",
                       todos[i].nome_pessoa, todos[j].titulo_filme);
                imprimiu = 1;
                break;
            }
        }
    }
    if (!imprimiu) printf("  Nenhum encontrado.\n");
    free(todos);
}

/* -------------------------------------------------------
 * (q) Retirar todos os relacionamentos de um filme
 * (não remove o filme da árvore, só os relacionamentos)
 * ------------------------------------------------------- */
void consulta_q_retirar_todos_do_filme(int id_filme) {
    FILE *f = fopen(ARQ_HASH, "rb+");
    if (!f) { perror("consulta_q"); return; }

    int pos = calcular_hash(id_filme);
    BlocoHash bloco;
    int removidos = 0;

    while (pos != -1) {
        if (!ler_bloco(f, pos, &bloco)) break;
        if (bloco.id_filme == id_filme) {
            removidos += bloco.num_relacionamentos;
            bloco.num_relacionamentos = 0;
            bloco.id_filme = -1;
            escrever_bloco(f, pos, &bloco);
        }
        pos = bloco.proximo_bloco;
    }
    fclose(f);
    printf("Removidos %d relacionamentos do filme ID %d.\n", removidos, id_filme);

    /* Remove também da árvore B+ */
    remover_por_id(id_filme);
}

/* -------------------------------------------------------
 * (r) Filmes em que a mesma pessoa escreveu, dirigiu e produziu
 * ------------------------------------------------------- */
void consulta_r_escreveu_dirigiu_produziu(void) {
    int total; Relacionamento *todos = carregar_todos(&total);
    if (!todos) return;

    printf("\n=== (r) Filmes onde a mesma pessoa escreveu, dirigiu e produziu ===\n");
    int imprimiu = 0;
    for (int i = 0; i < total; i++) {
        if (todos[i].papel != 'W') continue;
        for (int j = 0; j < total; j++) {
            if (todos[j].papel != 'D') continue;
            if (todos[j].id_pessoa != todos[i].id_pessoa) continue;
            if (todos[j].id_filme  != todos[i].id_filme)  continue;
            for (int k = 0; k < total; k++) {
                if (todos[k].papel != 'P') continue;
                if (todos[k].id_pessoa != todos[i].id_pessoa) continue;
                if (todos[k].id_filme  != todos[i].id_filme)  continue;
                printf("  %s — \"%s\"\n", todos[i].nome_pessoa, todos[i].titulo_filme);
                imprimiu = 1;
            }
        }
    }
    if (!imprimiu) printf("  Nenhum encontrado.\n");
    free(todos);
}

/* -------------------------------------------------------
 * (s) Filmes em que a mesma pessoa dirigiu e produziu
 * ------------------------------------------------------- */
void consulta_s_dirigiu_e_produziu(void) {
    int total; Relacionamento *todos = carregar_todos(&total);
    if (!todos) return;

    printf("\n=== (s) Filmes onde a mesma pessoa dirigiu e produziu ===\n");
    int imprimiu = 0;
    for (int i = 0; i < total; i++) {
        if (todos[i].papel != 'D') continue;
        for (int j = 0; j < total; j++) {
            if (todos[j].papel != 'P') continue;
            if (todos[j].id_pessoa != todos[i].id_pessoa) continue;
            if (todos[j].id_filme  != todos[i].id_filme)  continue;
            printf("  %s — \"%s\"\n", todos[i].nome_pessoa, todos[i].titulo_filme);
            imprimiu = 1;
        }
    }
    if (!imprimiu) printf("  Nenhum encontrado.\n");
    free(todos);
}

/* -------------------------------------------------------
 * (t) Atores que nasceram no ano de lançamento de um filme
 * ------------------------------------------------------- */
void consulta_t_ator_nasceu_no_ano_do_filme(void) {
    int total; Relacionamento *todos = carregar_todos(&total);
    if (!todos) return;

    printf("\n=== (t) Atores que nasceram no ano de lancamento de um filme ===\n");
    int imprimiu = 0;

    /* Para cada ator, busca seus dados de nascimento */
    for (int i = 0; i < total; i++) {
        if (todos[i].papel != 'A') continue;
        Registro *rp = buscar_por_id(todos[i].id_pessoa);
        if (!rp) continue;
        int ano_nasc = rp->dados.pessoa.ano_nascimento; free(rp);
        if (ano_nasc <= 0) continue;

        /* Para cada filme, verifica se o ano de lançamento coincide */
        for (int j = 0; j < total; j++) {
            if (todos[j].id_filme == todos[i].id_filme) continue; /* pula filmes do próprio ator */
            Registro *rf = buscar_por_nome(todos[j].titulo_filme);
            if (!rf) continue;
            int ano_filme = rf->dados.filme.ano_lancamento; free(rf);
            if (ano_filme == ano_nasc) {
                printf("  %s (nasc. %d) — \"%s\" (%d)\n",
                       todos[i].nome_pessoa, ano_nasc,
                       todos[j].titulo_filme, ano_filme);
                imprimiu = 1;
                break; /* um match já basta por ator para não repetir muito */
            }
        }
    }
    if (!imprimiu) printf("  Nenhum encontrado.\n");
    free(todos);
}


/* Remove todas as relações de um filme na Tabela Hash */
void limpar_relacionamentos_filme(int id_filme) {
    FILE *f = fopen(ARQ_HASH, "rb+");
    if (!f) return;

    int pos = calcular_hash(id_filme);
    BlocoHash bloco;

    while (pos != -1) {
        if (!ler_bloco(f, pos, &bloco)) break;
        if (bloco.id_filme == id_filme) {
            bloco.num_relacionamentos = 0;
            bloco.id_filme = -1; // Marca o bloco como livre
            escrever_bloco(f, pos, &bloco);
        }
        pos = bloco.proximo_bloco;
    }
    fclose(f);
}

/* Remove todas as relações de uma pessoa na Tabela Hash */
void limpar_relacionamentos_pessoa(int id_pessoa) {
    FILE *f = fopen(ARQ_HASH, "rb+");
    if (!f) return;

    int prox_overflow = ler_prox_overflow(f);
    int total_blocos  = prox_overflow;

    for (int pos = 0; pos < total_blocos; pos++) {
        BlocoHash bloco;
        if (!ler_bloco(f, pos, &bloco)) continue;
        if (bloco.id_filme == -1) continue;

        int modificou = 0;
        // Varre os relacionamentos de trás para frente para evitar pular itens ao remover
        for (int i = bloco.num_relacionamentos - 1; i >= 0; i--) {
            if (bloco.rels[i].id_pessoa == id_pessoa) {
                // Desloca os registros para a esquerda para preencher o buraco
                for (int j = i; j < bloco.num_relacionamentos - 1; j++) {
                    bloco.rels[j] = bloco.rels[j+1];
                }
                bloco.num_relacionamentos--;
                modificou = 1;
            }
        }

        if (modificou) {
            if (bloco.num_relacionamentos == 0) {
                bloco.id_filme = -1; // Se a pessoa era a única no bloco, libera ele
            }
            escrever_bloco(f, pos, &bloco);
        }
    }
    fclose(f);
}
