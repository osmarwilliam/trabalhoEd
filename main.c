#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "loader.h"
#include "arvoreBmais.h"
#include "hash_relacionamento.h"
#include "disco.h"
#include "structs.h"

static void limpar_buffer(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

/* -------------------------------------------------------
 * Inserção manual de relacionamento via menu
 * ------------------------------------------------------- */
static void menu_inserir_relacionamento(void) {
    Relacionamento rel;
    memset(&rel, 0, sizeof(rel));

    printf("ID do Filme: ");
    if (scanf("%d", &rel.id_filme) != 1) { limpar_buffer(); return; }
    limpar_buffer();

    printf("ID da Pessoa: ");
    if (scanf("%d", &rel.id_pessoa) != 1) { limpar_buffer(); return; }
    limpar_buffer();

    printf("Papel (A=Ator, D=Diretor, P=Produtor, W=Escritor): ");
    if (scanf("%c", &rel.papel) != 1) { limpar_buffer(); return; }
    limpar_buffer();

    /* Preenche campos de cache consultando a árvore */
    Registro *rp = buscar_por_id(rel.id_pessoa);
    Registro *rf = buscar_por_id(rel.id_filme);
    if (!rp || !rf) {
        printf("Erro: ID de pessoa ou filme nao encontrado na arvore.\n");
        if (rp) free(rp);
        if (rf) free(rf);
        return;
    }
    strncpy(rel.nome_pessoa,  rp->dados.pessoa.nome,   NOME_MAX - 1);
    strncpy(rel.titulo_filme, rf->dados.filme.titulo,  NOME_MAX - 1);
    free(rp); free(rf);

    inserir_relacionamento(rel);
    printf("Relacionamento inserido com sucesso.\n");
}

/* -------------------------------------------------------
 * Menu de consultas analíticas (a)–(t)
 * ------------------------------------------------------- */
static void menu_consultas(void) {
    printf("\n=== CONSULTAS ANALITICAS ===\n");
    printf(" (a) Todos que trabalharam juntos\n");
    printf(" (b) Atores e diretores juntos\n");
    printf(" (c) Atores que atuaram juntos\n");
    printf(" (d) Atores que mais atuaram juntos por decada\n");
    printf(" (e) Atores e diretores juntos por decada\n");
    printf(" (f) Atores que mais atuaram (com filmes)\n");
    printf(" (g) Atores que menos atuaram (com filmes)\n");
    printf(" (h) Diretores que mais dirigiram\n");
    printf(" (i) Diretores que menos dirigiram\n");
    printf(" (j) Produtores mais atuantes\n");
    printf(" (k) Produtores menos atuantes\n");
    printf(" (l) Consultas f-k por decada\n");
    printf(" (m) Filmes que sao continuacoes\n");
    printf(" (n) Atores nascidos no mesmo ano\n");
    printf(" (o) Atores que ja dirigiram\n");
    printf(" (p) Atores que ja produziram\n");
    printf(" (q) Retirar todos de um filme\n");
    printf(" (r) Escreveu, dirigiu e produziu\n");
    printf(" (s) Dirigiu e produziu\n");
    printf(" (t) Ator nascido no ano de lancamento de um filme\n");
    printf(" (0) Voltar\n");
    printf("Escolha: ");

    char letra[4];
    if (scanf("%3s", letra) != 1) { limpar_buffer(); return; }
    limpar_buffer();

    switch (letra[0]) {
        case 'a': case 'A': consulta_a_todos_juntos();              break;
        case 'b': case 'B': consulta_b_atores_diretores();          break;
        case 'c': case 'C': consulta_c_atores_juntos();             break;
        case 'd': case 'D': consulta_d_atores_juntos_decada();      break;
        case 'e': case 'E': consulta_e_ator_diretor_decada();       break;
        case 'f': case 'F': consulta_f_atores_mais_atuaram();       break;
        case 'g': case 'G': consulta_g_atores_menos_atuaram();      break;
        case 'h': case 'H': consulta_h_diretores_mais();            break;
        case 'i': case 'I': consulta_i_diretores_menos();           break;
        case 'j': case 'J': consulta_j_produtores_mais();           break;
        case 'k': case 'K': consulta_k_produtores_menos();          break;
        case 'l': case 'L': consulta_l_f_a_k_por_decada();         break;
        case 'm': case 'M': consulta_m_filmes_continuacao();        break;
        case 'n': case 'N': consulta_n_atores_mesmo_ano();          break;
        case 'o': case 'O': consulta_o_atores_que_dirigiram();      break;
        case 'p': case 'P': consulta_p_atores_que_produziram();     break;
        case 'q': case 'Q': {
            int id_f;
            printf("ID do Filme para remover todos os relacionamentos: ");
            if (scanf("%d", &id_f) == 1) consulta_q_retirar_todos_do_filme(id_f);
            limpar_buffer();
            break;
        }
        case 'r': case 'R': consulta_r_escreveu_dirigiu_produziu(); break;
        case 's': case 'S': consulta_s_dirigiu_e_produziu();        break;
        case 't': case 'T': consulta_t_ator_nasceu_no_ano_do_filme(); break;
        case '0':           break;
        default:            printf("Opcao invalida.\n");
    }
}

/* -------------------------------------------------------
 * main
 * ------------------------------------------------------- */
int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);

    /* Cria diretório "dados" se não existir */
    struct stat st = {0};
    if (stat("dados", &st) == -1) {
#if defined(_WIN32)
        mkdir("dados");
#else
        mkdir("dados", 0700);
#endif
    }

    printf("\n========== SISTEMA HOLLYWOOD ==========\n");

    int t_salvo = ler_grau_t();
    if (t_salvo > 0) {
        printf("Banco de dados existente encontrado (t = %d).\n", t_salvo);
        inicializar_arvore(t_salvo);
        inicializar_arquivo_hash();
    } else {
        int novo_t;
        printf("Banco de dados nao encontrado.\n");
        printf("Defina o grau minimo t da Arvore B+ (t >= 2): ");
        if (scanf("%d", &novo_t) != 1) return 1;
        limpar_buffer();
        if (novo_t < 2) novo_t = 2;

        inicializar_arvore(novo_t);
        inicializar_arquivo_hash();
        carregar_dados_iniciais();
    }

    int opcao;
    do {
        printf("\n========== MENU PRINCIPAL ==========\n");
        printf("1. Inserir pessoa ou filme na arvore B+\n");
        printf("2. Remover pessoa ou filme (por ID)\n");
        printf("3. Buscar pelo nome/titulo\n");
        printf("4. Buscar pelo ID\n");
        printf("5. Imprimir a arvore B+ (formato arvore)\n");
        printf("6. Inserir relacionamento no hash\n");
        printf("7. Buscar relacionamentos de um filme\n");
        printf("8. Buscar filmes de uma pessoa\n");
        printf("9. Consultas analiticas (a-t)\n");
        printf("0. Sair\n");
        printf("Escolha: ");

        int scan_res = scanf("%d", &opcao);
        if (scan_res == EOF) break;
        if (scan_res != 1) { limpar_buffer(); opcao = -1; continue; }
        limpar_buffer();

        switch (opcao) {

            case 1: {
                int tipo;
                printf("1 = Pessoa  2 = Filme: ");
                if (scanf("%d", &tipo) != 1) { limpar_buffer(); break; }
                limpar_buffer();

                if (tipo == 1) {
                    Pessoa p; memset(&p, 0, sizeof(p));
                    printf("Nome: "); fgets(p.nome, NOME_MAX, stdin);
                    p.nome[strcspn(p.nome, "\n")] = 0;

                    /* Verifica duplicidade de nome */
                    Registro *dup = buscar_por_nome(p.nome);
                    if (dup) {
                        printf("Erro: ja existe registro com o nome '%s'.\n", p.nome);
                        free(dup); break;
                    }

                    printf("Ano de Nascimento: ");
                    scanf("%d", &p.ano_nascimento); limpar_buffer();

                    /* Gera ID automaticamente e persiste */
                    int novo_id = ler_proximo_id_global();
                    salvar_proximo_id_global(novo_id + 1);
                    p.id_pessoa = novo_id;

                    inserir_pessoa(p);
                    printf("Pessoa '%s' (ID %d) inserida.\n", p.nome, p.id_pessoa);

                } else if (tipo == 2) {
                    Filme f; memset(&f, 0, sizeof(f));
                    printf("Titulo: "); fgets(f.titulo, NOME_MAX, stdin);
                    f.titulo[strcspn(f.titulo, "\n")] = 0;

                    Registro *dup = buscar_por_nome(f.titulo);
                    if (dup) {
                        printf("Erro: ja existe registro com o titulo '%s'.\n", f.titulo);
                        free(dup); break;
                    }

                    printf("Ano de Lancamento: ");
                    scanf("%d", &f.ano_lancamento); limpar_buffer();

                    int novo_id = ler_proximo_id_global();
                    salvar_proximo_id_global(novo_id + 1);
                    f.id_filme = novo_id;

                    inserir_filme(f);
                    printf("Filme '%s' (ID %d) inserido.\n", f.titulo, f.id_filme);
                } else {
                    printf("Opcao invalida.\n");
                }
                break;
            }

            case 2: {
                int id;
                printf("ID do registro a remover: ");
                if (scanf("%d", &id) != 1) { limpar_buffer(); break; }
                limpar_buffer();
                remover_por_id(id);
                break;
            }

            case 3: {
                char nome[NOME_MAX];
                printf("Nome ou titulo: ");
                fgets(nome, NOME_MAX, stdin);
                nome[strcspn(nome, "\n")] = 0;

                Registro *reg = buscar_por_nome(nome);
                if (reg) {
                    printf("--- Encontrado ---\n");
                    if (reg->tipo == TIPO_PESSOA) {
                        printf("Tipo: Pessoa\nID: %d\nNome: %s\nAno Nasc.: %d\n",
                               reg->dados.pessoa.id_pessoa,
                               reg->dados.pessoa.nome,
                               reg->dados.pessoa.ano_nascimento);
                    } else {
                        printf("Tipo: Filme\nID: %d\nTitulo: %s\nAno: %d\n",
                               reg->dados.filme.id_filme,
                               reg->dados.filme.titulo,
                               reg->dados.filme.ano_lancamento);
                    }
                    free(reg);
                } else {
                    printf("Nao encontrado.\n");
                }
                break;
            }

            case 4: {
                int id;
                printf("ID: ");
                if (scanf("%d", &id) != 1) { limpar_buffer(); break; }
                limpar_buffer();

                Registro *reg = buscar_por_id(id);
                if (reg) {
                    if (reg->tipo == TIPO_PESSOA) {
                        printf("Tipo: Pessoa\nID: %d\nNome: %s\nAno Nasc.: %d\n",
                               reg->dados.pessoa.id_pessoa,
                               reg->dados.pessoa.nome,
                               reg->dados.pessoa.ano_nascimento);
                    } else {
                        printf("Tipo: Filme\nID: %d\nTitulo: %s\nAno: %d\n",
                               reg->dados.filme.id_filme,
                               reg->dados.filme.titulo,
                               reg->dados.filme.ano_lancamento);
                    }
                    free(reg);
                } else {
                    printf("ID %d nao encontrado.\n", id);
                }
                break;
            }

            case 5:
                imprimir_arvore();
                break;

            case 6:
                menu_inserir_relacionamento();
                break;

            case 7: {
                int id;
                printf("ID do Filme: ");
                if (scanf("%d", &id) != 1) { limpar_buffer(); break; }
                limpar_buffer();
                buscar_pessoas_do_filme(id);
                break;
            }

            case 8: {
                int id;
                printf("ID da Pessoa: ");
                if (scanf("%d", &id) != 1) { limpar_buffer(); break; }
                limpar_buffer();
                buscar_filmes_da_pessoa(id);
                break;
            }

            case 9:
                menu_consultas();
                break;

            case 0:
                printf("Saindo...\n");
                break;

            default:
                printf("Opcao invalida.\n");
        }

    } while (opcao != 0);

    return 0;
}
