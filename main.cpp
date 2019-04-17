#define T 10
#define MSG_WANT 1
#define MSG_WANT_SIZE 2
#define MSG_RELEASE 2
#define MSG_RELEASE_SIZE 3
#define MSG_PERMISSION 3
#define MSG_PERMISSION_SIZE 2
#define MSG_MAX_SIZE 3

#include <pthread.h>
#include "mpi.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <vector>

int L, rank, legion_size;
int want_road_id = -1;
int my_clock, want_road_clock, people_already_in_want_road = 0;
int roads_capacity[T];
std::vector<int> legions_id_wanted_same_road;
int permissions_to_go;
pthread_mutex_t lock;
int msg[MSG_MAX_SIZE];
MPI_Status status;

void *RecvMessages(void *arg) {
    while (1) {
        MPI_Recv(msg, MSG_MAX_SIZE, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        int sender_clock = msg[0];
        int sender_id = status.MPI_SOURCE;
        my_clock = std::max(my_clock, sender_clock) + 1;
        switch (status.MPI_TAG) {
            case MSG_WANT: {
                int road_id = msg[1];
                if ((want_road_clock < sender_clock) || (want_road_clock == sender_clock && rank < sender_id)) {
                    legions_id_wanted_same_road.push_back(sender_id);
                }
                break;
            }
            case MSG_RELEASE: {
                break;
            }
            case MSG_PERMISSION: {
                break;
            }
        }
        pthread_mutex_unlock(&lock);
    }
}

int main() {
    for (int &road_capacity : roads_capacity) {
        road_capacity = 200 + rand() % 500;
    }
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    srand(static_cast<unsigned int>(rank * 100));
    legion_size = 50 + rand() % 100;
    MPI_Comm_size(MPI_COMM_WORLD, &L);

    return 0;
}
