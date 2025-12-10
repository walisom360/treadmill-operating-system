#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

#include "linha.h"

// Interface de linha de comando simples
// Comandos:
//   s  -> mostrar estado
//   p  -> pausar linha
//   r  -> retomar linha
//   q  -> sair

static void imprimir_menu()
{
    printf("=== MENU ===\n");
    printf("s - Mostrar estado da linha\n");
    printf("p - Pausar linha\n");
    printf("r - Retomar linha\n");
    printf("q - Sair\n");
    printf("Digite o comando: ");
    fflush(stdout);
}

int main(void)
{
    // 1) Inicia log
    log_inicializar("log.txt");

    // 2) Carrega configuração
    config_carregar("config.txt");

    // 3) Inicializa a linha
    LinhaProducao lp;
    linha_inicializar(&lp);

    pthread_t th_produtor, th_forno, th_embalagem, th_caixa, th_monitor;

    // Cria as tarefas (threads)
    if (pthread_create(&th_produtor, NULL, tarefa_produtor, &lp) != 0)
    {
        perror("pthread_create produtor");
        exit(1);
    }
    if (pthread_create(&th_forno, NULL, tarefa_forno, &lp) != 0)
    {
        perror("pthread_create forno");
        exit(1);
    }
    if (pthread_create(&th_embalagem, NULL, tarefa_embalagem, &lp) != 0)
    {
        perror("pthread_create embalagem");
        exit(1);
    }
    if (pthread_create(&th_caixa, NULL, tarefa_caixa, &lp) != 0)
    {
        perror("pthread_create caixa");
        exit(1);
    }
    if (pthread_create(&th_monitor, NULL, tarefa_monitor, &lp) != 0)
    {
        perror("pthread_create monitor");
        exit(1);
    }

    // Loop da interface de usuário
    char linha[32];
    int comandos_invalidos = 0;

    while (1)
    {
        imprimir_menu();

        if (!fgets(linha, sizeof(linha), stdin))
        {
            break;
        }

        // drop newline
        linha[strcspn(linha, "\r\n")] = '\0';

        if (strcmp(linha, "s") == 0)
        {
            linha_imprimir_estado(&lp);
            log_escrever("INFO", "Comando 's' - mostrar estado");
        }
        else if (strcmp(linha, "p") == 0)
        {
            linha_set_rodando(&lp, false);
            printf(">> Linha pausada.\n");
            log_escrever("INFO", "Comando 'p' - linha pausada pelo usuario");
        }
        else if (strcmp(linha, "r") == 0)
        {
            linha_set_rodando(&lp, true);
            printf(">> Linha retomada.\n");
            log_escrever("INFO", "Comando 'r' - linha retomada pelo usuario");
        }
        else if (strcmp(linha, "q") == 0)
        {
            printf(">> Encerrando o sistema...\n");
            log_escrever("INFO", "Comando 'q' - encerrando sistema pelo usuario");
            break;
        }
        else if (linha[0] == '\0')
        {
            continue;
        }
        else
        {
            comandos_invalidos++;
            printf("Comando desconhecido: %s\n", linha);
            log_escrever("ERRO", "Comando invalido: '%s' (tentativa %d)",
                         linha, comandos_invalidos);

            if (comandos_invalidos >= g_config.max_tentativas_acesso)
            {
                log_escrever("ERRO",
                             "Numero maximo de tentativas de acesso/comandos invalidos atingido (%d)",
                             g_config.max_tentativas_acesso);
            }
        }
    }

    // MVP: não fazemos join, só destruímos estruturas e fechamos log
    linha_destruir(&lp);
    log_escrever("INFO", "Sistema encerrado.");
    log_finalizar();

    printf("Sistema encerrado.\n");
    return 0;
}
