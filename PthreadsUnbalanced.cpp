#include <iostream>
#include <mpi.h>
#include <pthread.h>
#include <cstdlib>
#include <cmath>

constexpr int TASK_AMOUNT = 2000;
constexpr int ITERATION_COUNT = 5;
constexpr int SIN_CONST = 1000;
constexpr int L = 15000;
constexpr int MIN_TASKS_TO_SEND = 3;
constexpr int PERCENT = 30; // can be 50%
constexpr int STOP_CODE = -1; // Код остановки для потоков

typedef struct arguments {
    int* tasks;
} arguments_t;

int currentTask = 0;
int leftTasks = 0;
int localTasks = 0;
int allDoneTasks = 0;

pthread_mutex_t mutex;

void spaceByRank(int rank) {
    for (int i = 0; i < rank; ++i) {
        std::cout << "\t";
    }
}

void* recvTasks(void* tasksAndRank) {
    arguments_t params = *((arguments_t*)tasksAndRank);
    int sendTasks = 0;
    int recvStatus = 0;

    MPI_Status status;
    while (true) {
        MPI_Recv(&recvStatus, 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);

        if (recvStatus == STOP_CODE) {
            pthread_exit(nullptr);
        }
        pthread_mutex_lock(&mutex);
        if (leftTasks > MIN_TASKS_TO_SEND) {
            sendTasks = static_cast<int>(leftTasks * PERCENT / 100);
            leftTasks -= sendTasks;
            MPI_Send(&sendTasks, 1, MPI_INT, status.MPI_SOURCE, 1, MPI_COMM_WORLD);
            MPI_Send(&params.tasks[currentTask + 1], sendTasks, MPI_INT, status.MPI_SOURCE, 1, MPI_COMM_WORLD);
            currentTask += sendTasks;

        }
        else {
            pthread_mutex_unlock(&mutex);
            sendTasks = 0;
            MPI_Send(&sendTasks, 1, MPI_INT, status.MPI_SOURCE, 1, MPI_COMM_WORLD);
        }
        pthread_mutex_unlock(&mutex);
    }
}

void executeTasks(const int* tasks, int numTasks) {
    pthread_mutex_lock(&mutex);
    leftTasks = numTasks;
    int curTask = 0;

    while (leftTasks) {
        leftTasks--;
        localTasks++;
        allDoneTasks++;
        double task_size = tasks[curTask];
        pthread_mutex_unlock(&mutex);

        for (int i = 0; i < task_size; ++i) {
            sin(i * SIN_CONST);
        }

        pthread_mutex_lock(&mutex);
        curTask++;
    }
    pthread_mutex_unlock(&mutex);
}

int main(int argc, char** argv) {
    int provided = 0;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    int size;
    int rank;
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Status status;
    pthread_attr_t attrs;
    pthread_t recv;
    pthread_attr_init(&attrs);
    pthread_mutex_init(&mutex, nullptr);
    pthread_attr_setdetachstate(&attrs, PTHREAD_CREATE_JOINABLE);
    int processTasksAmount = TASK_AMOUNT / size + ((rank < TASK_AMOUNT % size) ? 1 : 0);
    leftTasks = processTasksAmount;
    //std::cout << processTasksAmount << " tasks for №" << rank << std::endl;
    int* tasks = new int[processTasksAmount];
    arguments_t params = { tasks };
    pthread_create(&recv, &attrs, recvTasks, &params);
    pthread_attr_destroy(&attrs);
    double startTime = MPI_Wtime();
    double iterationStartTime = 0;
    double iterationTime = 0;

    for (int num = 0; num < ITERATION_COUNT; ++num) {
        for (int i = 0; i < processTasksAmount; ++i) {
            tasks[i] = abs(100 - i % 100) * abs(rank - (num % size)) * L;
        }
        iterationStartTime = MPI_Wtime();
        // spaceByRank(rank);
        //std::cout << "Process №" << rank << " started" << std::endl;
        executeTasks(tasks, processTasksAmount);
        double time = MPI_Wtime() - iterationStartTime;
        // spaceByRank(rank);
        //std::cout << rank << " finished" << localTasks << " done " << time << std::endl;

        iterationTime = MPI_Wtime() - iterationStartTime;
        // spaceByRank(rank);
        //std::cout << rank << " finished iteration with time = " << iterationTime << std::endl;
        double minTime = 0;
        double maxTime = 0;
        MPI_Allreduce(&iterationTime, &minTime, 1, MPI_DOUBLE, MPI_MIN, MPI_COMM_WORLD);
        MPI_Allreduce(&iterationTime, &maxTime, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
        MPI_Barrier(MPI_COMM_WORLD);
        if (rank == 0) {
            double disbalance = maxTime - minTime;
            std::cout << "Disbalance = " << disbalance << std::endl;
            std::cout << "Disbalance in % = " << disbalance / maxTime * 100 << std::endl;
        }
        int executed_tasks = 0;
        MPI_Allreduce(&allDoneTasks, &executed_tasks, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
        // spaceByRank(rank);

        MPI_Barrier(MPI_COMM_WORLD);
        if (rank == 0) {
            std::cout << "Executed tasks count = " << executed_tasks << std::endl << std::endl;
        }
    }

    MPI_Send(&STOP_CODE, 1, MPI_INT, rank, 0, MPI_COMM_WORLD);
    if (rank == 0) {
        std::cout << "Whole time = " << MPI_Wtime() - startTime << std::endl;
    }

    delete[] tasks;
    pthread_join(recv, nullptr);
    pthread_mutex_destroy(&mutex);
    MPI_Finalize();
    return 0;
}