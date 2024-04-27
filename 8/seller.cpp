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

// Объявление глобальных переменных
std::string shar_memory = "/buffer";
key_t mem_key = 767;
key_t print_key = 768;
key_t create_key = 769;
key_t needed_base_key = 770;
key_t available_base_key = 777;

int shar_memory_descriptor;              // Дескриптор разделяемой памяти
int *buffer_ptr;                         // Указатель на начало разделяемой памяти
int create_mutex;                        // Мютекс добавления нового продавца
int print_mutex;                         // Мютекс вывода сообщений
std::vector<int> needed(3);              // Массив семафоров для вызова продавца
std::vector<int> available(3);           // Массив семафоров обозначающий что продавец свободен

struct sembuf opbuf = {.sem_num = 0, .sem_flg = 0};

// Функция эмулирующая процесс продавца
void Salesman(int num) {
    while (true) {
        opbuf.sem_op = -1;
        semop(needed[num], &opbuf, 1);
        semop(print_mutex, &opbuf, 1);
        std::cout << num << " the seller served " << *(buffer_ptr + num) << " the buyer\n";
        fflush(stdout);
        opbuf.sem_op = 1;
        semop(print_mutex, &opbuf, 1);
        semop(available[num], &opbuf, 1);
        sleep(2);
    }
}

// Освобождение всех ресурсов
void exit_func(int sig) {
    if (sig == SIGINT) {
        int pid = getpid();
        for (int i = 0; i < 3; ++i) {
            if(pid != *(buffer_ptr + 3 + i)) {
              kill(*(buffer_ptr + 3 + i), SIGINT);
            }
        }
        shmctl(shar_memory_descriptor, IPC_RMID, 0);
        shmdt(buffer_ptr);

        semctl(create_mutex, 0, IPC_RMID, 0);
        semctl(print_mutex, 0, IPC_RMID, 0);
        // Закрытие семафоров для ожидания и вызова продавцов
        for (int i = 0; i < 3; ++i) {
            semctl(needed[i], 0, IPC_RMID, 0);
            semctl(available[i], 0, IPC_RMID, 0);
        }
        exit(0);
    }
}

int main(int argn, char *argv[]) {
    signal(SIGINT, exit_func);    // Добавление обработчика сигнала

    // Инициализация разделяемой памяти
    if ((shar_memory_descriptor = shmget(mem_key, 7 * sizeof(int), 0666 | IPC_CREAT | IPC_EXCL)) < 0) {
        shar_memory_descriptor = shmget(mem_key, 7 * sizeof(int), 0);
        buffer_ptr = (int*)shmat(shar_memory_descriptor, NULL, 0);
    } else {
        ftruncate(shar_memory_descriptor, 7 * sizeof(int));
        buffer_ptr = (int*)shmat(shar_memory_descriptor, NULL, 0);
        for (int i = 0; i < 7; ++i) {
            *(buffer_ptr + i) = 0;
        }
    }

    if ((create_mutex = semget(create_key, 1, 0666 | IPC_CREAT | IPC_EXCL)) < 0) {
        create_mutex = semget(create_key, 1, 0);
    } else {
        semctl(create_mutex, 0, SETVAL, 1);
    }
    opbuf.sem_op = -1;
    semop(create_mutex, &opbuf, 1);
    if (*(buffer_ptr + 6) == 3) {
        std::cerr << "The store has run out of places for sellers!!!\n";
        return 1;
    }
    *(buffer_ptr + 3 + *(buffer_ptr + 6)) = getpid();
    ++*(buffer_ptr + 6);
    opbuf.sem_op = 1;
    semop(create_mutex, &opbuf, 1);

    // Инициализация семафоров
    if ((print_mutex = semget(print_key, 1, 0666 | IPC_CREAT | IPC_EXCL)) < 0) {
        print_mutex = semget(print_key, 1, 0);
    } else {
        semctl(print_mutex, 0, SETVAL, 1);
    }
    for (int i = 0; i < 3; ++i) {
        if ((needed[i] = semget(needed_base_key + i, 1, 0666 | IPC_CREAT | IPC_EXCL)) < 0) {
            needed[i] = semget(needed_base_key + i, 1, 0);
        } else {
            semctl(needed[i], 0, SETVAL, 0);
        }

        if ((available[i] = semget(available_base_key + i, 1, 0666 | IPC_CREAT | IPC_EXCL)) < 0) {
            available[i] = semget(available_base_key + i, 1, 0);
        } else {
            semctl(available[i], 0, SETVAL, 1);
        }
    }
    Salesman(*(buffer_ptr + 6) - 1);
    return 0;
}