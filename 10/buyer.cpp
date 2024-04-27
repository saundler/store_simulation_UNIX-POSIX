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
#include <sys/shm.h>

// Global Variables
std::string shar_memory = "/buffer";
std::string needed_sem_name = "/needed"; 
std::string available_sem_name = "/available"; 
int shar_memory_descriptor; 
int *buffer_ptr;

std::vector<sem_t*> needed(3);          // Semaphores for requesting a seller
std::vector<sem_t*> available(3);       // Semaphores indicating a seller is available
std::vector<int> BtoS(3);               // Array of pipes for data transfer from buyer to seller
char BtoS_base_name[] = "BtoS_0.fifo";

// Function to return a random number in a given range
int randomInteger(int min, int max);

// Function simulating the buyer process
void Buyer(int num) {
    int n = randomInteger(1, 3);       // Number of departments the buyer will visit
    for (int i = 0; i < n; ++i) {
        int k = randomInteger(0, 2);   // Department number
        sem_wait(available[k]);
        fflush(stdout);
        write(BtoS[k], &num, sizeof(num));
        sem_post(needed[k]);
        sleep(4);
    }
}

int main(int argc, char *argv[]) {
    // Shared Memory Initialization
    if ((shar_memory_descriptor = shm_open(shar_memory.c_str(), O_RDWR, 0666)) < 0) {
      std::cerr << "There are not enough sellers, the store is closed!!!\n";
      return 1;
    } else {
        buffer_ptr = (int *) mmap(0, 4 * sizeof(int), PROT_WRITE | PROT_READ, MAP_SHARED, shar_memory_descriptor, 0);
    }

    if (*(buffer_ptr + 3) != 3) {
        std::cerr << "There are not enough sellers, the store is closed!!!\n";
        return 1;
    }

  
    for (int i = 0; i < 3; ++i) {
        BtoS_base_name[5] = '0' + i;
        BtoS[i] = open(BtoS_base_name, O_WRONLY | O_NONBLOCK);
        std::string sem_name = needed_sem_name + std::to_string(i);
        needed[i] = sem_open(sem_name.c_str(), O_CREAT, 0666, 0);

        sem_name = available_sem_name + std::to_string(i);
        available[i] = sem_open(sem_name.c_str(), O_CREAT, 0666, 1);
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
