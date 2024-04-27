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


// Объявление глобальных переменных
std::string shar_memory = "/buffer";
std::string print_name = "/print";
std::string needed_base_name = "/needed";
std::string available_base_name = "/available";

int main_pid;                                // Идентификатор родительского процесса
int shar_memory_descriptor;                  // Дескриптор разделяемой памяти
int *buffer_ptr;                             // Указатель на начало разделяемой памяти
sem_t *print_mutex;                          // Мютекс вывода сообщений 
std::vector<sem_t *> needed(3);              // Массив семафоров для вызова продавца
std::vector<sem_t *> available(3);           // Массив семафоров обозначающий что продавец свободен


// Функция возвращающая случайное число в заданом диапазоне
int randomInteger(int min, int max);

// Функция эмулирующая процесс продавца
void Salesman(int num) {
    while (true) {
        sem_wait(needed[num]);    // Ожидание вызова продавца
        sem_wait(print_mutex);    // Вход в критическую секцию
        std::cout << num << " the seller served " << *(buffer_ptr + num) << " the buyer\n";
        fflush(stdout);
        sem_post(print_mutex);    // Выход из критической секции
        sem_post(available[num]); // Продавец свободен
        sleep(2);
    }
}

// Функция эмулирующая процесс покупателя
void Buyer(int num) {
    int n = randomInteger(1, 3);      // Количество отделов которые посетит покупатель
    for (int i = 0; i < n; ++i) {
        int k = randomInteger(0, 2);  // Номер отдела
        sem_wait(available[k]);       // Ожидание продавца
        *(buffer_ptr + k) = num;      // Передача номера покупателя
        sem_post(needed[k]);          // Вызов продавца
        sleep(4);
    }
}

// Освобождение всех ресурсов
void exit_func(int sig) {
    if (sig == SIGINT) {
        if (main_pid == getpid()) {
            close(shar_memory_descriptor);    // Закрытие дескриптора разделяемой памяти
            shm_unlink(shar_memory.c_str());  // Удаление разделяемой памяти
            sem_close(print_mutex);           // Закрытие мютекса для вывода сообщений
            sem_unlink(print_name.c_str());   // Удаление мютекса для вывода сообщений 

            // Закрытие семафоров для ожидания и вызова продавцов
            for (int i = 0; i < 3; ++i) {
                std::string sem_name = needed_base_name + std::to_string(i);
                sem_close(needed[i]);          // Закрытие семафора для вызова продавца
                sem_unlink(sem_name.c_str());  // Удаление семафора для вызова продавца

                sem_name = available_base_name + std::to_string(i);
                sem_close(available[i]);       // Закрытие семафора для ожидание продавца
                sem_unlink(sem_name.c_str());  // Удаление семафора для ожидание продавца
            }
        }
        exit(0);
    }
}

int main(int argn, char *argv[]) {
    int buyersCount;    // Количество покупателей

    if (argn != 2) {
        printf("Incorrect args!\n");
        return 1;
    }
    try {
        buyersCount = std::stoi(argv[1]);
    } catch (std::exception) {
        std::cerr << "Incorrect input!!!/n";
        return 1;
    }

    signal(SIGINT, exit_func);    // Добавление обработчика сигнала
    main_pid = getpid();          // Получение индекса родительского процесса

    // Инициализация разделяемой памяти
    shar_memory_descriptor = shm_open(shar_memory.c_str(), O_CREAT | O_EXCL | O_RDWR, 0666);
    ftruncate(shar_memory_descriptor, 3 * sizeof(int));
    buffer_ptr = (int *) mmap(0, 3 * sizeof(int), PROT_WRITE | PROT_READ, MAP_SHARED, shar_memory_descriptor, 0);

    // Инициализация семафоров
    print_mutex = sem_open(print_name.c_str(), O_CREAT | O_EXCL, 0666, 1);
    for (int i = 0; i < 3; ++i) {
        std::string sem_name = needed_base_name + std::to_string(i);
        needed[i] = sem_open(sem_name.c_str(), O_CREAT | O_EXCL, 0666, 0);

        sem_name = available_base_name + std::to_string(i);
        available[i] = sem_open(sem_name.c_str(), O_CREAT | O_EXCL, 0666, 1);
    }

    // Создание процессов продавцов
    int salesman_pids[3];
    for (int i = 0; i < 3; ++i) {
        int salesman_pid = fork();
        if (salesman_pid == 0) {
            Salesman(i);
        }
        salesman_pids[i] = salesman_pid;
    }

    // Создание процессов покупателей
    int buyer_pids[buyersCount];
    for (int i = 0; i < buyersCount; ++i) {
        int buyer_pid = fork();
        if (buyer_pid == 0) {
            Buyer(i);
            exit_func(SIGINT);
        }
        buyer_pids[i] = buyer_pid;
    }

    // Ожидание завершения процессов покупателей
    for (int i = 0; i < buyersCount; ++i) {
        int status;
        waitpid(buyer_pids[i], &status, 0);
    }

    // Завершение процессов продавцов
    for (int i = 0; i < 3; ++i) {
        close(salesman_pids[i]);
    }

    // Освобождение ресурсов
    exit_func(SIGINT);
    return 0;
}

int randomInteger(int min, int max) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(min, max);
    return distrib(gen);
}