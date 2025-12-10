#ifndef LINHA_H
#define LINHA_H

#include <semaphore.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>

#define CAPACIDADE_ESTEIRA 20

typedef enum
{
    BOLACHA_CRUA = 0,
    BOLACHA_ASSADA,
    BOLACHA_EMBALADA,
    BOLACHA_FINALIZADA
} EstadoBolacha;

typedef struct
{
    int id;
    EstadoBolacha estado;
} Bolacha;

typedef struct
{
    char nome[32];

    Bolacha fila[CAPACIDADE_ESTEIRA];
    int head;  // índice de remoção
    int tail;  // índice de inserção
    int count; // quantos itens na fila

    sem_t mutex;      // exclusão mútua
    sem_t tem_item;   // contador de itens
    sem_t tem_espaco; // contador de espaços livres
} Esteira;

typedef struct
{
    Esteira e_forno;     // Esteira 1: leva para o forno
    Esteira e_embalagem; // Esteira 2: após forno, separação/embalagem
    Esteira e_caixa;     // Esteira 3: leva para caixa destino final

    int proximo_id; // ID incremental das bolachas

    int total_criadas;
    int total_finalizadas;

    bool rodando;

    pthread_mutex_t estado_mutex;
} LinhaProducao;

// Configuração do sistema
typedef struct
{
    int tempo_verificacao_sensor_ms; // em milissegundos
    int hora_ativacao;
    int minuto_ativacao;
    int hora_desativacao;
    int minuto_desativacao;
    int max_tentativas_acesso;
} ConfigSistema;

// Configuração global
extern ConfigSistema g_config;

// API da linha
void linha_inicializar(LinhaProducao *lp);
void linha_destruir(LinhaProducao *lp);

// Operações de esteira (FIFO)
void esteira_inicializar(Esteira *e, const char *nome);
void esteira_destruir(Esteira *e);
void esteira_push(Esteira *e, Bolacha b);
Bolacha esteira_pop(Esteira *e);

// Threads (tarefas)
void *tarefa_produtor(void *arg);  // cria bolachas e coloca na esteira do forno
void *tarefa_forno(void *arg);     // "assa" bolacha (esteira forno -> embalagem)
void *tarefa_embalagem(void *arg); // "embala" (esteira embalagem -> caixa)
void *tarefa_caixa(void *arg);     // consome bolacha final na esteira caixa
void *tarefa_monitor(void *arg);   // monitora sensores / estado e horário

// Utilitários
void linha_set_rodando(LinhaProducao *lp, bool valor);
bool linha_get_rodando(LinhaProducao *lp);
void linha_imprimir_estado(LinhaProducao *lp);

// Configuração
void config_carregar(const char *path);

// Log
void log_inicializar(const char *path);
void log_finalizar(void);
void log_escrever(const char *nivel, const char *fmt, ...);

#endif // LINHA_H
