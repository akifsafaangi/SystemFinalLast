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
#include <time.h>
#include <errno.h>
#include <fcntl.h>

#define LOG_FILE "shop_log.txt"

pthread_mutex_t deliveryMutex;
pthread_cond_t deliveryCond;

pthread_mutex_t cookMutex; // Mutex for synchronization between cook and manager
pthread_mutex_t mealMutex; // 
pthread_cond_t condCook; // Condition variable for controlling cook threads
pthread_cond_t condManager; // Condition variable for controlling manager thread

sem_t oven_space;
sem_t door_access;
sem_t shovels;

int cookPoolSize; // Number of cook threads
int customers_to_serve = 0;
int total_customers = 0;
int prepared_meals = 0;

typedef struct {
    int cookId;
    int orderId;
} cookOrderInfo;
cookOrderInfo *prepared_by_cook; // Dynamic array to hold cook ids for each meal


int deliverySpeed; // Speed of the delivery personel
typedef struct {
    int pid;
    int cookId;
    int p;
    int q;
    int totalTime;
    int order_id;
} order;
order *orders;
int head = 0;
int last = 0;
int deliveryPackage = 0;



int logFile;
pthread_mutex_t logMutex; // Mutex for log file
/* Default format for time, date, message */
static const char default_format[] = "%b %d %Y %Z %H %M %S";

int orderIdIndex = 0;
/*  */
void handle_client(int client_socket);

/* Manager thread function */
void* manager(void* arg);

/* Cook thread function */
void* cook(void* arg);

/* Deliver Personel thread function */
void* deliveryPersonel(void* arg);
/**/
void generateRandomPQ(int *p, int *q, int maxP, int maxQ);

/**/
void writeLogMessage(char* startMessage);
void createMessageWithTime(char* startMessage, char* message);
/**/
void clear();
/**/
int min(int a, int b);

int main(int argc, char *argv[]) {
    if(argc != 5) {
        printf("Usage: %s <portnumber> <CookthreadPoolsize> <DeliveryPoolSize> <deliverySpeed>\n", argv[0]);
        exit(0);
    }
    int port = atoi(argv[1]);
    cookPoolSize = atoi(argv[2]);
    int deliveryPoolSize = atoi(argv[3]);
    deliverySpeed = atoi(argv[4]);

    prepared_by_cook = (cookOrderInfo *)malloc(cookPoolSize * sizeof(cookOrderInfo));
    if (prepared_by_cook == NULL) {
        perror("Failed to allocate memory for cook ids");
        exit(EXIT_FAILURE);
    }
    orders = (order *)malloc(cookPoolSize * sizeof(order));
    if (orders == NULL) {
        perror("Failed to allocate memory for orders");
        free(prepared_by_cook);
        exit(EXIT_FAILURE);
    }

    /* Create log file */
    logFile = open(LOG_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if(logFile == -1) {
        perror("The file cannot be opened\n");
        free(prepared_by_cook);
        free(orders);
        exit(-1);
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
        free(prepared_by_cook);
        free(orders);
        close(logFile);
        exit(0);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    // Binding newly created socket to given IP and verification
    if ((bind(sockfd, (struct sockaddr*)&address, sizeof(address))) != 0)
    {
        perror("socket bind failed...\n");
        free(prepared_by_cook);
        free(orders);
        close(logFile);
        close(sockfd);
        exit(0);
    }






    a = pthread_mutex_init(&cookMutex, 0);
    if (a != 0) {
        printf("Error creating mutex\n");
        free(prepared_by_cook);
        free(orders);
        close(logFile);
        close(sockfd);
        return 1;
    }
    a = pthread_mutex_init(&mealMutex, 0);
    if (a != 0) {
        printf("Error creating mutex\n");
        pthread_mutex_destroy(&cookMutex);
        free(prepared_by_cook);
        free(orders);
        close(logFile);
        close(sockfd);
        return 1;
    }

    a = pthread_cond_init(&condCook, 0);
    if (a != 0) {
        printf("Error creating condition variable\n");
        pthread_mutex_destroy(&cookMutex);
        pthread_mutex_destroy(&mealMutex);
        free(prepared_by_cook);
        free(orders);
        close(logFile);
        close(sockfd);
        return 1;
    }

    a = pthread_cond_init(&condManager, 0);
    if (a != 0) {
        printf("Error creating condition variable\n");
        pthread_mutex_destroy(&cookMutex);
        pthread_mutex_destroy(&mealMutex);
        pthread_cond_destroy(&condCook);
        free(prepared_by_cook);
        free(orders);
        close(logFile);
        close(sockfd);
        return 1;
    }

    a = sem_init(&oven_space, 0, 6);
    if (a != 0) {
        printf("Error creating semaphore\n");
        pthread_mutex_destroy(&cookMutex);
        pthread_mutex_destroy(&mealMutex);
        pthread_cond_destroy(&condCook);
        pthread_cond_destroy(&condManager);
        free(prepared_by_cook);
        free(orders);
        close(logFile);
        close(sockfd);
        return 1;
    }
    a = sem_init(&door_access, 0, 2);
    if (a != 0) {
        printf("Error creating semaphore\n");
        pthread_mutex_destroy(&cookMutex);
        pthread_mutex_destroy(&mealMutex);
        pthread_cond_destroy(&condCook);
        pthread_cond_destroy(&condManager);
        sem_destroy(&oven_space);
        free(prepared_by_cook);
        free(orders);
        close(logFile);
        close(sockfd);
        return 1;
    }
    a = sem_init(&shovels, 0, 3);
    if (a != 0) {
        printf("Error creating semaphore\n");
        pthread_mutex_destroy(&cookMutex);
        pthread_mutex_destroy(&mealMutex);
        pthread_cond_destroy(&condCook);
        pthread_cond_destroy(&condManager);
        sem_destroy(&oven_space);
        sem_destroy(&door_access);
        free(prepared_by_cook);
        free(orders);
        close(logFile);
        close(sockfd);
        return 1;
    }

    a = pthread_mutex_init(&deliveryMutex, 0);
    if (a != 0) {
        printf("Error creating mutex\n");
        pthread_mutex_destroy(&cookMutex);
        pthread_mutex_destroy(&mealMutex);
        pthread_cond_destroy(&condCook);
        pthread_cond_destroy(&condManager);
        sem_destroy(&oven_space);
        sem_destroy(&door_access);
        sem_destroy(&shovels);
        free(prepared_by_cook);
        free(orders);
        close(logFile);
        close(sockfd);
        return 1;
    }
    a = pthread_cond_init(&deliveryCond, 0);
    if (a != 0) {
        printf("Error creating condition variable\n");
        pthread_mutex_destroy(&deliveryMutex);
        pthread_mutex_destroy(&cookMutex);
        pthread_mutex_destroy(&mealMutex);
        pthread_cond_destroy(&condCook);
        pthread_cond_destroy(&condManager);
        sem_destroy(&oven_space);
        sem_destroy(&door_access);
        sem_destroy(&shovels);
        free(prepared_by_cook);
        free(orders);
        close(logFile);
        close(sockfd);
        return 1;
    }
    a = pthread_mutex_init(&logMutex, 0);
    if (a != 0) {
        printf("Error creating mutex\n");
        pthread_mutex_destroy(&deliveryMutex);
        pthread_cond_destroy(&deliveryCond);
        pthread_mutex_destroy(&cookMutex);
        pthread_mutex_destroy(&mealMutex);
        pthread_cond_destroy(&condCook);
        pthread_cond_destroy(&condManager);
        sem_destroy(&oven_space);
        sem_destroy(&door_access);
        sem_destroy(&shovels);
        free(prepared_by_cook);
        free(orders);
        close(logFile);
        close(sockfd);
        return 1;
    }
    a = pthread_create(&managerThread, NULL, manager, (void*)(intptr_t)sockfd); // Create manager thread
    if (a != 0) {
        printf("Error creating manager thread\n");
        clear();
        free(prepared_by_cook);
        free(orders);
        close(logFile);
        close(sockfd);
        return 1;
    }

    int *cook_ids = malloc(cookPoolSize * sizeof(int));
    for (int i = 0; i < cookPoolSize; i++) {
        cook_ids[i] = i + 1;
    }
    for (int i = 0; i < cookPoolSize; i++) { // Create worker threads
        a = pthread_create(&cookThread[i], NULL, cook, &cook_ids[i]);
        if (a != 0) {
            printf("Error creating cook thread\n");
            clear();
            free(prepared_by_cook);
            free(orders);
            close(logFile);
            close(sockfd);
            return 1;
        }
    }

    int *delivery_ids = malloc(deliveryPoolSize * sizeof(int));
    for (int i = 0; i < deliveryPoolSize; i++) {
        delivery_ids[i] = i + 1;
    }
    for (int i = 0; i < deliveryPoolSize; i++) { // Create worker threads
        a = pthread_create(&deliveryPersonelThread[i], NULL, deliveryPersonel, &delivery_ids[i]);
        if (a != 0) {
            printf("Error creating deliveryPersonel thread\n");
            clear();
            free(prepared_by_cook);
            free(orders);
            free(cook_ids);
            close(logFile);
            close(sockfd);
            return 1;
        }
    }

    // Wait for threads to finish
    a = pthread_join(managerThread, NULL);
    if (a != 0) {
        printf("Error joining manager thread\n");
        clear();
        free(prepared_by_cook);
        free(orders);
        free(cook_ids);
        free(delivery_ids);
        close(logFile);
        close(sockfd);
        return 1;
    }
    for (int i = 0; i < cookPoolSize; i++) {
        a = pthread_join(cookThread[i], NULL);
        if (a != 0) {
            printf("Error joining worker thread\n");
            clear();
            free(prepared_by_cook);
            free(orders);
            free(cook_ids);
            free(delivery_ids);
            close(logFile);
            close(sockfd);
            return 1;
        }
    }
    for (int i = 0; i < deliveryPoolSize; i++) {
        a = pthread_join(deliveryPersonelThread[i], NULL);
        if (a != 0) {
            printf("Error joining worker thread\n");
            clear();
            free(prepared_by_cook);
            free(orders);
            free(cook_ids);
            free(delivery_ids);
            close(logFile);
            close(sockfd);
            return 1;
        }
    }

    clear();
    free(prepared_by_cook);
    free(orders);
    free(cook_ids);
    free(delivery_ids);
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
    int cook_id = *(int*)arg;
    int oven_in_use = 0;
    int orderId1, orderId2;
    int inPreparation = 0;
    while (1) {
        // Wait for customers
        pthread_mutex_lock(&cookMutex);
        while (customers_to_serve == 0) {
            printf("Cook waiting...\n");
            pthread_cond_wait(&condCook, &cookMutex);
        }
        customers_to_serve--; // Decrease the count of customers to serve
        orderId1 = ++orderIdIndex;
        inPreparation = 1;
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
                inPreparation = 0;
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
                // Check if there are more customers to serve
                pthread_mutex_lock(&cookMutex);
                if (customers_to_serve == 0) {
                    pthread_mutex_unlock(&cookMutex);
                } else {
                    customers_to_serve--; // Decrease the count of customers to serve
                    orderId2 = ++orderIdIndex;
                    pthread_mutex_unlock(&cookMutex);
                    inPreparation = 1;
                    // Prepare the next meal
                    printf("Cook preparing another meal...\n");
                }
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
                    char log_msg[100];
                    sprintf(log_msg, "Cook %d prepared order %d\n", cook_id, orderId1);
                    prepared_by_cook[prepared_meals].orderId = orderId1;
                    orderId1 = orderId2;
                    writeLogMessage(log_msg);
                    prepared_by_cook[prepared_meals++].cookId = cook_id; //******
                    if (prepared_meals % 3 == 0 || (customers_to_serve == 0 && prepared_meals == total_customers % 3)) {
                        pthread_cond_signal(&condManager);
                    }
                    pthread_mutex_unlock(&mealMutex);

                    oven_in_use = 0; // Meal is out of the oven
            }

            
            if (inPreparation == 0) {
                break;
            }
            sleep(10);  // Simulating the preparation time for the next meal
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
                    // Release the door and the shovel
                    sem_post(&door_access);
                    sem_post(&shovels);
                    inPreparation = 0;
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

void *deliveryPersonel(void* arg) {
    int delivery_id = *(int*)arg;
    while (1) {
        pthread_mutex_lock(&deliveryMutex);
        while (head == last && deliveryPackage == 0) {
            printf("Delivery personel waiting...\n");
            pthread_cond_wait(&deliveryCond, &deliveryMutex);
        }
        int totalOrders = deliveryPackage;
        order deliveryOrders[3];  
        for(int i = 0; i < totalOrders; i++) {
            deliveryOrders[i] = orders[head];
            head = (head + 1) % cookPoolSize;
        }
        deliveryPackage = 0;
        pthread_mutex_unlock(&deliveryMutex);
        for (int i = 0; i < totalOrders; i++) {
            sleep(deliveryOrders[i].totalTime);
            char msg[128];
            sprintf(msg, "Delivery %d delivered order %d\n", delivery_id, deliveryOrders[i].order_id);
            writeLogMessage(msg);
        }
    }
    pthread_exit(0);
}

void handle_client(int client_socket) {
    client_information info;
    client_server_message clientMsg;
    if (read(client_socket, &info, sizeof(client_information)) <= 0) {
        perror("Failed to read from client");
        close(client_socket);
        return;
    }
    printf("%d new customers from %d.. Serving\n", info.numberOfCustomers, info.pid);
    char msg[128];
    snprintf(msg, sizeof(msg), "%d new customers from %d.. Serving\n", info.numberOfCustomers, info.pid);
    writeLogMessage(msg);

    snprintf(clientMsg.msg, sizeof(clientMsg.msg), "Orders received. Meals are preparing\n");
    if (write(client_socket, &clientMsg, sizeof(client_server_message)) < 0) {
        perror("Failed to send message");
    }
    // Order deneme
    order deliveryOrders[3]; 
    for (int i = 0; i < 3; i++) {
        generateRandomPQ(&deliveryOrders[i].p, &deliveryOrders[i].q, info.p, info.q);
        deliveryOrders[i].pid = info.pid;
        deliveryOrders[i].cookId = 0;
        deliveryOrders[i].p = info.p;
        deliveryOrders[i].q = info.q;
        deliveryOrders[i].totalTime = (info.p + info.q)/deliverySpeed;
    }
    // Order bitiş

    pthread_mutex_lock(&cookMutex);
    customers_to_serve = info.numberOfCustomers;
    total_customers = info.numberOfCustomers;
    pthread_cond_broadcast(&condCook);
    pthread_mutex_unlock(&cookMutex);

    while (total_customers > 0) {
        pthread_mutex_lock(&mealMutex);
        while (prepared_meals < 3 && total_customers > 3) {
            pthread_cond_wait(&condManager, &mealMutex);
        }

        if (prepared_meals >= 3) {
            printf("3 meals completed.\n");
            printf("First one prepared by cook %d\n", prepared_by_cook[0].cookId);
            pthread_mutex_lock(&deliveryMutex);
            deliveryOrders[0].cookId = prepared_by_cook[0].cookId;
            orders[last] = deliveryOrders[0];
            orders[last].order_id = prepared_by_cook[0].orderId;
            last = (last + 1) % cookPoolSize;
            printf("Second one prepared by cook %d\n", prepared_by_cook[1].cookId);
            deliveryOrders[1].cookId = prepared_by_cook[1].cookId;
            orders[last] = deliveryOrders[1];
            orders[last].order_id = prepared_by_cook[1].orderId;
            last = (last + 1) % cookPoolSize;
            printf("Third one prepared by cook %d\n", prepared_by_cook[2].cookId);
            deliveryOrders[2].cookId = prepared_by_cook[2].cookId;
            orders[last] = deliveryOrders[2];
            orders[last].order_id = prepared_by_cook[2].orderId;
            last = (last + 1) % cookPoolSize;
            deliveryPackage = 3;
            snprintf(clientMsg.msg, sizeof(clientMsg.msg), "Order %d-%d-%d prepared. Will be delivered soon!\n", prepared_by_cook[0].orderId, prepared_by_cook[1].orderId, prepared_by_cook[2].orderId);
            if (write(client_socket, &clientMsg, sizeof(client_server_message)) < 0) {
                perror("Failed to send message");
            }
            total_customers -= 3;
            prepared_meals -= 3;
            pthread_cond_signal(&deliveryCond);
            pthread_mutex_unlock(&deliveryMutex);
        } else if (total_customers <= 3 && prepared_meals == total_customers) {
            // Wait for the last remaining meals to be prepared
            printf("Meals finished, %d meals prepared at last.\n", prepared_meals);
            pthread_mutex_lock(&deliveryMutex);
            deliveryPackage = prepared_meals;
            for (int i = 0; i < prepared_meals; i++) {
    
                printf("Meal %d prepared by cook %d\n", i + 1, prepared_by_cook[i].cookId);
                deliveryOrders[i].cookId = prepared_by_cook[i].cookId;
                orders[last] = deliveryOrders[i];
                orders[last].order_id = prepared_by_cook[i].orderId;
                last = (last + 1) % cookPoolSize;
    
            }
            total_customers -= prepared_meals;
            if(prepared_meals == 1) {
                snprintf(clientMsg.msg, sizeof(clientMsg.msg), "Order %d prepared. Will be delivered soon!\n", prepared_by_cook[0].orderId);
            } else if(prepared_meals == 2) {
                snprintf(clientMsg.msg, sizeof(clientMsg.msg), "Order %d-%d prepared. Will be delivered soon!\n", prepared_by_cook[0].orderId, prepared_by_cook[1].orderId);
            }
            if (write(client_socket, &clientMsg, sizeof(client_server_message)) < 0) {
                perror("Failed to send message");
            }
            prepared_meals = 0;
            pthread_cond_signal(&deliveryCond);
            pthread_mutex_unlock(&deliveryMutex);
        }

        pthread_mutex_unlock(&mealMutex);
    }
    printf("> done serving client @ XXX PID %d\n", info.pid);
    snprintf(clientMsg.msg, sizeof(clientMsg.msg), "Done serving meals for customers\n");
    if (write(client_socket, &clientMsg, sizeof(client_server_message)) < 0) {
        perror("Failed to send message");
    }
    // printf("> Thanks Cook %d and Moto %d\n", deliveryOrders[i].cookId, delivery_id);
    close(client_socket);
}

void generateRandomPQ(int *p, int *q, int maxP, int maxQ) {
    *p = rand() % (maxP + 1);
    *q = rand() % (maxQ + 1);
}
int min(int a, int b) {
    return (a < b) ? a : b;
}

/* Function to write message to log file */
void writeLogMessage(char* startMessage)
{
    char errorMessage[400];
    // Edit log message with adding time
    createMessageWithTime(startMessage, errorMessage);
    // Write to log file
    while((write(logFile, errorMessage, strlen(errorMessage))==-1) && (errno==EINTR));

}

/* Add time and date to log message and return it */
void createMessageWithTime(char* startMessage, char* message)
{
    time_t currentTime;
    char res[32];
    struct tm *lt;
    // Get time
    time(&currentTime);
    lt=localtime(&currentTime);
    strftime(res, 32, default_format, lt);
    // Add time and startMessage to a single string 
    strcpy(message, res);
    strcat(message, ": ");
    strcat(message, startMessage);
}

void clear() {
    pthread_mutex_destroy(&deliveryMutex);
    pthread_cond_destroy(&deliveryCond);
    pthread_mutex_destroy(&cookMutex);
    pthread_mutex_destroy(&mealMutex);
    pthread_cond_destroy(&condCook);
    pthread_cond_destroy(&condManager);
    pthread_mutex_destroy(&logMutex);
    sem_destroy(&oven_space);
    sem_destroy(&door_access);
    sem_destroy(&shovels);
}