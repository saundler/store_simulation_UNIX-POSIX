#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>
#include <csignal>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <vector>
#include <string>
#include <iostream>
#include <random>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/stat.h>

// Объявление глобальных переменных
std::string shar_memory = "/buffer";
std::string print_mutex_name = "/print";   
std::string create_mutex_name = "/create";
std::string needed_sem_name = "/needed"; 
std::string available_sem_name = "/available"; 
key_t mem_key = 767;

int shar_memory_descriptor;              // Дескриптор разделяемой памяти
int *buffer_ptr;                         // Указатель на начало разделяемой памяти
sem_t* create_mutex;                        // Мютекс добавления нового продавца
sem_t* print_mutex;                         // Мютекс вывода сообщений
std::vector<sem_t*> needed(3);              // Массив семафоров для вызова продавца
std::vector<sem_t*> available(3);           // Массив семафоров обозначающий что продавец свободен
std::vector<int> BtoS(3);                 // Массив каналов для передачи данных от покупателя к продавцу
char BtoS_base_name[] = "BtoS_0.fifo";

struct sembuf opbuf = {.sem_num = 0, .sem_flg = 0};

// Функция эмулирующая процесс продавца
void Salesman(int num) {
    while (true) {
        sem_wait(needed[num]);
        sem_wait(print_mutex);
        int buyer_pid;
        read(BtoS[num], &buyer_pid, sizeof(buyer_pid));
        std::cout << num << " the seller served " << buyer_pid << " the buyer\n";
        fflush(stdout);
        sem_post(print_mutex);
        sem_post(available[num]);
        sleep(2);
    }
}

// Освобождение всех ресурсов
void exit_func(int sig) {
    if (sig == SIGINT) {
        int pid = getpid();
        for (int i = 0; i < 3; ++i) {
            if(pid != *(buffer_ptr + i)) {
              kill(*(buffer_ptr + i), SIGINT);
            }
            close(BtoS[i]);
            BtoS_base_name[5] = i + 48;
            unlink(BtoS_base_name);
        }
        close(shar_memory_descriptor);
        shm_unlink(shar_memory.c_str());

        sem_close(create_mutex);
        sem_unlink(create_mutex_name.c_str()); 
        sem_close(print_mutex);
        sem_unlink(print_mutex_name.c_str()); 
        // Закрытие семафоров для ожидания и вызова продавцов
        for (int i = 0; i < 3; ++i) {
            std::string sem_name = needed_sem_name + std::to_string(i);
            sem_close(needed[i]);
            sem_unlink(sem_name.c_str());

            sem_name = available_sem_name + std::to_string(i);
            sem_close(available[i]);
            sem_unlink(sem_name.c_str());
        }
        exit(0);
    }
}

int main(int argn, char *argv[]) {
    signal(SIGINT, exit_func);    // Добавление обработчика сигнала
    // Инициализация разделяемой памяти
    if ((shar_memory_descriptor = shm_open(shar_memory.c_str(), O_RDWR, 0666)) < 0) {
        shar_memory_descriptor = shm_open(shar_memory.c_str(), O_CREAT | O_EXCL | O_RDWR, 0666);
        ftruncate(shar_memory_descriptor, 4 * sizeof(int));
        buffer_ptr = (int *) mmap(0, 4 * sizeof(int), PROT_WRITE | PROT_READ, MAP_SHARED, shar_memory_descriptor, 0);
        for (int i = 0; i < 4; ++i) {
            *(buffer_ptr + i) = 0;
        }
        for (int i = 0; i < 3; ++i) {
            BtoS_base_name[5] = i + 48;
            mknod(BtoS_base_name, S_IFIFO | 0666, 0);
        }
    } else {
        buffer_ptr = (int *) mmap(0, 4 * sizeof(int), PROT_WRITE | PROT_READ, MAP_SHARED, shar_memory_descriptor, 0);
    }

    if ((create_mutex = sem_open(create_mutex_name.c_str(), 0)) == SEM_FAILED) {
      create_mutex = sem_open(create_mutex_name.c_str(), O_CREAT | O_EXCL, 0666, 1);
    } 
    sem_wait(create_mutex);
    if (*(buffer_ptr + 3) == 3) {
        std::cerr << "The store has run out of places for sellers!!!\n";
        return 1;
    }
    *(buffer_ptr + *(buffer_ptr + 3)) = getpid();
    ++*(buffer_ptr + 3);
    sem_post(create_mutex);
    for (int i = 0; i < 3; ++i) {
        BtoS_base_name[5] = i + 48;
        BtoS[i] = open(BtoS_base_name, O_RDONLY | O_NONBLOCK);
        fflush(stdout);
    }
    // Инициализация семафоров
    if ((print_mutex = sem_open(print_mutex_name.c_str(), 0)) == SEM_FAILED) {
        print_mutex = sem_open(print_mutex_name.c_str(), O_CREAT | O_EXCL, 0666, 1);
    } 
    for (int i = 0; i < 3; ++i) {
        std::string sem_name = needed_sem_name + std::to_string(i);
        if ((needed[i] = sem_open(sem_name.c_str(), 0)) == SEM_FAILED) {
              needed[i] = sem_open(sem_name.c_str(), O_CREAT | O_EXCL, 0666, 0);
        }

        sem_name = available_sem_name + std::to_string(i);
        if ((available[i] = sem_open(sem_name.c_str(), 0)) == SEM_FAILED) {
              available[i] = sem_open(sem_name.c_str(), O_CREAT | O_EXCL, 0666, 1);
        }
      
    }
    Salesman(*(buffer_ptr + 3) - 1);
    return 0;
}