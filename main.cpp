#define T 2
#define MSG_WANT 1
#define MSG_WANT_SIZE 2
#define MSG_RELEASE 2
#define MSG_RELEASE_SIZE 4
#define MSG_PERMISSION 3
#define MSG_PERMISSION_SIZE 2
#define MSG_MAX_SIZE 4

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
pthread_mutex_t releaseLock;
pthread_mutex_t clockLock;
int msg_recv[MSG_MAX_SIZE];
int msg_send[MSG_MAX_SIZE];
MPI_Status status;

bool can_i_enter_critical_section(){
    return permissions_to_go == 0
        && legion_size < (roads_capacity[want_road_id] - people_already_in_want_road);
}

void *RecvMessages(void *arg) {
    while (1) {
        MPI_Recv(msg_recv, MSG_MAX_SIZE, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        int sender_clock = msg_recv[0];
        int sender_id = status.MPI_SOURCE;
        pthread_mutex_lock(&clockLock);
        msg_recv[0] = my_clock;
        my_clock = std::max(my_clock, sender_clock) + 1;
        pthread_mutex_unlock(&clockLock);
        switch (status.MPI_TAG) {
            case MSG_WANT: {
                int road_id = msg_recv[1];
                pthread_mutex_lock(&releaseLock);
                if(want_road_id == road_id) {
                    pthread_mutex_lock(&clockLock);
                    if ((want_road_clock < sender_clock) || (want_road_clock == sender_clock && rank < sender_id)) {
                        legions_id_wanted_same_road.push_back(sender_id);
                        legions_id_wanted_same_road.push_back(sender_clock);
                        msg_recv[1] = legion_size;
                        MPI_Send(msg_recv, MSG_PERMISSION_SIZE, MPI_INT, sender_id, MSG_PERMISSION, MPI_COMM_WORLD);
                    }else{
                        msg_recv[1] = 0;
                        MPI_Send(msg_recv, MSG_PERMISSION_SIZE, MPI_INT, sender_id, MSG_PERMISSION, MPI_COMM_WORLD);
                    }
                    pthread_mutex_unlock(&clockLock);
                } else{
                    msg_recv[1] = 0;
                    MPI_Send(msg_recv, MSG_PERMISSION_SIZE, MPI_INT, sender_id, MSG_PERMISSION, MPI_COMM_WORLD);
                }
                pthread_mutex_unlock(&releaseLock);
                break;
            }
            case MSG_RELEASE: {
                int road_id = msg_recv[1];
                int other_legion_size = msg_recv[2];
                int want_road_clock_from_sender = msg_recv[3];
                if(want_road_id == road_id && want_road_clock == want_road_clock_from_sender) {
                    people_already_in_want_road -= other_legion_size;
                    if(can_i_enter_critical_section()){
                        pthread_mutex_unlock(&lock);
                    }
                }
                break;
            }
            case MSG_PERMISSION: {
                permissions_to_go--;
                int people_in_road_from_process = msg_recv[1];
                people_already_in_want_road += people_in_road_from_process;
                if(can_i_enter_critical_section()){
                    pthread_mutex_unlock(&lock);
                }
            }
        }
    }
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    
    pthread_t thread_id;
    errno = pthread_create(&thread_id, NULL, RecvMessages, NULL);

    //for (int &road_capacity : roads_capacity) {
    for(int i = 0; i < T; i++){
        roads_capacity[i] = 300 + ((i + 13) * T * 5) % 100;
        printf("Droga ma rozmiar %d.\n", roads_capacity[i]);
    }
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    srand(static_cast<unsigned int>(rank * 100));
    legion_size = 200 + rand() % 100;
    printf("Legion %d ma rozmiar %d.\n",rank ,legion_size);
    MPI_Comm_size(MPI_COMM_WORLD, &L);
    pthread_mutex_unlock(&lock);
    while(1) {
        int sleepTime = 3 + rand() % 5;
        //printf("%d: Rozkladamy oboz na %d sekund\n", rank, sleepTime);
        sleep(sleepTime);
        want_road_id = rand()%T;
        printf("%d: Otrzymalem rozkaz przemieszczenia traktem %d\n", rank, want_road_id);
        //printf("%d: Pytam sie innych czy moge wejsc na trakt\n", rank);
        pthread_mutex_lock(&clockLock);
        my_clock++;
        want_road_clock = my_clock;
        pthread_mutex_unlock(&clockLock);
        people_already_in_want_road = 0;
        permissions_to_go = L - 1;
        msg_send[0] = want_road_clock;
        msg_send[1] = want_road_id;
        pthread_mutex_lock(&lock);
        for(int i = 0; i < L; i++){
            if(i != rank){
                MPI_Send(msg_send, MSG_WANT_SIZE, MPI_INT, i, MSG_WANT, MPI_COMM_WORLD);
            }
        }
        //printf("%d: Czekam na pozwolenia\n", rank);
        pthread_mutex_lock(&lock);
        sleepTime = 3 + rand() % 5;
        printf("%d: Wchodze na %d trakt na %d sekund\n", rank, want_road_id, sleepTime);
        sleep(sleepTime);
        pthread_mutex_unlock(&lock);
        printf("%d: Wychodze z %d traktu i informuje o tym zainteresowanych\n", rank, want_road_id);
        pthread_mutex_lock(&clockLock);
        msg_send[0] = my_clock;
        pthread_mutex_unlock(&clockLock);
        msg_send[1] = want_road_id;
        msg_send[2] = legion_size;
        want_road_id = -1;
        pthread_mutex_lock(&releaseLock);
        while (legions_id_wanted_same_road.size() > 0){
            msg_send[3] = legions_id_wanted_same_road.back();
            legions_id_wanted_same_road.pop_back();
            MPI_Send(msg_send, MSG_RELEASE_SIZE, MPI_INT, legions_id_wanted_same_road.back(), MSG_RELEASE, MPI_COMM_WORLD);
            legions_id_wanted_same_road.pop_back();
        }
        pthread_mutex_unlock(&releaseLock);
    }
    errno = pthread_join(thread_id, NULL);
    return 0;
}
