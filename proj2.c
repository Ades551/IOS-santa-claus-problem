// proj2.c
// Author: Adam Rajko
// xlogin: xrajko00
// Description: Santa-Claus problem solution using semaphores and shared memory

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdarg.h>

typedef struct {
    sem_t elf_sem;
    sem_t reindeer_sem;
    sem_t santa_sem;
    sem_t santa_helped;
    sem_t santa_sleep;
    sem_t reindeers_hitched;

    sem_t mutex;

    int elves;
    int reindeer;
    int process_count;

    bool end;
} shared_mem;

shared_mem *shm; // shared memory

#define MAX_ELVES_HELP 3
#define OUTPUT_FILE "proj2.out"
#define SHM_KEY 12345
#define SHM_KEY_SIZE (sizeof(shared_mem))

void initialize();
void destroy();

void santa_process(int *elf_count, int *reindeer_count);
void elf_process(int *elf_count, int *elf_max_sleep);
void reindeer_process(int *reindeer_count, int *reindeer_max_sleep);

void error_msg(const char *format, ...);
void print_msg(const char *output, const char *format, ...);
void print_msg_mutex(const char *output, const char *format, ...);

void getargs(int argc, const char* argv[argc], int *elves, int *reindeer, int *elf_sleep, int *reindeer_sleep);
void clean_file(const char *file_name);

int main(int argc, char const *argv[]){

    int elves, reindeers, elf_sleep, reindeer_sleep; // arguments

    getargs(argc, argv, &elves, &reindeers, &elf_sleep, &reindeer_sleep);

    key_t shm_key = SHM_KEY;
    int shm_id = shmget(shm_key, SHM_KEY_SIZE, IPC_CREAT | 0666);
    shm = (shared_mem *) shmat(shm_id, NULL, 0);

    clean_file(OUTPUT_FILE);

    initialize();

    santa_process(&elves, &reindeers);
    reindeer_process(&reindeers, &reindeer_sleep);
    elf_process(&elves, &elf_sleep);
    
    while(wait(NULL) > 0);
    
    destroy();

    shmctl(shm_id, IPC_RMID, NULL);

    return 0;
}

/**
 * @brief santa process
 * @param elf_count pointer to elf count
 * @param reindeer_count pointer to reindeer count
 */
void santa_process(int *elf_count, int *reindeer_count){
    pid_t pid = fork();
    if(pid < 0){
        error_msg("Santa process fork\n");
    }
    else if(pid == 0){
        // while reindeers are not hitched
        while(!shm->end){
            sem_wait(&shm->santa_sleep); // wait for the last elf to leave workshop
            print_msg_mutex(OUTPUT_FILE, "Santa: going to sleep\n");
            sem_wait(&shm->santa_sem); // wait for santa signal
            sem_wait(&shm->mutex); // acces to shm (only one process at the time)

            // if all reindeers are home
            if(shm->reindeer == *reindeer_count){
                shm->end = true; // to tell santa process to end and elves that workshop is closed
                print_msg(OUTPUT_FILE, "Santa: closing workshop\n");
                for(int i = 0; i < *elf_count; i++) sem_post(&shm->elf_sem); // every elf can go in line for workshop
                for(int i = 0; i < MAX_ELVES_HELP; i++) sem_post(&shm->santa_helped); // let elves waiting for workshop know that is closed
                for(int i = 0; i < *reindeer_count; i++) sem_post(&shm->reindeer_sem); // start hitching reindeers
            }
            // if elves are ready
            else if(shm->elves == MAX_ELVES_HELP){
                print_msg(OUTPUT_FILE, "Santa: helping elves\n");
                for(int i = 0; i < MAX_ELVES_HELP; i++) sem_post(&shm->santa_helped); // santa helped elves
            }
            sem_post(&shm->mutex);
        }
        sem_wait(&shm->reindeers_hitched); // wait for reindeers to be hitched
        print_msg_mutex(OUTPUT_FILE, "Santa: Christmas started\n");
        exit(0);
    }   
}

/**
 * @brief reindeer process
 * @param reindeer_count pointer to reindeer count
 * @param reindeer_max_sleep pointer to reindeer max holiday time
 */ 
void reindeer_process(int *reindeer_count, int *reindeer_max_sleep){
    for(int i = 1; i <= *reindeer_count; i++){
        pid_t pid = fork();
        if(pid < 0){
            error_msg("Reindeer fork failed\n");
        }
        else if(pid == 0){

            int reindeerID = i;

            print_msg_mutex(OUTPUT_FILE, "RD %d: rstarted\n", reindeerID);

            srand(getpid()); // get different seed than other process
            usleep(*reindeer_max_sleep + (rand() % (*reindeer_max_sleep + 1))); // sleep time in ms
            
            print_msg_mutex(OUTPUT_FILE, "RD %d: return home\n", reindeerID);

            sem_wait(&shm->mutex);

            shm->reindeer++;

            // if last reindeer came home
            if(shm->reindeer == *reindeer_count) sem_post(&shm->santa_sem); // wake up santa

            sem_post(&shm->mutex);

            sem_wait(&shm->reindeer_sem); // wait for santa to hitch reindeer
            print_msg_mutex(OUTPUT_FILE, "RD %d: get hitched\n", reindeerID);
            sem_wait(&shm->mutex);
            shm->reindeer--;
            // if last reindeer is hitched
            if(shm->reindeer == 0) sem_post(&shm->reindeers_hitched); // tell santa that x-mas started
            sem_post(&shm->mutex);
            exit(0);
        }
    }
}

/**
 * @brief elf process
 * @param elf_count pointer to elf count
 * @param elf_max_sleep pointer to elf max work time
 */
void elf_process(int *elf_count, int *elf_max_sleep){
    for(int i = 1; i <= *elf_count; i++){
        pid_t pid = fork();
        if(pid < 0){
            error_msg("Elf fork failed\n");
        }
        else if(pid == 0){

            int elfID = i;

            print_msg_mutex(OUTPUT_FILE, "Elf %d: started\n", elfID);

            srand(getpid()); // get different seed than other process

            while (true)
            {
                usleep(rand() % (*elf_max_sleep + 1)); // work time in ms

                print_msg_mutex(OUTPUT_FILE, "Elf %d: need help\n", elfID);
                
                sem_wait(&shm->elf_sem); // wait for signal from santa or from elf

                if(shm->end) break; // workshop closed

                sem_wait(&shm->mutex);

                shm->elves++;
                // if elves are ready
                if(shm->elves == MAX_ELVES_HELP){
                    sem_post(&shm->santa_sem); // wake up santa
                } else {
                    sem_post(&shm->elf_sem); // telf other elf to get in the line
                }

                sem_post(&shm->mutex);
                sem_wait(&shm->santa_helped); // wait for santa to help elf

                if(shm->end) break; //workshop closed

                print_msg_mutex(OUTPUT_FILE, "Elf %d: get help\n", elfID);
                
                sem_wait(&shm->mutex);
                shm->elves--;
                // if santa helped elves
                if(shm->elves == 0){
                    sem_post(&shm->santa_sleep); // tell santa that he can go to sleep
                    sem_post(&shm->elf_sem); // telf other elf to get in the line
                }
                sem_post(&shm->mutex);
            }
            print_msg_mutex(OUTPUT_FILE, "Elf %d: taking holidays\n", elfID);
            exit(0);
        }
    }
}

/**
 * @brief checks agruments and assign them
 * @param arc total amount of arguments
 * @param argv array of arguments
 * @param elves pointer to elf count
 * @param reindeers pointer to reindeer count
 * @param elf_sleep pointer to elf max work time
 * @param reindeer_sleep pointer to reindeer max holiday time
 */
void getargs(int argc, const char* argv[argc], int *elves, int *reindeers, int *elf_sleep, int *reindeer_sleep){
    if(argc < 5 || argc > 5) error_msg("Invalid arg count: %d\n", argc);

    // elf count - 1
    // reindeer count - 2
    // elf work time - 3
    // reindeer holiday time - 4
    for(int i = 1; i < argc; i++){
        switch (i){
        case 1:
            *elves = atoi(argv[1]);
            if(*elves <= 0 || *elves >= 1000) error_msg("Elf count out of range: %d\n", *elves);
            break;
        case 2:
            *reindeers = atoi(argv[2]);
            if(*reindeers <= 0 || *reindeers >= 20) error_msg("Reindeer count out of range: %d\n", *reindeers);
            break;
        case 3:
            *elf_sleep = atoi(argv[3]);
            if(*elf_sleep < 0 || *elf_sleep > 1000) error_msg("Elf work time out of range: %d\n", *elf_sleep);
            break;
        case 4:
            *reindeer_sleep = atoi(argv[4]);
            if(*reindeer_sleep < 0 || *reindeer_sleep > 1000) error_msg("Reindeer holiday time out of range: %d\n", *reindeer_sleep);
            break;
        default:
            error_msg("Invalid arg count: %d\n", argc);
            break;
        }
    }
}

/**
 * @brief prints error message and exits program
 * @param format message format
 * @param ... format arguments
 */
void error_msg(const char *format, ...){
    va_list ap;
    va_start(ap, format);

    // print error message
    fprintf(stderr, "error: ");
    vfprintf(stderr, format, ap);
    
    va_end(ap);

    exit(1);
}

/**
 * @brief appends message to OUTPUT_FILE and serves as process counter
 * @param output output file
 * @param format message format
 * @param ... format arguments
 */
void print_msg(const char *output, const char *format, ...){
    va_list ap;
    va_start(ap, format);

    FILE *file;
    if( (file = fopen(output, "a")) == NULL) error_msg("Failed to open file %s\n", output);

    shm->process_count++; // process counter
    fprintf(file, "%d: ", shm->process_count);
    vfprintf(file, format, ap);
    fclose(file);

    va_end(ap);
}

/**
 * @brief appends message to OUTPUT_FILE but waits for mutex and serves as process counter
 * @param output output file
 * @param format message format
 * @param ... format arguments
 */
void print_msg_mutex(const char *output, const char *format, ...){
    va_list ap;
    va_start(ap, format);

    FILE *file;
    if( (file = fopen(output, "a")) == NULL) error_msg("Failed to open file %s\n", output);

    sem_wait(&shm->mutex); // wait for shared mem to be available
    
    shm->process_count++; // process counter
    fprintf(file, "%d: ", shm->process_count);
    vfprintf(file, format, ap);
    fclose(file);
    
    sem_post(&shm->mutex);

    va_end(ap);
}

/**
 * @brief erasing data from file
 * @param file_name file name
 */
void clean_file(const char *file_name){
    FILE *file;
    if ((file = fopen(file_name, "w")) == NULL) error_msg("Failed to open file %s\n", file_name);
    fclose(file);
}

// initialize shared memory structure
void initialize(){
    sem_init(&shm->elf_sem, 1, 1);
    sem_init(&shm->reindeer_sem, 1, 0);
    sem_init(&shm->santa_sem, 1, 0);
    sem_init(&shm->santa_helped, 1, 0);
    sem_init(&shm->santa_sleep, 1, 1);
    sem_init(&shm->reindeers_hitched, 1, 0);
    sem_init(&shm->mutex, 1, 1);

    shm->elves = 0;
    shm->reindeer = 0;
    shm->process_count = 0;

    shm->end = false;
}

// destroy semaphores
void destroy(){
    sem_destroy(&shm->elf_sem);
    sem_destroy(&shm->reindeer_sem);
    sem_destroy(&shm->santa_sem);
    sem_destroy(&shm->santa_helped);
    sem_destroy(&shm->santa_sleep);
    sem_destroy(&shm->reindeers_hitched);
    sem_destroy(&shm->mutex);
}
