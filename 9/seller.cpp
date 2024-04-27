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
std::vector<int> BtoS(3);                 // Массив каналов для передачи данных от покупателя к продавцу
char BtoS_base_name[] = "BtoS_0.fifo";

struct sembuf opbuf = {.sem_num = 0, .sem_flg = 0};

// Функция эмулирующая процесс продавца
void Salesman(int num) {
    while (true) {
        opbuf.sem_op = -1;
        semop(needed[num], &opbuf, 1);
        semop(print_mutex, &opbuf, 1);
        int buyer_pid;
        read(BtoS[num], &buyer_pid, sizeof(buyer_pid));
        std::cout << num << " the seller served " << buyer_pid << " the buyer\n";
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
            if(pid != *(buffer_ptr + i)) {
              kill(*(buffer_ptr + i), SIGINT);
            }
            close(BtoS[i]);
            BtoS_base_name[5] = i + 48;
            unlink(BtoS_base_name);
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
    if ((shar_memory_descriptor = shmget(mem_key, 4 * sizeof(int), 0666 | IPC_CREAT | IPC_EXCL)) < 0) {
        shar_memory_descriptor = shmget(mem_key, 4 * sizeof(int), 0);
        buffer_ptr = (int*)shmat(shar_memory_descriptor, NULL, 0);
    } else {
        ftruncate(shar_memory_descriptor, 4 * sizeof(int));
        buffer_ptr = (int*)shmat(shar_memory_descriptor, NULL, 0);
        for (int i = 0; i < 4; ++i) {
            *(buffer_ptr + i) = 0;
        }
        for (int i = 0; i < 3; ++i) {
            BtoS_base_name[5] = i + 48;
            mknod(BtoS_base_name, S_IFIFO | 0666, 0);
        }
    }

    if ((create_mutex = semget(create_key, 1, 0666 | IPC_CREAT | IPC_EXCL)) < 0) {
        create_mutex = semget(create_key, 1, 0);
    } else {
        semctl(create_mutex, 0, SETVAL, 1);
    }
    opbuf.sem_op = -1;
    semop(create_mutex, &opbuf, 1);
    if (*(buffer_ptr + 3) == 3) {
        std::cerr << "The store has run out of places for sellers!!!\n";
        return 1;
    }
    *(buffer_ptr + *(buffer_ptr + 3)) = getpid();
    ++*(buffer_ptr + 3);
    opbuf.sem_op = 1;
    semop(create_mutex, &opbuf, 1);
    for (int i = 0; i < 3; ++i) {
        BtoS_base_name[5] = i + 48;
        BtoS[i] = open(BtoS_base_name, O_RDONLY | O_NONBLOCK);
        fflush(stdout);
    }
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
    Salesman(*(buffer_ptr + 3) - 1);
    return 0;
}