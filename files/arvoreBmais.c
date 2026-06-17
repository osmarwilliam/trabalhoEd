#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "arvoreBmais.h"
#include "disco.h"
#include "hash_relacionamento.h"

static int grau_t = -1;

/* -------------------------------------------------------
 * Stack de ancestrais usada durante inserção/remoção.
 * Guarda pares (id_no, indice_filho_seguido).
 * ------------------------------------------------------- */
typedef struct { int id_no; int idx_filho; } EntradaStack;
static EntradaStack stack_pais[MAX_ALTURA];
static int          stack_top = 0;

static void stack_push(int id_no, int idx_filho) {
    if (stack_top >= MAX_ALTURA) {
        fprintf(stderr, "ERRO: stack de pais overflow (altura > %d)\n", MAX_ALTURA);
        exit(EXIT_FAILURE);
    }
    stack_pais[stack_top].id_no     = id_no;
    stack_pais[stack_top].idx_filho = idx_filho;
    stack_top++;
}

static EntradaStack stack_pop(void) {
    return stack_pais[--stack_top];
}

 //Inicialização
void inicializar_arvore(int t) {
    grau_t = t;
    inicializar_disco(t);
    /* Se já inicializado em sessão anterior, usa o t gravado */
    int t_disco = ler_grau_t();
    if (t_disco != -1) grau_t = t_disco;

    int raiz = ler_raiz_id();
    if (raiz == -1) {
        /* Cria folha vazia e raiz apontando para ela */
        int id_folha = gerar_novo_id_folha();
        NoFolha *folha = (NoFolha *)malloc(sizeof(NoFolha));
        folha->registros        = (Registro *)calloc(2*grau_t - 1, sizeof(Registro));
        folha->num_registros    = 0;
        folha->id_proxima_folha = -1;
        salvar_folha(id_folha, folha, grau_t);
        liberar_folha(folha);

        int id_raiz = gerar_novo_id_indice();
        NoIndice *raiz_no = (NoIndice *)malloc(sizeof(NoIndice));
        raiz_no->chaves = (char **)malloc((2*grau_t - 1) * sizeof(char *));
        for (int i = 0; i < 2*grau_t - 1; i++)
            raiz_no->chaves[i] = (char *)calloc(NOME_MAX, 1);
        raiz_no->filhos = (int *)malloc((2*grau_t) * sizeof(int));
        for (int i = 0; i < 2*grau_t; i++) raiz_no->filhos[i] = -1;
        raiz_no->eh_folha   = 1;
        raiz_no->num_chaves = 0;
        raiz_no->filhos[0]  = id_folha;
        salvar_no_indice(id_raiz, raiz_no, grau_t);
        salvar_raiz_id(id_raiz);
        liberar_no_indice(raiz_no, grau_t);
    }
}

/* -------------------------------------------------------
 * Navegação: desce até o nó folha correto para `chave`.
 * Em B+, a chave copiada no nó interno é a PRIMEIRA chave
 * da folha DIREITA. Logo, se chave == chave_interna, o
 * registro está na folha DIREITA → usamos >= 0 (avança i).
 * Preenche stack_pais com o caminho percorrido.
 * ------------------------------------------------------- */
static int descer_ate_folha(int id_no, const char *chave, int guardar_stack) {
    NoIndice *no = ler_no_indice(id_no, grau_t);
    if (!no) return -1;

    int i = 0;
    /* >= 0: igualdade avança para o filho direito (folha que contém a chave) */
    while (i < no->num_chaves && strcmp(chave, no->chaves[i]) >= 0)
        i++;

    int id_filho = no->filhos[i];
    int eh_folha = no->eh_folha;
    liberar_no_indice(no, grau_t);

    if (guardar_stack) stack_push(id_no, i);

    if (eh_folha) return id_filho;
    return descer_ate_folha(id_filho, chave, guardar_stack);
}

/* -------------------------------------------------------
 * Primeira folha (varredura sequencial)
 * ------------------------------------------------------- */
static int obter_primeira_folha(int id_no) {
    NoIndice *no = ler_no_indice(id_no, grau_t);
    if (!no) return -1;
    int eh_folha  = no->eh_folha;
    int filho_esq = no->filhos[0];
    liberar_no_indice(no, grau_t);
    if (eh_folha) return filho_esq;
    return obter_primeira_folha(filho_esq);
}

/* -------------------------------------------------------
 * Varredura completa para consultas analíticas
 * ------------------------------------------------------- */
void varredura_folhas(void (*callback)(const Registro *)) {
    if (grau_t == -1) grau_t = ler_grau_t();
    int raiz = ler_raiz_id();
    if (raiz == -1) return;
    int id_folha = obter_primeira_folha(raiz);
    while (id_folha != -1) {
        NoFolha *folha = ler_folha(id_folha, grau_t);
        if (!folha) break;
        for (int i = 0; i < folha->num_registros; i++)
            callback(&folha->registros[i]);
        int prox = folha->id_proxima_folha;
        liberar_folha(folha);
        id_folha = prox;
    }
}

/* -------------------------------------------------------
 * Buscas
 * ------------------------------------------------------- */
Registro *buscar_por_nome(const char *nome) {
    if (grau_t == -1) grau_t = ler_grau_t();
    int raiz = ler_raiz_id();
    if (raiz == -1) return NULL;

    int id_folha = descer_ate_folha(raiz, nome, 0);
    if (id_folha == -1) return NULL;

    NoFolha *folha = ler_folha(id_folha, grau_t);
    if (!folha) return NULL;

    Registro *resultado = NULL;
    for (int i = 0; i < folha->num_registros; i++) {
        if (strcmp(folha->registros[i].chave, nome) == 0) {
            resultado = (Registro *)malloc(sizeof(Registro));
            *resultado = folha->registros[i];
            break;
        }
    }
    liberar_folha(folha);
    return resultado;
}

/* busca por ID: varredura linear (ID não é a chave da árvore) */
Registro *buscar_por_id(int id) {
    if (grau_t == -1) grau_t = ler_grau_t();
    int raiz = ler_raiz_id();
    if (raiz == -1) return NULL;

    int id_folha = obter_primeira_folha(raiz);
    while (id_folha != -1) {
        NoFolha *folha = ler_folha(id_folha, grau_t);
        if (!folha) break;
        for (int i = 0; i < folha->num_registros; i++) {
            int reg_id = (folha->registros[i].tipo == TIPO_PESSOA)
                         ? folha->registros[i].dados.pessoa.id_pessoa
                         : folha->registros[i].dados.filme.id_filme;
            if (reg_id == id) {
                Registro *res = (Registro *)malloc(sizeof(Registro));
                *res = folha->registros[i];
                liberar_folha(folha);
                return res;
            }
        }
        int prox = folha->id_proxima_folha;
        liberar_folha(folha);
        id_folha = prox;
    }
    return NULL;
}

/* -------------------------------------------------------
 * Inserção auxiliar: insere reg ordenado em folha não cheia
 * ------------------------------------------------------- */
static void inserir_na_folha(int id_folha, Registro reg) {
    NoFolha *folha = ler_folha(id_folha, grau_t);
    if (!folha) return;
    int pos = folha->num_registros - 1;
    while (pos >= 0 && strcmp(reg.chave, folha->registros[pos].chave) < 0) {
        folha->registros[pos + 1] = folha->registros[pos];
        pos--;
    }
    folha->registros[pos + 1] = reg;
    folha->num_registros++;
    salvar_folha(id_folha, folha, grau_t);
    liberar_folha(folha);
}

/* -------------------------------------------------------
 * Inserção com split e propagação
 * ------------------------------------------------------- */
void inserir_registro(Registro reg) {
    if (grau_t == -1) grau_t = ler_grau_t();
    int raiz = ler_raiz_id();

    stack_top = 0;
    int id_folha = descer_ate_folha(raiz, reg.chave, 1);
    NoFolha *folha = ler_folha(id_folha, grau_t);
    if (!folha) return;

    if (folha->num_registros < 2*grau_t - 1) {
        /* Folha tem espaço — insere diretamente */
        liberar_folha(folha);
        inserir_na_folha(id_folha, reg);
        return;
    }

    /* --- Split da folha --- */
    Registro *temp = (Registro *)malloc(2*grau_t * sizeof(Registro));
    for (int i = 0; i < folha->num_registros; i++) temp[i] = folha->registros[i];

    int pos = 2*grau_t - 1;
    while (pos > 0 && strcmp(reg.chave, temp[pos-1].chave) < 0) {
        temp[pos] = temp[pos-1]; pos--;
    }
    temp[pos] = reg;

    /* metade esquerda: índices 0 .. grau_t-1  (grau_t registros) */
    /* metade direita:  índices grau_t .. 2t-1 (grau_t registros) */
    int novo_id_folha = gerar_novo_id_folha();
    NoFolha *nova_folha = (NoFolha *)malloc(sizeof(NoFolha));
    nova_folha->registros = (Registro *)malloc((2*grau_t - 1) * sizeof(Registro));

    folha->num_registros    = grau_t;
    nova_folha->num_registros = grau_t;
    for (int i = 0; i < grau_t; i++) folha->registros[i]      = temp[i];
    for (int i = 0; i < grau_t; i++) nova_folha->registros[i] = temp[grau_t + i];

    nova_folha->id_proxima_folha = folha->id_proxima_folha;
    folha->id_proxima_folha      = novo_id_folha;

    /* Chave copiada para o pai: primeira chave da folha direita */
    char chave_promovida[NOME_MAX];
    strncpy(chave_promovida, nova_folha->registros[0].chave, NOME_MAX - 1);
    chave_promovida[NOME_MAX - 1] = '\0';

    salvar_folha(id_folha,      folha,      grau_t);
    salvar_folha(novo_id_folha, nova_folha, grau_t);
    liberar_folha(folha);
    liberar_folha(nova_folha);
    free(temp);

    /* --- Propaga split para os nós internos --- */
    int id_filho_direito = novo_id_folha;

    while (stack_top > 0) {
        EntradaStack entrada = stack_pop();
        int id_pai  = entrada.id_no;
        NoIndice *pai = ler_no_indice(id_pai, grau_t);
        if (!pai) return;

        if (pai->num_chaves < 2*grau_t - 1) {
            /* Pai tem espaço — insere chave e filho */
            int idx = pai->num_chaves - 1;
            while (idx >= 0 && strcmp(chave_promovida, pai->chaves[idx]) < 0) {
                strncpy(pai->chaves[idx+1], pai->chaves[idx], NOME_MAX);
                pai->filhos[idx+2] = pai->filhos[idx+1];
                idx--;
            }
            strncpy(pai->chaves[idx+1], chave_promovida, NOME_MAX - 1);
            pai->filhos[idx+2] = id_filho_direito;
            pai->num_chaves++;
            salvar_no_indice(id_pai, pai, grau_t);
            liberar_no_indice(pai, grau_t);
            return;
        }

        /* --- Split do nó interno ---
         * O pai tem 2t-1 chaves e 2t filhos (cheio).
         * Apos inserir chave_promovida ficamos com 2t chaves e 2t+1 filhos.
         * A chave do meio (indice t) sobe; esquerda fica com t-1, direita com t.
         *
         * Usamos arrays temporarios de tamanho exato:
         *   tc[0..2t-1]  = 2t chaves  (indices validos)
         *   tf[0..2t]    = 2t+1 filhos
         */
        int n_tmp = 2 * grau_t;           /* numero de chaves apos insercao */
        char **tc = (char **)malloc((n_tmp) * sizeof(char *));
        for (int i = 0; i < n_tmp; i++) tc[i] = (char *)calloc(NOME_MAX, 1);
        int *tf = (int *)malloc((n_tmp + 1) * sizeof(int));
        for (int i = 0; i <= n_tmp; i++) tf[i] = -1;

        /* Copia os 2t-1 chaves e 2t filhos existentes */
        for (int i = 0; i < 2*grau_t - 1; i++) strncpy(tc[i], pai->chaves[i], NOME_MAX - 1);
        for (int i = 0; i < 2*grau_t;     i++) tf[i] = pai->filhos[i];

        /* Acha posicao de insercao (busca linear nas 2t-1 chaves existentes) */
        int pos_ins = 2*grau_t - 1; /* posicao default = no final */
        for (int i = 0; i < 2*grau_t - 1; i++) {
            if (strcmp(chave_promovida, tc[i]) < 0) { pos_ins = i; break; }
        }
        /* Desloca para abrir espaco em pos_ins */
        for (int i = 2*grau_t - 1; i > pos_ins; i--) {
            strncpy(tc[i], tc[i-1], NOME_MAX);
            tf[i+1] = tf[i];
        }
        strncpy(tc[pos_ins], chave_promovida, NOME_MAX - 1);
        tf[pos_ins + 1] = id_filho_direito;

        /* Chave do meio (indice t-1 de 0-based apos ter 2t chaves) sobe */
        int m = grau_t - 1; /* indice da chave do meio em tc[0..2t-1] */
        /* Nó com 2t chaves: queremos t-1 a esquerda, 1 sobe, t a direita */
        /* Convencao classica: m = t-1 chaves esquerda, tc[m] sobe, tc[m+1..2t-1] direita */
        /* Com 2t chaves (0..2t-1): m = t-1, sobe tc[m], esq tc[0..m-1], dir tc[m+1..2t-1] */
        char nova_promovida[NOME_MAX];
        strncpy(nova_promovida, tc[m], NOME_MAX - 1);
        nova_promovida[NOME_MAX - 1] = '\0';

        /* Nó esquerdo (pai reutilizado): t-1 chaves, t filhos */
        pai->num_chaves = m; /* = t-1 */
        for (int i = 0; i < m; i++) {
            strncpy(pai->chaves[i], tc[i], NOME_MAX - 1);
            pai->filhos[i] = tf[i];
        }
        pai->filhos[m] = tf[m];

        /* Nó direito: t chaves, t+1 filhos */
        int n_dir = n_tmp - m - 1; /* = 2t - (t-1) - 1 = t */
        int novo_id_indice = gerar_novo_id_indice();
        NoIndice *novo_no = (NoIndice *)malloc(sizeof(NoIndice));
        novo_no->chaves = (char **)malloc((2*grau_t - 1) * sizeof(char *));
        for (int i = 0; i < 2*grau_t - 1; i++) novo_no->chaves[i] = (char *)calloc(NOME_MAX, 1);
        novo_no->filhos = (int *)malloc((2*grau_t) * sizeof(int));
        for (int i = 0; i < 2*grau_t; i++) novo_no->filhos[i] = -1;
        novo_no->eh_folha   = pai->eh_folha;
        novo_no->num_chaves = n_dir;
        for (int i = 0; i < n_dir; i++) {
            strncpy(novo_no->chaves[i], tc[m + 1 + i], NOME_MAX - 1);
            novo_no->filhos[i] = tf[m + 1 + i];
        }
        novo_no->filhos[n_dir] = tf[m + 1 + n_dir];

        for (int i = 0; i < n_tmp; i++) free(tc[i]);
        free(tc); free(tf);

        salvar_no_indice(id_pai,         pai,    grau_t);
        salvar_no_indice(novo_id_indice, novo_no, grau_t);
        liberar_no_indice(pai,    grau_t);
        liberar_no_indice(novo_no, grau_t);

        strncpy(chave_promovida, nova_promovida, NOME_MAX);
        id_filho_direito = novo_id_indice;

        /* Se o nó que acabou de splittar era a raiz, cria nova raiz */
        if (id_pai == ler_raiz_id()) {
            int nova_raiz_id = gerar_novo_id_indice();
            NoIndice *nova_raiz = (NoIndice *)malloc(sizeof(NoIndice));
            nova_raiz->chaves = (char **)malloc((2*grau_t - 1) * sizeof(char *));
            for (int i = 0; i < 2*grau_t - 1; i++) nova_raiz->chaves[i] = (char *)calloc(NOME_MAX, 1);
            nova_raiz->filhos = (int *)malloc((2*grau_t) * sizeof(int));
            for (int i = 0; i < 2*grau_t; i++) nova_raiz->filhos[i] = -1;
            nova_raiz->eh_folha   = 0;
            nova_raiz->num_chaves = 1;
            strncpy(nova_raiz->chaves[0], chave_promovida, NOME_MAX - 1);
            nova_raiz->filhos[0] = id_pai;
            nova_raiz->filhos[1] = novo_id_indice;
            salvar_no_indice(nova_raiz_id, nova_raiz, grau_t);
            salvar_raiz_id(nova_raiz_id);
            liberar_no_indice(nova_raiz, grau_t);
            return;
        }
    }
}

void inserir_pessoa(Pessoa nova_pessoa) {
    Registro reg;
    reg.tipo = TIPO_PESSOA;
    strncpy(reg.chave, nova_pessoa.nome, NOME_MAX - 1);
    reg.chave[NOME_MAX - 1] = '\0';
    reg.dados.pessoa = nova_pessoa;
    inserir_registro(reg);
}

void inserir_filme(Filme novo_filme) {
    Registro reg;
    reg.tipo = TIPO_FILME;
    strncpy(reg.chave, novo_filme.titulo, NOME_MAX - 1);
    reg.chave[NOME_MAX - 1] = '\0';
    reg.dados.filme = novo_filme;
    inserir_registro(reg);
}

/* -------------------------------------------------------
 * Remoção — com propagação de underflow nos nós internos
 * ------------------------------------------------------- */
void remover_registro(const char *nome) {
    if (grau_t == -1) grau_t = ler_grau_t();
    int raiz = ler_raiz_id();

    stack_top = 0;
    int id_folha = descer_ate_folha(raiz, nome, 1);
    NoFolha *folha = ler_folha(id_folha, grau_t);
    if (!folha) { printf("Registro '%s' nao encontrado.\n", nome); return; }

    /* Localiza o registro na folha */
    int pos = -1;
    for (int i = 0; i < folha->num_registros; i++) {
        if (strcmp(folha->registros[i].chave, nome) == 0) { pos = i; break; }
    }
    if (pos == -1) {
        printf("Registro '%s' nao encontrado.\n", nome);
        liberar_folha(folha);
        return;
    }

    for (int i = pos; i < folha->num_registros - 1; i++)
        folha->registros[i] = folha->registros[i+1];
    folha->num_registros--;
    salvar_folha(id_folha, folha, grau_t);
    printf("Registro '%s' removido com sucesso.\n", nome);

    /* --- Rebalanceia folha se necessário --- */
    if (folha->num_registros >= grau_t - 1 || stack_top == 0) {
        liberar_folha(folha);
        return;  /* sem underflow ou é raiz */
    }

    /* Trabalha com id_folha_atual como nó com underflow */
    int id_atual = id_folha;
    NoFolha *folha_atual = folha; /* já temos ela em memória */

    /* ---- loop de propagação de underflow ---- */
    while (stack_top > 0) {
        EntradaStack entrada = stack_pop();
        int id_pai   = entrada.id_no;
        int pos_pai  = entrada.idx_filho; /* índice que leva ao nó com underflow */

        NoIndice *pai = ler_no_indice(id_pai, grau_t);
        if (!pai) { liberar_folha(folha_atual); return; }

        int balanceado = 0;

        if (folha_atual != NULL) {
            /* Rebalancear folha */

            /* 1. Emprestar do irmão esquerdo */
            if (pos_pai > 0 && !balanceado) {
                int id_esq   = pai->filhos[pos_pai - 1];
                NoFolha *esq = ler_folha(id_esq, grau_t);
                if (esq && esq->num_registros > grau_t - 1) {
                    for (int i = folha_atual->num_registros; i > 0; i--)
                        folha_atual->registros[i] = folha_atual->registros[i-1];
                    folha_atual->registros[0] = esq->registros[esq->num_registros - 1];
                    folha_atual->num_registros++;
                    esq->num_registros--;
                    strncpy(pai->chaves[pos_pai - 1], folha_atual->registros[0].chave, NOME_MAX);
                    salvar_folha(id_atual, folha_atual, grau_t);
                    salvar_folha(id_esq,   esq,         grau_t);
                    salvar_no_indice(id_pai, pai, grau_t);
                    balanceado = 1;
                }
                liberar_folha(esq);
            }

            /* 2. Emprestar do irmão direito */
            if (pos_pai < pai->num_chaves && !balanceado) {
                int id_dir   = pai->filhos[pos_pai + 1];
                NoFolha *dir = ler_folha(id_dir, grau_t);
                if (dir && dir->num_registros > grau_t - 1) {
                    folha_atual->registros[folha_atual->num_registros] = dir->registros[0];
                    folha_atual->num_registros++;
                    for (int i = 0; i < dir->num_registros - 1; i++)
                        dir->registros[i] = dir->registros[i+1];
                    dir->num_registros--;
                    strncpy(pai->chaves[pos_pai], dir->registros[0].chave, NOME_MAX);
                    salvar_folha(id_atual, folha_atual, grau_t);
                    salvar_folha(id_dir,   dir,         grau_t);
                    salvar_no_indice(id_pai, pai, grau_t);
                    balanceado = 1;
                }
                liberar_folha(dir);
            }

            /* 3. Merge com irmão */
            if (!balanceado) {
                if (pos_pai > 0) {
                    /* Merge: folha_atual vai para o irmão esquerdo */
                    int id_esq   = pai->filhos[pos_pai - 1];
                    NoFolha *esq = ler_folha(id_esq, grau_t);
                    for (int i = 0; i < folha_atual->num_registros; i++)
                        esq->registros[esq->num_registros + i] = folha_atual->registros[i];
                    esq->num_registros      += folha_atual->num_registros;
                    esq->id_proxima_folha    = folha_atual->id_proxima_folha;
                    /* Remove ponteiro e chave do pai */
                    for (int i = pos_pai - 1; i < pai->num_chaves - 1; i++) {
                        strncpy(pai->chaves[i], pai->chaves[i+1], NOME_MAX);
                        pai->filhos[i+1] = pai->filhos[i+2];
                    }
                    pai->num_chaves--;
                    salvar_folha(id_esq, esq, grau_t);
                    liberar_folha(esq);
                } else {
                    /* Merge: irmão direito vai para folha_atual */
                    int id_dir   = pai->filhos[pos_pai + 1];
                    NoFolha *dir = ler_folha(id_dir, grau_t);
                    for (int i = 0; i < dir->num_registros; i++)
                        folha_atual->registros[folha_atual->num_registros + i] = dir->registros[i];
                    folha_atual->num_registros  += dir->num_registros;
                    folha_atual->id_proxima_folha = dir->id_proxima_folha;
                    for (int i = pos_pai; i < pai->num_chaves - 1; i++) {
                        strncpy(pai->chaves[i], pai->chaves[i+1], NOME_MAX);
                        pai->filhos[i+1] = pai->filhos[i+2];
                    }
                    pai->num_chaves--;
                    salvar_folha(id_atual, folha_atual, grau_t);
                    liberar_folha(dir);
                }
                salvar_no_indice(id_pai, pai, grau_t);
            }
            liberar_folha(folha_atual);
            folha_atual = NULL; /* folha tratada — passamos para nível interno */

            /* Verifica se o pai agora tem underflow */
            if (balanceado || pai->num_chaves >= grau_t - 1 || stack_top == 0) {
                liberar_no_indice(pai, grau_t);
                return;
            }
        } else {
            /* Rebalancear nó INTERNO com underflow */
            /* (id_atual agora é o id do nó interno com underflow) */
            NoIndice *no_uf = ler_no_indice(id_atual, grau_t);
            if (!no_uf) { liberar_no_indice(pai, grau_t); return; }

            /* 1. Emprestar do nó interno esquerdo */
            if (pos_pai > 0 && !balanceado) {
                int id_esq    = pai->filhos[pos_pai - 1];
                NoIndice *esq = ler_no_indice(id_esq, grau_t);
                if (esq && esq->num_chaves > grau_t - 1) {
                    /* Rotação: desce chave do pai, sobe última do esquerdo */
                    for (int i = no_uf->num_chaves; i > 0; i--) {
                        strncpy(no_uf->chaves[i], no_uf->chaves[i-1], NOME_MAX);
                        no_uf->filhos[i+1] = no_uf->filhos[i];
                    }
                    no_uf->filhos[1] = no_uf->filhos[0];
                    strncpy(no_uf->chaves[0], pai->chaves[pos_pai-1], NOME_MAX);
                    no_uf->filhos[0] = esq->filhos[esq->num_chaves];
                    no_uf->num_chaves++;
                    strncpy(pai->chaves[pos_pai-1], esq->chaves[esq->num_chaves-1], NOME_MAX);
                    esq->num_chaves--;
                    salvar_no_indice(id_atual, no_uf, grau_t);
                    salvar_no_indice(id_esq,   esq,   grau_t);
                    salvar_no_indice(id_pai,   pai,   grau_t);
                    balanceado = 1;
                }
                liberar_no_indice(esq, grau_t);
            }

            /* 2. Emprestar do nó interno direito */
            if (pos_pai < pai->num_chaves && !balanceado) {
                int id_dir    = pai->filhos[pos_pai + 1];
                NoIndice *dir = ler_no_indice(id_dir, grau_t);
                if (dir && dir->num_chaves > grau_t - 1) {
                    strncpy(no_uf->chaves[no_uf->num_chaves], pai->chaves[pos_pai], NOME_MAX);
                    no_uf->filhos[no_uf->num_chaves + 1] = dir->filhos[0];
                    no_uf->num_chaves++;
                    strncpy(pai->chaves[pos_pai], dir->chaves[0], NOME_MAX);
                    for (int i = 0; i < dir->num_chaves - 1; i++) {
                        strncpy(dir->chaves[i], dir->chaves[i+1], NOME_MAX);
                        dir->filhos[i] = dir->filhos[i+1];
                    }
                    dir->filhos[dir->num_chaves - 1] = dir->filhos[dir->num_chaves];
                    dir->num_chaves--;
                    salvar_no_indice(id_atual, no_uf, grau_t);
                    salvar_no_indice(id_dir,   dir,   grau_t);
                    salvar_no_indice(id_pai,   pai,   grau_t);
                    balanceado = 1;
                }
                liberar_no_indice(dir, grau_t);
            }

            /* 3. Merge de nós internos */
            if (!balanceado) {
                if (pos_pai > 0) {
                    int id_esq    = pai->filhos[pos_pai - 1];
                    NoIndice *esq = ler_no_indice(id_esq, grau_t);
                    /* Desce chave do pai para o esquerdo */
                    strncpy(esq->chaves[esq->num_chaves], pai->chaves[pos_pai-1], NOME_MAX);
                    esq->filhos[esq->num_chaves + 1] = no_uf->filhos[0];
                    esq->num_chaves++;
                    for (int i = 0; i < no_uf->num_chaves; i++) {
                        strncpy(esq->chaves[esq->num_chaves], no_uf->chaves[i], NOME_MAX);
                        esq->filhos[esq->num_chaves + 1] = no_uf->filhos[i+1];
                        esq->num_chaves++;
                    }
                    for (int i = pos_pai - 1; i < pai->num_chaves - 1; i++) {
                        strncpy(pai->chaves[i], pai->chaves[i+1], NOME_MAX);
                        pai->filhos[i+1] = pai->filhos[i+2];
                    }
                    pai->num_chaves--;
                    salvar_no_indice(id_esq, esq, grau_t);
                    salvar_no_indice(id_pai, pai, grau_t);
                    liberar_no_indice(esq, grau_t);
                } else {
                    int id_dir    = pai->filhos[pos_pai + 1];
                    NoIndice *dir = ler_no_indice(id_dir, grau_t);
                    strncpy(no_uf->chaves[no_uf->num_chaves], pai->chaves[pos_pai], NOME_MAX);
                    no_uf->filhos[no_uf->num_chaves + 1] = dir->filhos[0];
                    no_uf->num_chaves++;
                    for (int i = 0; i < dir->num_chaves; i++) {
                        strncpy(no_uf->chaves[no_uf->num_chaves], dir->chaves[i], NOME_MAX);
                        no_uf->filhos[no_uf->num_chaves + 1] = dir->filhos[i+1];
                        no_uf->num_chaves++;
                    }
                    for (int i = pos_pai; i < pai->num_chaves - 1; i++) {
                        strncpy(pai->chaves[i], pai->chaves[i+1], NOME_MAX);
                        pai->filhos[i+1] = pai->filhos[i+2];
                    }
                    pai->num_chaves--;
                    salvar_no_indice(id_atual, no_uf, grau_t);
                    salvar_no_indice(id_pai,   pai,   grau_t);
                    liberar_no_indice(dir, grau_t);
                }
            }
            liberar_no_indice(no_uf, grau_t);
        }

        /* Se o pai ficou abaixo do mínimo e ainda temos ancestrais, continua */
        if (balanceado || pai->num_chaves >= grau_t - 1 || stack_top == 0) {
            /* Verifica se a raiz ficou vazia após merge */
            int raiz_id = ler_raiz_id();
            if (id_pai == raiz_id && pai->num_chaves == 0) {
                /* Nova raiz é o único filho restante */
                salvar_raiz_id(pai->filhos[0]);
            }
            liberar_no_indice(pai, grau_t);
            return;
        }

        id_atual = id_pai;
        liberar_no_indice(pai, grau_t);
    }
}

void remover_por_id(int id) {
    Registro *reg = buscar_por_id(id);
    if (reg) {
        char chave[NOME_MAX];
        strncpy(chave, reg->chave, NOME_MAX);
        if(reg->tipo == TIPO_FILME){
            limpar_relacionamentos_filme(id);
        }
        if(reg->tipo == TIPO_PESSOA){
            limpar_relacionamentos_pessoa(id);
        }
        remover_registro(chave);
        free(reg);
    } else {
        printf("Registro com ID %d nao encontrado.\n", id);
    }
}

/* -------------------------------------------------------
 * Impressão em formato de árvore
 * ------------------------------------------------------- */
static void imprimir_no_recursivo(int id_no, int nivel) {
    NoIndice *no = ler_no_indice(id_no, grau_t);
    if (!no) return;

    for (int i = 0; i < nivel; i++) printf("  ");
    printf("[No %d | chaves: %d]\n", id_no, no->num_chaves);
    for (int i = 0; i < no->num_chaves; i++) {
        for (int k = 0; k < nivel + 1; k++) printf("  ");
        printf("chave[%d]: \"%s\"\n", i, no->chaves[i]);
    }

    if (no->eh_folha) {
        for (int i = 0; i <= no->num_chaves; i++) {
            int id_f = no->filhos[i];
            if (id_f == -1) continue;
            NoFolha *folha = ler_folha(id_f, grau_t);
            if (!folha) continue;
            for (int k = 0; k < nivel + 1; k++) printf("  ");
            printf("Folha %d [%d regs]: ", id_f, folha->num_registros);
            for (int r = 0; r < folha->num_registros; r++) {
                int rid = (folha->registros[r].tipo == TIPO_PESSOA)
                          ? folha->registros[r].dados.pessoa.id_pessoa
                          : folha->registros[r].dados.filme.id_filme;
                printf("[%d]\"%s\"", rid, folha->registros[r].chave);
                if (r < folha->num_registros - 1) printf(" | ");
            }
            printf("\n");
            liberar_folha(folha);
        }
    } else {
        for (int i = 0; i <= no->num_chaves; i++) {
            if (no->filhos[i] != -1)
                imprimir_no_recursivo(no->filhos[i], nivel + 1);
        }
    }
    liberar_no_indice(no, grau_t);
}

void imprimir_arvore(void) {
    if (grau_t == -1) grau_t = ler_grau_t();
    int raiz = ler_raiz_id();
    printf("\n=== ARVORE B+ (t=%d) ===\n", grau_t);
    imprimir_no_recursivo(raiz, 0);
    printf("========================\n");
}
