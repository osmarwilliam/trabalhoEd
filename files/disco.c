#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "disco.h"

#define ARQ_INDICES "dados/indices.bin"

/* -------------------------------------------------------
 * Cabeçalho do arquivo de índices
 * Campos persistidos: grau_t, id da raiz, próximo id de
 * nó interno, próximo id de folha, próximo id global de
 * Pessoa/Filme (para não repetir IDs entre sessões).
 * ------------------------------------------------------- */
typedef struct {
    int grau_t;
    int raiz_id;
    int prox_id_indice;
    int prox_id_folha;
    int prox_id_global;   /* NOVO: persiste contador de IDs entre sessões */
} CabecalhoIndices;

/* Tamanho fixo de um nó índice serializado:
 *   2 ints (eh_folha, num_chaves)
 *   (2t-1) strings de NOME_MAX bytes
 *   (2t)   ints (filhos)
 * Exportado via disco.h para uso em salvar/ler.
 */
long tamanho_no_indice_disco(int t) {
    return (long)(2 * sizeof(int))
         + (long)(2*t - 1) * NOME_MAX
         + (long)(2*t)     * sizeof(int);
}

/* ---- helpers internos ---- */

static CabecalhoIndices ler_cabecalho(void) {
    CabecalhoIndices cab = {0, -1, 0, 0, 1};
    FILE *f = fopen(ARQ_INDICES, "rb");
    if (f) {
        fread(&cab, sizeof(CabecalhoIndices), 1, f);
        fclose(f);
    }
    return cab;
}

static void salvar_cabecalho(CabecalhoIndices *cab) {
    FILE *f = fopen(ARQ_INDICES, "rb+");
    if (!f) f = fopen(ARQ_INDICES, "wb");
    if (!f) return;
    fseek(f, 0, SEEK_SET);
    fwrite(cab, sizeof(CabecalhoIndices), 1, f);
    fflush(f);
    fclose(f);
}

/* ---- API pública ---- */

void inicializar_disco(int t) {
    FILE *f = fopen(ARQ_INDICES, "rb");
    if (!f) {
        /* Arquivo novo: cria cabeçalho */
        CabecalhoIndices cab = {t, -1, 0, 0, 1};
        f = fopen(ARQ_INDICES, "wb");
        if (!f) { perror("Erro ao criar indices.bin"); return; }
        fwrite(&cab, sizeof(CabecalhoIndices), 1, f);
        fclose(f);
    } else {
        /* Arquivo existente: usa o t que já está gravado (ignora o parâmetro) */
        fclose(f);
    }
}

int ler_grau_t(void) {
    return ler_cabecalho().grau_t;
}

int ler_raiz_id(void) {
    return ler_cabecalho().raiz_id;
}

void salvar_raiz_id(int raiz_id) {
    CabecalhoIndices cab = ler_cabecalho();
    cab.raiz_id = raiz_id;
    salvar_cabecalho(&cab);
}

int ler_proximo_id_global(void) {
    return ler_cabecalho().prox_id_global;
}

void salvar_proximo_id_global(int id) {
    CabecalhoIndices cab = ler_cabecalho();
    cab.prox_id_global = id;
    salvar_cabecalho(&cab);
}

int gerar_novo_id_indice(void) {
    CabecalhoIndices cab = ler_cabecalho();
    int id = cab.prox_id_indice++;
    salvar_cabecalho(&cab);
    return id;
}

int gerar_novo_id_folha(void) {
    CabecalhoIndices cab = ler_cabecalho();
    int id = cab.prox_id_folha++;
    salvar_cabecalho(&cab);
    return id;
}

/* ---- Nós de índice ---- */

void salvar_no_indice(int id_no, NoIndice *no, int t) {
    FILE *f = fopen(ARQ_INDICES, "rb+");
    if (!f) { perror("salvar_no_indice: fopen"); return; }

    long offset = (long)sizeof(CabecalhoIndices)
                + (long)id_no * tamanho_no_indice_disco(t);
    if (fseek(f, offset, SEEK_SET) != 0) {
        perror("salvar_no_indice: fseek"); fclose(f); return;
    }

    /* eh_folha */
    if (fwrite(&no->eh_folha,   sizeof(int), 1, f) != 1) goto erro;
    /* num_chaves */
    if (fwrite(&no->num_chaves, sizeof(int), 1, f) != 1) goto erro;
    /* chaves: sempre 2t-1 slots de NOME_MAX bytes */
    for (int i = 0; i < 2*t - 1; i++) {
        char buf[NOME_MAX] = {0};
        if (i < no->num_chaves) strncpy(buf, no->chaves[i], NOME_MAX - 1);
        if (fwrite(buf, NOME_MAX, 1, f) != 1) goto erro;
    }
    /* filhos: sempre 2t slots */
    for (int i = 0; i < 2*t; i++) {
        int filho = (i <= no->num_chaves) ? no->filhos[i] : -1;
        if (fwrite(&filho, sizeof(int), 1, f) != 1) goto erro;
    }
    fflush(f);
    fclose(f);
    return;
erro:
    perror("salvar_no_indice: fwrite");
    fclose(f);
}

NoIndice *ler_no_indice(int id_no, int t) {
    FILE *f = fopen(ARQ_INDICES, "rb");
    if (!f) { perror("ler_no_indice: fopen"); return NULL; }

    long offset = (long)sizeof(CabecalhoIndices)
                + (long)id_no * tamanho_no_indice_disco(t);
    if (fseek(f, offset, SEEK_SET) != 0) {
        perror("ler_no_indice: fseek"); fclose(f); return NULL;
    }

    NoIndice *no = (NoIndice *)malloc(sizeof(NoIndice));
    if (!no) { fclose(f); return NULL; }

    no->chaves = (char **)malloc((2*t - 1) * sizeof(char *));
    for (int i = 0; i < 2*t - 1; i++)
        no->chaves[i] = (char *)calloc(NOME_MAX, 1);
    no->filhos = (int *)malloc((2*t) * sizeof(int));

    if (fread(&no->eh_folha,   sizeof(int), 1, f) != 1) goto erro;
    if (fread(&no->num_chaves, sizeof(int), 1, f) != 1) goto erro;
    for (int i = 0; i < 2*t - 1; i++) {
        if (fread(no->chaves[i], NOME_MAX, 1, f) != 1) goto erro;
    }
    for (int i = 0; i < 2*t; i++) {
        if (fread(&no->filhos[i], sizeof(int), 1, f) != 1) goto erro;
    }
    fclose(f);
    return no;

erro:
    perror("ler_no_indice: fread");
    fclose(f);
    liberar_no_indice(no, t);
    return NULL;
}

void liberar_no_indice(NoIndice *no, int t) {
    if (!no) return;
    if (no->chaves) {
        for (int i = 0; i < 2*t - 1; i++) free(no->chaves[i]);
        free(no->chaves);
    }
    free(no->filhos);
    free(no);
}

/* ---- Folhas (um arquivo por folha) ---- */

static void nome_arquivo_folha(char *buf, int id_folha) {
    snprintf(buf, 64, "dados/folha_%d.bin", id_folha);
}

void salvar_folha(int id_folha, NoFolha *folha, int t) {
    char nome[64];
    nome_arquivo_folha(nome, id_folha);
    FILE *f = fopen(nome, "wb");
    if (!f) { perror("salvar_folha: fopen"); return; }

    if (fwrite(&folha->num_registros,     sizeof(int), 1, f) != 1) goto erro;
    if (fwrite(&folha->id_proxima_folha,  sizeof(int), 1, f) != 1) goto erro;

    for (int i = 0; i < 2*t - 1; i++) {
        Registro vazia;
        memset(&vazia, 0, sizeof(Registro));
        const Registro *r = (i < folha->num_registros) ? &folha->registros[i] : &vazia;
        if (fwrite(r, sizeof(Registro), 1, f) != 1) goto erro;
    }
    fclose(f);
    return;
erro:
    perror("salvar_folha: fwrite");
    fclose(f);
}

NoFolha *ler_folha(int id_folha, int t) {
    char nome[64];
    nome_arquivo_folha(nome, id_folha);
    FILE *f = fopen(nome, "rb");
    if (!f) return NULL;

    NoFolha *folha = (NoFolha *)malloc(sizeof(NoFolha));
    if (!folha) { fclose(f); return NULL; }
    folha->registros = (Registro *)malloc((2*t - 1) * sizeof(Registro));
    if (!folha->registros) { free(folha); fclose(f); return NULL; }

    if (fread(&folha->num_registros,    sizeof(int), 1, f) != 1) goto erro;
    if (fread(&folha->id_proxima_folha, sizeof(int), 1, f) != 1) goto erro;
    for (int i = 0; i < 2*t - 1; i++) {
        if (fread(&folha->registros[i], sizeof(Registro), 1, f) != 1) goto erro;
    }
    fclose(f);
    return folha;

erro:
    perror("ler_folha: fread");
    fclose(f);
    liberar_folha(folha);
    return NULL;
}

void liberar_folha(NoFolha *folha) {
    if (!folha) return;
    free(folha->registros);
    free(folha);
}
