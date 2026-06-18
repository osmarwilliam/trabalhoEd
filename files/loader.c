#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "loader.h"
#include "arvoreBmais.h"
#include "hash_relacionamento.h"
#include "disco.h"
#include "structs.h"

 //ID global — lido do disco ao iniciar, salvo a cada uso.
 //Isso evita colisão de IDs entre sessões.

static int proximo_id_global = 1;

static void sincronizar_id_global(void) {
    int salvo = ler_proximo_id_global();
    if (salvo > proximo_id_global) proximo_id_global = salvo;
}

static int proximo_id(void) {
    int id = proximo_id_global++;
    salvar_proximo_id_global(proximo_id_global);
    return id;
}

//Remove espaços/tabs/\r\n das bordas de uma string

void remover_espacos_extras(char *str) {
    if (!str || *str == '\0') return;

    int inicio = 0;
    while (str[inicio] == ' ' || str[inicio] == '\t' || str[inicio] == '\r' || str[inicio] == '\n')
        inicio++;

    int fim = (int)strlen(str) - 1;
    while (fim >= inicio && (str[fim] == ' ' || str[fim] == '\t' || str[fim] == '\r' || str[fim] == '\n'))
        fim--;

    int j = 0;
    for (int i = inicio; i <= fim; i++) str[j++] = str[i];
    str[j] = '\0';
}

 // Helpers para obter IDs consultando a árvore B+

static int obter_id_filme(const char *titulo) {
    Registro *reg = buscar_por_nome(titulo);
    if (reg && reg->tipo == TIPO_FILME) {
        int id = reg->dados.filme.id_filme;
        free(reg);
        return id;
    }
    if (reg) free(reg);
    return -1;
}

static int obter_id_pessoa(const char *nome) {
    Registro *reg = buscar_por_nome(nome);
    if (reg && reg->tipo == TIPO_PESSOA) {
        int id = reg->dados.pessoa.id_pessoa;
        free(reg);
        return id;
    }
    if (reg) free(reg);
    return -1;
}

// Processa uma linha do Nodes.txt
// Formatos:
//   Movie | titulo | ano | tagline
//  Person | nome | ano_nascimento
static void processar_node(char *buffer) {
    char *token = strtok(buffer, "|");
    if (!token)
        return;

    char tipo[50] = {0};
    strncpy(tipo, token, 49);
    remover_espacos_extras(tipo);

    if (strstr(tipo, "Movie")) {
        char *titulo_tok = strtok(NULL, "|");
        char *ano_tok    = strtok(NULL, "|");
        if (!titulo_tok) return;

        Filme f;
        memset(&f, 0, sizeof(f));
        f.id_filme = proximo_id();

        strncpy(f.titulo, titulo_tok, NOME_MAX - 1);
        remover_espacos_extras(f.titulo);

        /* Valida ano: atoi("(no tagline)") = 0, o que é aceitável */
        if (ano_tok) f.ano_lancamento = atoi(ano_tok);

        /* Verifica duplicidade de nome antes de inserir */
        Registro *existe = buscar_por_nome(f.titulo);
        if (existe) { free(existe); return; }

        inserir_filme(f);

    } else if (strstr(tipo, "Person")) {
        char *nome_tok = strtok(NULL, "|");
        char *ano_tok  = strtok(NULL, "|");
        if (!nome_tok) return;

        Pessoa p;
        memset(&p, 0, sizeof(p));
        p.id_pessoa = proximo_id();

        strncpy(p.nome, nome_tok, NOME_MAX - 1);
        remover_espacos_extras(p.nome);

        /* "(no birth year)" → atoi = 0 → salvo como 0, ok */
        if (ano_tok) {
            remover_espacos_extras(ano_tok);
            p.ano_nascimento = atoi(ano_tok);
        }

        Registro *existe = buscar_por_nome(p.nome);
        if (existe) { free(existe); return; }

        inserir_pessoa(p);
    }
}

// Processa uma linha do Relationships.txt
//Formatos possíveis:
//  START Person | <nome> | ACTED_IN | END Movie | <titulo> | role: <papel>
//   START Person | <nome> | DIRECTED | END Movie | <titulo>
//   START Person | <nome> | PRODUCED | END Movie | <titulo>
//   START Person | <nome> | WROTE    | END Movie | <titulo>
static void processar_relacionamento(char *buffer) {
    /* Localiza "START Person | " */
    char *ptr_pessoa = strstr(buffer, "START Person | ");
    if (!ptr_pessoa) return;
    ptr_pessoa += 15;  /* pula "START Person | " */

    /* Nome da pessoa: vai até o próximo " | " */
    char *sep = strstr(ptr_pessoa, " | ");
    if (!sep) return;
    char nome_pessoa[NOME_MAX] = {0};
    size_t len = (size_t)(sep - ptr_pessoa);
    if (len >= NOME_MAX) len = NOME_MAX - 1;
    strncpy(nome_pessoa, ptr_pessoa, len);
    remover_espacos_extras(nome_pessoa);

    /* Verbo/papel: avança " | " e pega até o próximo " | " */
    char *ptr_verbo = sep + 3;
    char *sep2 = strstr(ptr_verbo, " | ");
    if (!sep2) return;
    char verbo[32] = {0};
    len = (size_t)(sep2 - ptr_verbo);
    if (len >= 32) len = 31;
    strncpy(verbo, ptr_verbo, len);
    remover_espacos_extras(verbo);

    char papel = 'O';
    if (strcmp(verbo, "ACTED_IN") == 0) papel = 'A';
    else if (strcmp(verbo, "DIRECTED") == 0) papel = 'D';
    else if (strcmp(verbo, "PRODUCED") == 0) papel = 'P';
    else if (strcmp(verbo, "WROTE")    == 0) papel = 'W';
    else if (strcmp(verbo, "REVIEWED") == 0) papel = 'R';

    /* Localiza "END Movie | " */
    char *ptr_filme = strstr(sep2, "END Movie | ");
    if (!ptr_filme) return;
    ptr_filme += 12;  /* pula "END Movie | " */

    /* Título: vai até " | role" ou fim da string */
    char *sep_role = strstr(ptr_filme, " | role");
    char titulo_filme[NOME_MAX] = {0};
    if (sep_role) {
        len = (size_t)(sep_role - ptr_filme);
    } else {
        len = strlen(ptr_filme);
    }
    if (len >= NOME_MAX) len = NOME_MAX - 1;
    strncpy(titulo_filme, ptr_filme, len);
    remover_espacos_extras(titulo_filme);

    /* Consulta IDs na árvore B+ */
    int id_p = obter_id_pessoa(nome_pessoa);
    int id_f = obter_id_filme(titulo_filme);

    if (id_p == -1 || id_f == -1) {
        printf("  Aviso: nao inserido — '%s' / '%s' nao encontrados na arvore.\n",
               nome_pessoa, titulo_filme);
        return;
    }

    /* Monta relacionamento com nomes em cache para evitar I/O nas consultas */
    Relacionamento rel;
    memset(&rel, 0, sizeof(rel));
    rel.id_pessoa = id_p;
    rel.id_filme  = id_f;
    rel.papel     = papel;
    strncpy(rel.nome_pessoa,  nome_pessoa,  NOME_MAX - 1);
    strncpy(rel.titulo_filme, titulo_filme, NOME_MAX - 1);

    inserir_relacionamento(rel);
}

// Ponto de entrada público
void carregar_dados_iniciais(void) {
    /* Garante que proximo_id_global está sincronizado com disco */
    sincronizar_id_global();

    printf("Carregando Nodes.txt...\n");
    FILE *fn = fopen("Nodes.txt", "r");
    if (fn) {
        char buffer[1024];
        int contagem = 0;
        while (fgets(buffer, sizeof(buffer), fn)) {
            buffer[strcspn(buffer, "\r\n")] = 0;
            if (strlen(buffer) == 0) continue;
            processar_node(buffer);
            contagem++;
        }
        fclose(fn);
        printf("  %d linhas processadas.\n", contagem);
    } else {
        printf("  Nodes.txt nao encontrado.\n");
    }

    printf("Carregando Relationships.txt...\n");
    FILE *fr = fopen("Relationships.txt", "r");
    if (fr) {
        char buffer[1024];
        int contagem = 0;
        while (fgets(buffer, sizeof(buffer), fr)) {
            buffer[strcspn(buffer, "\r\n")] = 0;
            if (strlen(buffer) == 0) continue;
            processar_relacionamento(buffer);
            contagem++;
        }
        fclose(fr);
        printf("  %d relacionamentos processados.\n", contagem);
    } else {
        printf("  Relationships.txt nao encontrado.\n");
    }
}
