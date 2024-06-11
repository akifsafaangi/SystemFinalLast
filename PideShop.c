#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include "utility.h"
#include <semaphore.h>

pthread_mutex_t cookMutex; // Mutex for synchronization between cook and manager
pthread_mutex_t mealMutex; // 
pthread_cond_t condCook; // Condition variable for controlling cook threads
pthread_cond_t condManager; // Condition variable for controlling manager thread

sem_t oven_space;
sem_t door_access;
sem_t shovels;


int customers_to_serve = 0;
int total_customers = 0;
int prepared_meals = 0;


int *prepared_by_cook; // Dynamic array to hold cook ids for each meal

/*  */
void handle_client(int client_socket);

/* Manager thread function */
void* manager(void* arg);

/* Cook thread function */
void* cook(void* arg);

/* Deliver Personel thread function */
void* deliveryPersonel(void* arg);

int main(int argc, char *argv[]) {
    if(argc != 5) {
        printf("Usage: %s <portnumber> <CookthreadPoolsize> <DeliveryPoolSize> <deliverySpeed>\n", argv[0]);
        exit(0);
    }
    int port = atoi(argv[1]);
    int cookPoolSize = atoi(argv[2]);
    int deliveryPoolSize = atoi(argv[3]);
    int deliverySpeed = atoi(argv[4]);

    prepared_by_cook = (int *)malloc(cook_count * sizeof(int));
    if (prepared_by_cook == NULL) {
        perror("Failed to allocate memory for cook ids");
        exit(EXIT_FAILURE);
    }

    pthread_t managerThread;
    pthread_t cookThread[cookPoolSize];
    pthread_t deliveryPersonelThread[deliveryPoolSize];


    int a;
    int sockfd;
    struct sockaddr_in address;

    // socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        printf("socket creation failed...\n");
        exit(0);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    // Binding newly created socket to given IP and verification
    if ((bind(sockfd, (struct sockaddr*)&address, sizeof(address))) != 0)
    {
        perror("socket bind failed...\n");
        close(sockfd);
        exit(0);
    }






    a = pthread_mutex_init(&cookMutex, 0);
    if (a != 0) {
        printf("Error creating mutex\n");
        return 1;
    }
    a = pthread_mutex_init(&mealMutex, 0);
    if (a != 0) {
        printf("Error creating mutex\n");
        return 1;
    }

    a = pthread_cond_init(&condCook, 0);
    if (a != 0) {
        printf("Error creating condition variable\n");
        return 1;
    }

    a = pthread_cond_init(&condManager, 0);
    if (a != 0) {
        printf("Error creating condition variable\n");
        return 1;
    }

    a = sem_init(&oven_space, 0, 6);
    if (a != 0) {
        printf("Error creating semaphore\n");
        return 1;
    }
    a = sem_init(&door_access, 0, 2);
    if (a != 0) {
        printf("Error creating semaphore\n");
        return 1;
    }
    a = sem_init(&shovels, 0, 3);
    if (a != 0) {
        printf("Error creating semaphore\n");
        return 1;
    }



    a = pthread_create(&managerThread, NULL, manager, (void*)(intptr_t)sockfd); // Create manager thread
    if (a != 0) {
        printf("Error creating manager thread\n");
        return 1;
    }

    int *cook_ids = malloc(cook_count * sizeof(int));
    for (int i = 0; i < cookPoolSize; i++) { // Create worker threads
        a = pthread_create(&cookThread[i], NULL, cook, &cook_ids[i]);
        if (a != 0) {
            printf("Error creating cook thread\n");
            return 1;
        }
    }

    for (int i = 0; i < deliveryPoolSize; i++) { // Create worker threads
        a = pthread_create(&deliveryPersonelThread[i], NULL, deliveryPersonel, NULL);
        if (a != 0) {
            printf("Error creating deliveryPersonel thread\n");
            return 1;
        }
    }

    // Wait for threads to finish
    a = pthread_join(managerThread, NULL);
    if (a != 0) {
        printf("Error joining manager thread\n");
        return 1;
    }
    for (int i = 0; i < cookPoolSize; i++) {
        a = pthread_join(cookThread[i], NULL);
        if (a != 0) {
            printf("Error joining worker thread\n");
            return 1;
        }
    }
    for (int i = 0; i < deliveryPoolSize; i++) {
        a = pthread_join(deliveryPersonelThread[i], NULL);
        if (a != 0) {
            printf("Error joining worker thread\n");
            return 1;
        }
    }

    pthread_mutex_destroy(&cookMutex);
    pthread_mutex_destroy(&mealMutex);
    pthread_cond_destroy(&condCook);
    pthread_cond_destroy(&condManager);
    sem_destroy(&oven_space);
    sem_destroy(&door_access);
    sem_destroy(&shovels);

    free(prepared_by_cook);
    free(cook_ids);
    return 0;
}

void *manager(void* arg) {
    int sockfd = (intptr_t)arg;
    printf("%d\n", sockfd);
    if (listen(sockfd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    printf("Server active, waiting for connection...\n");
    int new_socket;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    while ((new_socket = accept(sockfd, (struct sockaddr*)&address, &addrlen)) >= 0) {
        handle_client(new_socket);
    }
    if (new_socket < 0) {
        perror("accept");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    close(sockfd);
    pthread_exit(0);
}
void *cook(void* arg) {
    int oven_in_use = 0;
    while (1) {
        // Wait for customers
        pthread_mutex_lock(&cookMutex);
        while (customers_to_serve == 0) {
            printf("Cook waiting...\n");
            pthread_cond_wait(&condCook, &cookMutex);
        }
        customers_to_serve--; // Decrease the count of customers to serve
        pthread_mutex_unlock(&cookMutex);

        // Prepare a meal
        printf("Cook preparing a meal...\n");
        sleep(20);  // Simulating the preparation time
        printf("Cook prepared a meal.\n");

        // Place the meal in the oven
        while (1) {
            // Wait for an available shovel
            sem_wait(&shovels);

            // Try to wait for space in the oven
            if (sem_trywait(&oven_space) == 0) {
                // Successfully reserved space in the oven
                // Wait for an available door
                sem_wait(&door_access);
                printf("Cook placing meal in the oven...\n");
                sleep(2); // Simulate placing the meal in the oven

                // Release the door and the shovel
                sem_post(&door_access);
                sem_post(&shovels);
                oven_in_use = 1;
                break;
            } else {
                // Oven is full, release the shovel
                sem_post(&shovels);
                usleep(100000);  // Wait for some time before trying again
            }
        }

        // Loop to continuously prepare and cook meals
        while (1) {
            // Check if the oven is in use
            if (oven_in_use) {
                // Wait for the meal to cook
                sleep(10); // Cooking time in the oven
                // Wait for an available shovel to take out the meal
                sem_wait(&shovels);

                // Wait for an available door to take out the meal
                sem_wait(&door_access);

                // Take the meal out of the oven
                sem_post(&oven_space);
                printf("Cook taking meal out of the oven...\n");

                // Release the door and the shovel
                sem_post(&door_access);
                sem_post(&shovels);

                // Signal the manager if needed
                pthread_mutex_lock(&mealMutex);
                prepared_meals++;
                if (prepared_meals % 3 == 0 || (customers_to_serve == 0 && prepared_meals == total_customers % 3)) {
                    pthread_cond_signal(&condManager);
                }
                pthread_mutex_unlock(&mealMutex);

                oven_in_use = 0; // Meal is out of the oven
            }

            // Check if there are more customers to serve
            pthread_mutex_lock(&cookMutex);
            if (customers_to_serve == 0) {
                pthread_mutex_unlock(&cookMutex);
                break;
            }
            customers_to_serve--; // Decrease the count of customers to serve
            pthread_mutex_unlock(&cookMutex);

            // Prepare the next meal
            printf("Cook preparing another meal...\n");
            sleep(20);  // Simulating the preparation time for the next meal
            printf("Cook prepared another meal.\n");

            // Place the next meal in the oven
            while (1) {
                // Wait for an available shovel
                sem_wait(&shovels);

                // Try to wait for space in the oven
                if (sem_trywait(&oven_space) == 0) {
                    // Successfully reserved space in the oven
                    // Wait for an available door
                    sem_wait(&door_access);
                    printf("Cook placing another meal in the oven...\n");
                    sleep(2); // Simulate placing the meal in the oven

                    // Release the door and the shovel
                    sem_post(&door_access);
                    sem_post(&shovels);
                    oven_in_use = 1;
                    break;
                } else {
                    // Oven is full, release the shovel
                    sem_post(&shovels);
                    usleep(100000);  // Wait for some time before trying again
                }
            }
        }
    }

    pthread_exit(0);
}
// void *cook(void* arg) {
//     while (1) {
//         pthread_mutex_lock(&cookMutex);
//         while (customers_to_serve == 0) {
//             printf("Cook waiting...\n");
//             pthread_cond_wait(&condCook, &cookMutex);
//         }

//         if (customers_to_serve > 0) {
//             customers_to_serve--;
//             // Prepare a meal
//             printf("Cook preparing a meal...\n");
//             sleep(20);  // Simulating the preparation time
//             printf("Cook prepared a meal.\n");

//             // Place the meal in the oven
//             while (1) {
//                 // Wait for an available shovel
//                 sem_wait(&shovels);

//                 // Try to wait for space in the oven
//                 if (sem_trywait(&oven_space) == 0) {
//                     // Successfully reserved space in the oven
//                     // Wait for an available door
//                     sem_wait(&door_access);
//                     printf("Cook placing meal in the oven...\n");
//                     sleep(2); // Simulate placing the meal in the oven

//                     // Release the door and the shovel
//                     sem_post(&door_access);
//                     sem_post(&shovels);
//                     oven_in_use = 1;
//                     break;
//                 } else {
//                     // Oven is full, release the shovel
//                     sem_post(&shovels);
//                     usleep(100000);  // Wait for some time before trying again
//                 }
//             }
//             pthread_mutex_lock(&cookMutex);
            
//             pthread_mutex_unlock(&cookMutex);

//         } else {
//             pthread_mutex_unlock(&cookMutex);
//         }
//     }

    

//     pthread_exit(0);
// }


void *deliveryPersonel(void* arg) {
    pthread_exit(0);
}

void handle_client(int client_socket) {
    client_information info;
    if (read(client_socket, &info, sizeof(client_information)) <= 0) {
        perror("Failed to read from client");
        close(client_socket);
        return;
    }

    printf("Received PID: %d\n", info.pid);
    printf("Number of customers: %d\n", info.numberOfCustomers);
    printf("p: %d\n", info.p);
    printf("q: %d\n", info.q);

    pthread_mutex_lock(&cookMutex);
    customers_to_serve = info.numberOfCustomers;
    total_customers += info.numberOfCustomers;
    pthread_cond_broadcast(&condCook);
    pthread_mutex_unlock(&cookMutex);

    while (total_customers > 0) {
        pthread_mutex_lock(&mealMutex);
        while (prepared_meals < 3 && total_customers > 3) {
            pthread_cond_wait(&condManager, &mealMutex);
        }

        if (prepared_meals >= 3) {
            printf("3 meals completed.\n");
            prepared_meals -= 3;
            total_customers -= 3;
        } else if (total_customers <= 3 && prepared_meals == total_customers) {
            // Wait for the last remaining meals to be prepared
            printf("Meals finished, %d meals prepared at last.\n", prepared_meals);
            total_customers -= prepared_meals;
            prepared_meals = 0;
        }

        pthread_mutex_unlock(&mealMutex);
    }

    close(client_socket);
}