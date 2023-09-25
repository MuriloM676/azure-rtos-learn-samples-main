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

/* Urgent thread entry function */

void    Urgent_entry(ULONG thread_input)
{
    ULONG   start_time, cycle_time, current_time, sleep_time, status;

    /* This is the Urgent thread--it has a higher priority than the Routine thread */
    while (1)
    {
        /* Get the starting time for this cycle */
        start_time = tx_time_get();

        /* Get the semaphore and sleep--90% of the time sleep_time==5,
                                        10% of the time sleep_time==75. */
        tx_semaphore_get(&my_semaphore, TX_WAIT_FOREVER);
        if (rand() % 100 < 90) sleep_time = 5;
        else sleep_time = 75;

        status = tx_thread_sleep(sleep_time);
        if (status == TX_WAIT_ABORTED)
        {
            /* Sleep suspension is terminated
               Code to handle this situation would be here */;
        }

        /* Release the semaphore.  */
        tx_semaphore_put(&my_semaphore);

        /* Increment the thread counter and get timing info  */
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

void    Monitor_entry(ULONG thread_input)
{

    /* Parameter for the thread info get service */
    ULONG run_count;

    /* This is the Monitor thread - it has the highest priority */
    while (1)
    {
        /* The Monitor thread wakes up every 50 timer ticks and checks on the other threads */
        tx_thread_sleep(50);

        /* Determine whether the Urgent thread has stalled--if so, break its suspension */
        tx_thread_info_get(&Urgent, TX_NULL, TX_NULL,
            &run_count, TX_NULL, TX_NULL, TX_NULL, TX_NULL, TX_NULL);

        /* If the previous Urgent thread run count is the same as the current run count, abort suspension */
        if (Urgent_previous_run_count == run_count)  tx_thread_wait_abort(&Urgent);
        Urgent_previous_run_count = run_count;

        /**** Insert code here for the Routine thread ****/

    }
}

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
