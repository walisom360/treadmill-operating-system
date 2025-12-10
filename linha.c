#include "linha.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdarg.h>
#include <stdlib.h>

ConfigSistema g_config;

// Log global
static FILE *g_log_file = NULL;
static pthread_mutex_t g_log_mutex = PTHREAD_MUTEX_INITIALIZER;

// ---------- Funções de Log ----------

static void log_timestamp(FILE *f)
{
    time_t t = time(NULL);
    struct tm *lt = localtime(&t);
    if (lt)
    {
        fprintf(f, "%04d-%02d-%02d %02d:%02d:%02d",
                lt->tm_year + 1900,
                lt->tm_mon + 1,
                lt->tm_mday,
                lt->tm_hour,
                lt->tm_min,
                lt->tm_sec);
    }
    else
    {
        fprintf(f, "0000-00-00 00:00:00");
    }
}

void log_inicializar(const char *path)
{
    g_log_file = fopen(path, "a");
    if (!g_log_file)
    {
        perror("fopen log.txt");
        // segue sem log se não conseguir abrir
    }
}

void log_finalizar(void)
{
    if (g_log_file)
    {
        fclose(g_log_file);
        g_log_file = NULL;
    }
}

void log_escrever(const char *nivel, const char *fmt, ...)
{
    pthread_mutex_lock(&g_log_mutex);

    if (!g_log_file)
    {
        g_log_file = fopen("log.txt", "a");
        if (!g_log_file)
        {
            pthread_mutex_unlock(&g_log_mutex);
            return;
        }
    }

    log_timestamp(g_log_file);
    fprintf(g_log_file, " [%s] ", nivel);

    va_list args;
    va_start(args, fmt);
    vfprintf(g_log_file, fmt, args);
    va_end(args);

    fprintf(g_log_file, "\n");
    fflush(g_log_file);

    pthread_mutex_unlock(&g_log_mutex);
}

// ---------- Configuração ----------

void config_carregar(const char *path)
{
    // Valores padrão
    g_config.tempo_verificacao_sensor_ms = 1000; // 1 segundo
    g_config.hora_ativacao = 0;
    g_config.minuto_ativacao = 0;
    g_config.hora_desativacao = 23;
    g_config.minuto_desativacao = 59;
    g_config.max_tentativas_acesso = 5;

    FILE *f = fopen(path, "r");
    if (!f)
    {
        log_escrever("ERRO", "Nao foi possivel abrir config '%s', usando valores padrao", path);
        return;
    }

    char linha[128];
    while (fgets(linha, sizeof(linha), f))
    {
        // ignora comentários
        char *p = strchr(linha, '#');
        if (p)
            *p = '\0';

        char chave[64];
        char valor[64];

        if (sscanf(linha, " %63[^=]=%63s", chave, valor) == 2)
        {
            if (strcmp(chave, "TEMPO_VERIFICACAO_SENSOR_MS") == 0)
            {
                g_config.tempo_verificacao_sensor_ms = atoi(valor);
            }
            else if (strcmp(chave, "HORA_ATIVACAO") == 0)
            {
                sscanf(valor, "%d:%d", &g_config.hora_ativacao, &g_config.minuto_ativacao);
            }
            else if (strcmp(chave, "HORA_DESATIVACAO") == 0)
            {
                sscanf(valor, "%d:%d", &g_config.hora_desativacao, &g_config.minuto_desativacao);
            }
            else if (strcmp(chave, "MAX_TENTATIVAS_ACESSO") == 0)
            {
                g_config.max_tentativas_acesso = atoi(valor);
            }
        }
    }

    fclose(f);

    log_escrever("INFO",
                 "Config carregada: tempo_verif=%d ms, ativacao=%02d:%02d, desativacao=%02d:%02d, max_tentativas=%d",
                 g_config.tempo_verificacao_sensor_ms,
                 g_config.hora_ativacao,
                 g_config.minuto_ativacao,
                 g_config.hora_desativacao,
                 g_config.minuto_desativacao,
                 g_config.max_tentativas_acesso);
}

// ---------- Funções de Esteira ----------

void esteira_inicializar(Esteira *e, const char *nome)
{
    strncpy(e->nome, nome, sizeof(e->nome));
    e->nome[sizeof(e->nome) - 1] = '\0';

    e->head = 0;
    e->tail = 0;
    e->count = 0;

    sem_init(&e->mutex, 0, 1);
    sem_init(&e->tem_item, 0, 0);
    sem_init(&e->tem_espaco, 0, CAPACIDADE_ESTEIRA);
}

void esteira_destruir(Esteira *e)
{
    sem_destroy(&e->mutex);
    sem_destroy(&e->tem_item);
    sem_destroy(&e->tem_espaco);
}

void esteira_push(Esteira *e, Bolacha b)
{
    // garante espaço
    sem_wait(&e->tem_espaco);
    // trava fila
    sem_wait(&e->mutex);

    e->fila[e->tail] = b;
    e->tail = (e->tail + 1) % CAPACIDADE_ESTEIRA;
    e->count++;

    sem_post(&e->mutex);
    sem_post(&e->tem_item);
}

Bolacha esteira_pop(Esteira *e)
{
    Bolacha b;

    // garante que há item
    sem_wait(&e->tem_item);
    // trava fila
    sem_wait(&e->mutex);

    b = e->fila[e->head];
    e->head = (e->head + 1) % CAPACIDADE_ESTEIRA;
    e->count--;

    sem_post(&e->mutex);
    sem_post(&e->tem_espaco);

    return b;
}

// ---------- Linha de Produção ----------

void linha_inicializar(LinhaProducao *lp)
{
    esteira_inicializar(&lp->e_forno, "Esteira Forno");
    esteira_inicializar(&lp->e_embalagem, "Esteira Embalagem");
    esteira_inicializar(&lp->e_caixa, "Esteira Caixa");

    lp->proximo_id = 1;
    lp->total_criadas = 0;
    lp->total_finalizadas = 0;
    lp->rodando = true;

    pthread_mutex_init(&lp->estado_mutex, NULL);

    log_escrever("INFO", "Linha de producao inicializada");
}

void linha_destruir(LinhaProducao *lp)
{
    esteira_destruir(&lp->e_forno);
    esteira_destruir(&lp->e_embalagem);
    esteira_destruir(&lp->e_caixa);

    pthread_mutex_destroy(&lp->estado_mutex);
}

void linha_set_rodando(LinhaProducao *lp, bool valor)
{
    pthread_mutex_lock(&lp->estado_mutex);
    lp->rodando = valor;
    pthread_mutex_unlock(&lp->estado_mutex);
}

bool linha_get_rodando(LinhaProducao *lp)
{
    bool v;
    pthread_mutex_lock(&lp->estado_mutex);
    v = lp->rodando;
    pthread_mutex_unlock(&lp->estado_mutex);
    return v;
}

static const char *estado_bolacha_str(EstadoBolacha e)
{
    switch (e)
    {
    case BOLACHA_CRUA:
        return "CRUA";
    case BOLACHA_ASSADA:
        return "ASSADA";
    case BOLACHA_EMBALADA:
        return "EMBALADA";
    case BOLACHA_FINALIZADA:
        return "FINALIZADA";
    default:
        return "DESCONHECIDO";
    }
}

void linha_imprimir_estado(LinhaProducao *lp)
{
    pthread_mutex_lock(&lp->estado_mutex);
    int criadas = lp->total_criadas;
    int finalizadas = lp->total_finalizadas;
    bool rodando = lp->rodando;
    pthread_mutex_unlock(&lp->estado_mutex);

    printf("=== ESTADO DA LINHA DE PRODUCAO ===\n");
    printf("Status: %s\n", rodando ? "RODANDO" : "PAUSADA");
    printf("Bolachas criadas:     %d\n", criadas);
    printf("Bolachas finalizadas: %d\n", finalizadas);
    printf("\n");

    Esteira *e1 = &lp->e_forno;
    Esteira *e2 = &lp->e_embalagem;
    Esteira *e3 = &lp->e_caixa;

    sem_wait(&e1->mutex);
    sem_wait(&e2->mutex);
    sem_wait(&e3->mutex);

    printf("%s: %d bolachas na fila\n", e1->nome, e1->count);
    printf("%s: %d bolachas na fila\n", e2->nome, e2->count);
    printf("%s: %d bolachas na fila\n", e3->nome, e3->count);

    // Exemplo: imprime só alguns IDs para não poluir
    printf("\n[Debug curto das filas]\n");
    printf("  %s -> ", e1->nome);
    for (int i = 0, idx = e1->head; i < e1->count && i < 5; i++)
    {
        printf("#%d(%s) ", e1->fila[idx].id, estado_bolacha_str(e1->fila[idx].estado));
        idx = (idx + 1) % CAPACIDADE_ESTEIRA;
    }
    printf("\n");

    printf("  %s -> ", e2->nome);
    for (int i = 0, idx = e2->head; i < e2->count && i < 5; i++)
    {
        printf("#%d(%s) ", e2->fila[idx].id, estado_bolacha_str(e2->fila[idx].estado));
        idx = (idx + 1) % CAPACIDADE_ESTEIRA;
    }
    printf("\n");

    printf("  %s -> ", e3->nome);
    for (int i = 0, idx = e3->head; i < e3->count && i < 5; i++)
    {
        printf("#%d(%s) ", e3->fila[idx].id, estado_bolacha_str(e3->fila[idx].estado));
        idx = (idx + 1) % CAPACIDADE_ESTEIRA;
    }
    printf("\n\n");

    sem_post(&e3->mutex);
    sem_post(&e2->mutex);
    sem_post(&e1->mutex);
}

// ---------- Tarefas (Threads) ----------

// Produtor: cria bolachas cruas e coloca na esteira do forno (FIFO)
void *tarefa_produtor(void *arg)
{
    LinhaProducao *lp = (LinhaProducao *)arg;

    while (1)
    {
        if (!linha_get_rodando(lp))
        {
            // Pausado: espera um pouco e tenta de novo
            usleep(200 * 1000);
            continue;
        }

        pthread_mutex_lock(&lp->estado_mutex);
        int id = lp->proximo_id++;
        lp->total_criadas++;
        pthread_mutex_unlock(&lp->estado_mutex);

        Bolacha b;
        b.id = id;
        b.estado = BOLACHA_CRUA;

        esteira_push(&lp->e_forno, b);
        log_escrever("INFO", "[PRODUTOR] Criou bolacha #%d (CRUA) e colocou na Esteira Forno", id);

        // controla a taxa de produção
        sleep(1);
    }

    return NULL;
}

// Forno: retira da esteira do forno, "assa", e coloca na esteira de embalagem
void *tarefa_forno(void *arg)
{
    LinhaProducao *lp = (LinhaProducao *)arg;

    while (1)
    {
        if (!linha_get_rodando(lp))
        {
            usleep(200 * 1000);
            continue;
        }

        Bolacha b = esteira_pop(&lp->e_forno);
        log_escrever("INFO", "[FORNO] Recebeu bolacha #%d (CRUA) -> assando...", b.id);

        // simula tempo de forno
        sleep(2);
        b.estado = BOLACHA_ASSADA;

        esteira_push(&lp->e_embalagem, b);
        log_escrever("INFO", "[FORNO] Bolacha #%d esta ASSADA e foi para Esteira Embalagem", b.id);
    }

    return NULL;
}

// Embalagem: retira da esteira de embalagem, "embala", e coloca na esteira da caixa
void *tarefa_embalagem(void *arg)
{
    LinhaProducao *lp = (LinhaProducao *)arg;

    while (1)
    {
        if (!linha_get_rodando(lp))
        {
            usleep(200 * 1000);
            continue;
        }

        Bolacha b = esteira_pop(&lp->e_embalagem);
        log_escrever("INFO", "[EMBALAGEM] Recebeu bolacha #%d (ASSADA) -> embalando...", b.id);

        // simula tempo de embalagem
        sleep(1);
        b.estado = BOLACHA_EMBALADA;

        esteira_push(&lp->e_caixa, b);
        log_escrever("INFO", "[EMBALAGEM] Bolacha #%d esta EMBALADA e foi para Esteira Caixa", b.id);
    }

    return NULL;
}

// Caixa: retira da esteira da caixa e marca como finalizada
void *tarefa_caixa(void *arg)
{
    LinhaProducao *lp = (LinhaProducao *)arg;

    while (1)
    {
        if (!linha_get_rodando(lp))
        {
            usleep(200 * 1000);
            continue;
        }

        Bolacha b = esteira_pop(&lp->e_caixa);

        b.estado = BOLACHA_FINALIZADA;

        pthread_mutex_lock(&lp->estado_mutex);
        lp->total_finalizadas++;
        pthread_mutex_unlock(&lp->estado_mutex);

        log_escrever("INFO", "[CAIXA] Bolacha #%d FINALIZADA e colocada na caixa", b.id);

        // tempo de "organizar caixa"
        sleep(1);
    }

    return NULL;
}

// Monitor: verifica horario e estado das esteiras
void *tarefa_monitor(void *arg)
{
    LinhaProducao *lp = (LinhaProducao *)arg;

    while (1)
    {
        // 1) Decide se o sistema deve estar rodando com base no horario
        time_t t = time(NULL);
        struct tm *lt = localtime(&t);

        if (lt)
        {
            int minutos_atual = lt->tm_hour * 60 + lt->tm_min;
            int minutos_on = g_config.hora_ativacao * 60 + g_config.minuto_ativacao;
            int minutos_off = g_config.hora_desativacao * 60 + g_config.minuto_desativacao;

            bool deveria_rodar = (minutos_atual >= minutos_on && minutos_atual < minutos_off);
            linha_set_rodando(lp, deveria_rodar);

            log_escrever("DEBUG",
                         "[MONITOR] Hora atual %02d:%02d, deveria_rodar=%d",
                         lt->tm_hour, lt->tm_min, deveria_rodar);
        }

        // 2) Le um resumo das esteiras e do estado global
        int c1, c2, c3;
        int criadas, finalizadas;
        bool rodando;

        pthread_mutex_lock(&lp->estado_mutex);
        criadas = lp->total_criadas;
        finalizadas = lp->total_finalizadas;
        rodando = lp->rodando;
        pthread_mutex_unlock(&lp->estado_mutex);

        sem_wait(&lp->e_forno.mutex);
        c1 = lp->e_forno.count;
        sem_post(&lp->e_forno.mutex);

        sem_wait(&lp->e_embalagem.mutex);
        c2 = lp->e_embalagem.count;
        sem_post(&lp->e_embalagem.mutex);

        sem_wait(&lp->e_caixa.mutex);
        c3 = lp->e_caixa.count;
        sem_post(&lp->e_caixa.mutex);

        log_escrever("INFO",
                     "[MONITOR] rodando=%d | forno=%d, embalagem=%d, caixa=%d | criadas=%d, finalizadas=%d",
                     rodando, c1, c2, c3, criadas, finalizadas);

        // 3) espera o tempo configurado
        usleep(g_config.tempo_verificacao_sensor_ms * 1000);
    }

    return NULL;
}
