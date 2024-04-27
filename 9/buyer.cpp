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
key_t needed_base_key = 770;
key_t available_base_key = 777;

int shar_memory_descriptor;              // Дескриптор разделяемой памяти
int *buffer_ptr;                         // Указатель на начало разделяемой памяти
std::vector<int> needed(3);              // Массив семафоров для вызова продавца
std::vector<int> available(3);           // Массив семафоров обозначающий что продавец свободен
std::vector<int> BtoS(3);                 // Массив каналов для передачи данных от покупателя к продавцу
char BtoS_base_name[] = "BtoS_0.fifo";

struct sembuf opbuf = {.sem_num = 0, .sem_flg = 0};

// Функция возвращающая случайное число в заданом диапазоне
int randomInteger(int min, int max);

// Функция эмулирующая процесс покупателя
void Buyer(int num) {
    int n = randomInteger(1, 3);      // Количество отделов которые посетит покупатель
    for (int i = 0; i < n; ++i) {
        int k = randomInteger(0, 2);  // Номер отдела
        opbuf.sem_op = -1;
        semop(available[k], &opbuf, 1);
        write(BtoS[k], &num, sizeof(num));
        opbuf.sem_op = 1;
        semop(needed[k], &opbuf, 1);
        sleep(4);
    }
}

int main(int argn, char *argv[]) {
    // Инициализация разделяемой памяти
    if ((shar_memory_descriptor = shmget(mem_key, 4 * sizeof(int), 0666 | IPC_CREAT | IPC_EXCL)) < 0) {
        shar_memory_descriptor = shmget(mem_key, 4 * sizeof(int), 0);
        buffer_ptr = (int*)shmat(shar_memory_descriptor, NULL, 0);
    } else {
      std::cerr << "There are not enough sellers, the store is closed!!!\n";
      return 1;
    }

    if (*(buffer_ptr + 3) != 3) {
        std::cerr << "There are not enough sellers, the store is closed!!!\n";
        return 1;
    }

    for (int i = 0; i < 3; ++i) {
        BtoS_base_name[5] = i + 48;
        BtoS[i] = open(BtoS_base_name, O_WRONLY | O_NONBLOCK);
    }
  
    for (int i = 0; i < 3; ++i) {
        needed[i] = semget(needed_base_key + i, 1, 0);
        available[i] = semget(available_base_key + i, 1, 0);
    }
    Buyer(getpid());
    for (int i = 0; i < 3; ++i) {
        close(BtoS[i]);
    }
    exit(0);
}

int randomInteger(int min, int max) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(min, max);
    return distrib(gen);
}