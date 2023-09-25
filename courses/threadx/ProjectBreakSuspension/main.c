/* ProjectBreakSuspension - create three threads, one byte pool, one semaphore, and one timer.
   The Monitor thread uses wait abort to break Urgent or Routine thread's suspension after
   a long period of idleness. The Urgent and Routine threads each use the semaphore to proceed,
   but occasionally each thread sleeps for an extremely long time. The Monitor thread
   detects this idleness and breaks that thread's suspension.

   Your task is to detect the idleness for the Routine thread and break its suspension */

#include   "tx_api.h"
#include   <stdio.h>

#define     STACK_SIZE         1024
#define     BYTE_POOL_SIZE     9120

/* Defina os blocos de controle de objeto do ThreadX...  */
TX_THREAD      Urgent, Routine, Monitor;
TX_SEMAPHORE   my_semaphore;
TX_BYTE_POOL   my_byte_pool;
TX_TIMER       stats_timer;

/* Defina os contadores usados na aplicação PROJETO...  */
ULONG    Urgent_counter = 0, total_Urgent_time = 0;
ULONG    Routine_counter = 0, total_Routine_time = 0;

/* Defina a contagem de execuções atual para as threads Urgente e Routine. */
ULONG	 Urgent_previous_run_count = 0;
ULONG	 Routine_previous_run_count = 0;

/*Defina variáveis para informações de desempenho da thread Routine. */
ULONG resumptions_Routine;
ULONG suspensions_Routine;
ULONG wait_aborts_Routine;

/* Defina variáveis para informações de desempenho da thread Urgente*/
ULONG resumptions_Urgent;
ULONG suspensions_Urgent;
ULONG wait_aborts_Urgent;

/* Defina os protótipos de função.  */
void    Urgent_entry(ULONG thread_input);
void    Routine_entry(ULONG thread_input);
void    Monitor_entry(ULONG thread_input);
void    print_stats(ULONG);

/* Defina o ponto de entrada principa.  */

int main()
{
    /* Inicie o kernel do ThreadX.  */
    tx_kernel_enter();
}

/* Defina como o sistema inicial se parece.  */
/* Coloque informações de definição do sistema aqui */

void    tx_application_define(void* first_unused_memory)
{
    CHAR* Urgent_stack_ptr, * Routine_stack_ptr, * Monitor_stack_ptr;

    /* Crie um pool de memória de bytes a partir do qual alocar as pilhas das threads.  */
    tx_byte_pool_create(&my_byte_pool, "my_byte_pool",
                        first_unused_memory, BYTE_POOL_SIZE);

    /* Aloque a pilha para a thread Urgente.  */
    tx_byte_allocate(&my_byte_pool, (VOID**)&Urgent_stack_ptr, STACK_SIZE, TX_NO_WAIT);

    /* Crie a thread urgente  */
    tx_thread_create(&Urgent, "Urgent", Urgent_entry, 0x1234,
                     Urgent_stack_ptr, STACK_SIZE, 5, 5, TX_NO_TIME_SLICE, TX_AUTO_START);

    /* Aloque a pilha para a thread Routine.  */
    tx_byte_allocate(&my_byte_pool, (VOID**)&Routine_stack_ptr, STACK_SIZE, TX_NO_WAIT);

    /* Crie a rotina de thread */
    tx_thread_create(&Routine, "Routine", Routine_entry, 0x1234,
                     Routine_stack_ptr, STACK_SIZE, 15, 15,
                     TX_NO_TIME_SLICE, TX_AUTO_START);

    /* Aloque a pilha para a thread de Monitoramento.  */
    tx_byte_allocate(&my_byte_pool, (VOID**)&Monitor_stack_ptr, STACK_SIZE, TX_NO_WAIT);

    /*Crie a thread de Monitoramento*/
    tx_thread_create(&Monitor, "Monitor", Monitor_entry, 0x1234,
                     Monitor_stack_ptr, STACK_SIZE, 3, 3,
                     TX_NO_TIME_SLICE, TX_AUTO_START);

    /* Crie o semáforo usado por ambas as threads.  */
    tx_semaphore_create(&my_semaphore, "my_semaphore", 2);

    /* Crie e ative o timer */
    tx_timer_create(&stats_timer, "stats_timer", print_stats,
                    0x1234, 1000, 1000, TX_AUTO_ACTIVATE);
}

/************************************************************/

/* Função de entrada para a thread Urgent. */

void    Urgent_entry(ULONG thread_input)
{
    ULONG   start_time, cycle_time, current_time, sleep_time, status;

    /* Esta é a thread Urgent - ela tem uma prioridade maior que a thread Routine. */
    while (1)
    {
        /* Obter o tempo de início para este ciclo. */
        start_time = tx_time_get();

        /* Obter o semáforo e dormir - 90% do tempo sleep_time==5, 10% do tempo sleep_time==75. */
        tx_semaphore_get(&my_semaphore, TX_WAIT_FOREVER);
        if (rand() % 100 < 90) sleep_time = 5;
        else sleep_time = 75;

        status = tx_thread_sleep(sleep_time);
        if (status == TX_WAIT_ABORTED)
        {
            /* Suspensão de sono terminada. */

            /* O código para lidar com esta situação seria colocado aqui. */
        }

        /* Liberar o semáforo. */
        tx_semaphore_put(&my_semaphore);

        /* Incrementar o contador de threads e obter as informações de tempo. */
        Urgent_counter++;
        current_time = tx_time_get();
        cycle_time = current_time - start_time;
        total_Urgent_time += cycle_time;
    }
}

/************************************************************/

/* Routine thread entry function */

void    Routine_entry(ULONG thread_input)
{
    ULONG	start_time, current_time, cycle_time, sleep_time;

    /* This is the Routine thread--it has a lower priority than the Urgent thread */
    while (1)
    {
        /* Get the starting time for this cycle */
        start_time = tx_time_get();

        /* Get the semaphore and sleep--90% of the time sleep_time==25,
                                        10% of the time sleep_time==400 */
        tx_semaphore_get(&my_semaphore, TX_WAIT_FOREVER);
        if (rand() % 100 < 90) sleep_time = 25;
        else sleep_time = 400;

        tx_thread_sleep(sleep_time);
        /* Insert if statement to determine whether sleep was wait aborted */

        /* Release the semaphore.  */
        tx_semaphore_put(&my_semaphore);

        /* Increment the thread counter and get timing info  */
        Routine_counter++;

        current_time = tx_time_get();
        cycle_time = current_time - start_time;
        total_Routine_time += cycle_time;
    }
}

/************************************************************/

/************************************************************/
/* implementando a parte que detecta a ociosidade da thread Routine e interrompe sua suspensão,
  semelhante ao que é feito com a thread Urgent na função Monitor_entry */
void    Monitor_entry(ULONG thread_input)
{
    /* Parâmetro para o serviço de obtenção de informações da thread */
    ULONG run_count;

    /* Esta é a thread Monitor - ela tem a mais alta prioridade. */
    while (1)
    {
        /* A thread Monitor acorda a cada 50 marcas de tempo do temporizador e verifica as outras threads */
        tx_thread_sleep(50);

        /* Determinar se a thread Urgent está travada - se estiver, interrompa sua suspensão */
        tx_thread_info_get(&Urgent, TX_NULL, TX_NULL,
                           &run_count, TX_NULL, TX_NULL, TX_NULL, TX_NULL, TX_NULL);

        /* Se a contagem de execução anterior da thread Urgent for igual à contagem de execução atual, cancele a suspensão */
        if (Urgent_previous_run_count == run_count)  tx_thread_wait_abort(&Urgent);
        Urgent_previous_run_count = run_count;

        /* Determinar se a thread Routine está travada - se estiver, interrompa sua suspensão */
        tx_thread_info_get(&Routine, TX_NULL, TX_NULL,
                           &run_count, TX_NULL, TX_NULL, TX_NULL, TX_NULL, TX_NULL);

        /* Se a contagem de execução anterior da thread Routine for igual à contagem de execução atual, cancele a suspensão */
        if (Routine_previous_run_count == run_count)  tx_thread_wait_abort(&Routine);
        Routine_previous_run_count = run_count;
    }
}
/*Agora, a função Monitor_entry verificará a ociosidade da thread Routine e interromperá sua suspensão da mesma forma que faz com a thread Urgent.*/

/***************************************************/
/* print statistics at specified times */
void print_stats(ULONG invalue)
{
    // Declara três variáveis globais para armazenar informações de desempenho.
    ULONG   current_time, avg_Routine_time, avg_Urgent_time;

    // Obter informações de desempenho no thread de rotina
    // Recupera informações de desempenho da thread Routine
    tx_thread_performance_info_get(&Routine, &resumptions_Routine, &suspensions_Routine,
        TX_NULL, TX_NULL, TX_NULL, TX_NULL, TX_NULL, TX_NULL,
        &wait_aborts_Routine, TX_NULL);

    // Obter informações de desempenho no thread urgente
    tx_thread_performance_info_get(&Urgent, &resumptions_Urgent, &suspensions_Urgent,
        TX_NULL, TX_NULL, TX_NULL, TX_NULL, TX_NULL, TX_NULL,
        &wait_aborts_Urgent, TX_NULL);

    // Obtém a hora atual do sistema
    current_time = tx_time_get();

    // Verifica se as duas threads já foram retomadas pelo menos uma vez.
    if ((Urgent_counter > 0) && (Routine_counter > 0))
    {
        avg_Routine_time = total_Routine_time / Routine_counter; // Calcula o tempo médio que a thread Routine passou em execução
        avg_Urgent_time = total_Urgent_time / Urgent_counter; // Calcula o tempo médio que a thread Urgent passou em execução

        printf("\nProjectBreakSuspension: 3 threads, 1 byte pool, 1 semaphore, and 1 timer.\n\n"); // Imprime uma mensagem de cabeçalho
        printf("     Current Time:                %lu\n", current_time); // Imprime a hora atual do sistema
        printf("            Urgent counter:       %lu\n", Urgent_counter); // Imprime informações sobre a thread Urgente
        printf("           Urgent avg time:       %lu\n", avg_Urgent_time); // Imprime informações sobre a thread Urgente
        printf("           Routine counter:       %lu\n", Routine_counter); // Imprime informações sobre a thread Routine
        printf("          Routine avg time:       %lu\n\n", avg_Routine_time); // Imprime informações sobre a thread Routine

        // Imprime informações sobre as operações de retomada e suspensão da thread Urgente
        printf(" Urgent Thread resumptions:       %lu\n", resumptions_Urgent);
        printf("               suspensions:       %lu\n", suspensions_Urgent);
        printf("               wait aborts:       %lu\n\n", wait_aborts_Urgent);

        // Imprimem informações sobre as operações de aborto da thread Urgent
        printf("Routine Thread resumptions:       %lu\n", resumptions_Routine);
        printf("               suspensions:       %lu\n", suspensions_Routine);
        printf("               wait aborts:       %lu\n\n", wait_aborts_Routine);

    }
    else printf("Bypassing print_stats function, Current Time: %d\n", tx_time_get());
}
