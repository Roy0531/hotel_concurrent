#include <stdio.h> 
#include <stdlib.h> 
#include <pthread.h> 
#include <semaphore.h>
#include <sys/shm.h>

// set the max number of each role
#define GUEST 25
#define EMPLOYEE 2
#define BELLHOP 2

// vars for queue 1
int queue1[GUEST] = {0};
int front1 = 0;
int rear1 = 0;
int itemCount1 = 0;

// vars for queue 2
int queue2[GUEST] = {0};
int front2 = 0;
int rear2 = 0;
int itemCount2 = 0;

int guest_cnt = 0;
int emp_cnt = 0;
int bell_cnt = 0;
int room_cnt = 1;
//store all the instance of the guests and their info 
// info[guest_number][guest_number, emp_number, room_number, bellhop_number, bag_number]
int info[GUEST][5] = {0};

int shmid;

// semaphores
sem_t desk_open;
sem_t bell_avairable;
sem_t mutex1;
sem_t mutex2;
sem_t mutex3;
sem_t mutex4;
sem_t mutex5;
sem_t guest_ready;
sem_t key_received;
sem_t bag_ready;
sem_t room_entered;
sem_t bag_received;

sem_t *registered;
sem_t *received;
sem_t *delivered;


// queue used between guest and employee
void enqueue1(int data) {
    if(itemCount1 != GUEST){
        if(rear1 == GUEST-1) {
            rear1 = 0;            
        }
        queue1[rear1++] = data;
        itemCount1++;
    }
}
int dequeue1() {
    int data = queue1[front1++];
	
    if(front1 == GUEST - 1) {
        front1 = 0; 
    }
	
    itemCount1--;
    return data;  
}
// queue used between guest and bellhop
void enqueue2(int data) {
    if(itemCount2 != GUEST){
        if(rear2 == GUEST-1) {
            rear2 = 0;            
        }
        queue2[rear2++] = data;
        itemCount2++;
    }
}   
int dequeue2() {
    int data = queue2[front2++];
	
    if(front2 == GUEST - 1) {
        front2 = 0;
    }
        
    itemCount2--;
    return data;  
}


void *guest(){
    // guest number of this thread
    int guest_num;
    // generate a random number between 0 and 5 for every guest
    int bag_cnt = rand() % 6;

    // creating a new guest
    sem_wait(&mutex1);
    guest_num = guest_cnt;
    printf("guest %d created\n", guest_num);
    info[guest_num][0] = guest_num;
    info[guest_num][4] = bag_cnt;
    guest_cnt++;
    sem_post(&mutex1);

    // guest enter the hotel
    if (bag_cnt < 2){
        printf("Guest %d entered with %d bag\n", info[guest_num][0], info[guest_num][4]);
    } else {
        printf("Guest %d entered with %d bags\n", info[guest_num][0], info[guest_num][4]);
    }
    // wait for the desks to open
    sem_wait(&desk_open);

    // guest getting ready
    sem_wait(&mutex3);
    enqueue1(guest_num);    
    sem_post(&guest_ready);
    sem_post(&mutex3);

    // wait for the room key from the eomployee
    sem_wait(&registered[guest_num]);

    // guest received the room key
    printf("Guest %d receives room key for room %d from front desk employee %d\n", info[guest_num][0], info[guest_num][2], info[guest_num][1]);
    sem_post(&key_received);

    // communication between a guest wiht more than 3 bags and a bellhop
    if (info[guest_num][4] >= 3){
        // wait for a bellhop to be available
        printf("Guest %d requests help with bags\n",info[guest_num][0]);
        sem_wait(&bell_avairable);

        // guest handing over the bags
        sem_wait(&mutex5);
        enqueue2(guest_num);
        // printf("enqueued: %d\n", guest_num);
        sem_post(&bag_ready);
        sem_post(&mutex5);

        // wait for the bellhop to take the bags
        sem_wait(&received[info[guest_num][0]]);

        printf("Guest %d enters room %d\n", info[guest_num][0], info[guest_num][2]);

        // signal the bellhop that guest has entered the room
        sem_post(&room_entered);

        // wait for the bellhop to deliver the bags
        sem_wait(&delivered[info[guest_num][0]]);
        
        printf("Guest %d receives bags from bellhop %d and gives tip\n", info[guest_num][0], info[guest_num][3]);
        // receive the bag delivered and reliese the bellhop
        sem_post(&bag_received);

    } else {
        printf("Guest %d enters room %d\n", info[guest_num][0], info[guest_num][2]);
    }
    
    printf("Guest %d retires for the evening\n", info[guest_num][0]);
    pthread_exit(NULL);
}

void *employee(){
    // employee number of this thread
    int emp_num;
    int h_guest;
    int room_num;

    // creating a new employee
    sem_wait(&mutex2);
    emp_num = emp_cnt;
    printf("Employee %d created\n", emp_num);
    emp_cnt++;
    sem_post(&mutex2);
    
    while(1){
        // wait for a new guest to be ready
        sem_wait(&guest_ready);

        // employee hadles a new guest
        sem_wait(&mutex3);
        h_guest = dequeue1();
        info[h_guest][1] = emp_num;
        sem_post(&mutex3);

        // register the guest and find an available room
        sem_wait(&mutex2);
        room_num = room_cnt;
        room_cnt++;
        info[h_guest][2] = room_num;
        printf("Front desk employee %d registers guest %d and assigns room %d\n", info[h_guest][1], info[h_guest][0], info[h_guest][2]);
        sem_post(&mutex2);

        // passing a room key
        sem_post(&registered[h_guest]);

        // wait for the guest to receive the room key
        sem_wait(&key_received);

        // call the next guest in line
        sem_post(&desk_open);
    }
}
void *bellhop(){
    // bellhop number of this thread
    int bell_num;
    int b_guest;

    // create a new bellhop
    sem_wait(&mutex4);
    bell_num = bell_cnt;
    printf("Bellhop %d created\n", bell_num);
    bell_cnt++;
    sem_post(&mutex4);

    while(1){
        // wait for new request from a guest
        sem_wait(&bag_ready);

        // bellhop handles a new guest
        sem_wait(&mutex5);
        b_guest = dequeue2();
        info[b_guest][3] = bell_num;
        sem_post(&mutex5);

        // signal the guest that the bags has been received
        printf("Bellhop %d receives bags from guest %d \n", info[b_guest][3], info[b_guest][0]);
        sem_post(&received[info[b_guest][0]]); 

        // wait for the guest enter the room
        sem_wait(&room_entered);

        // signal the guest that the bellhop has arrived the room
        printf("Bellhop %d delivers bags to guest %d\n", info[b_guest][3], info[b_guest][0]);
        sem_post(&delivered[info[b_guest][0]]); 

        // wait for the guest receive th bags
        sem_wait(&bag_received);

        // bellhop goes handle a new request
        sem_post(&bell_avairable);
    }
    
}
int main() 
{ 
    pthread_t guest_t[GUEST]; 
    pthread_t emp_t[EMPLOYEE]; 
    pthread_t bell_t[BELLHOP]; 

    // initialize semaphores
    sem_init(&desk_open, 0, EMPLOYEE);
    sem_init(&bell_avairable, 0, BELLHOP);
    sem_init(&mutex1, 0, 1);
    sem_init(&mutex2, 0, 1);
    sem_init(&mutex3, 0, 1);
    sem_init(&mutex4, 0, 1);
    sem_init(&mutex5, 0, 1);
    sem_init(&guest_ready, 0, 0);
    sem_init(&key_received, 0, 0);
    sem_init(&bag_ready, 0, 0);
    sem_init(&bag_received, 0, 0);
    sem_init(&room_entered, 0, 0);
    shmid = shmget(IPC_PRIVATE, sizeof(sem_t) * GUEST, IPC_CREAT | 0600);
    registered = shmat(shmid, NULL, 0);
    received = shmat(shmid, NULL, 0);
    delivered = shmat(shmid, NULL, 0);
    
    for (int k = 0; k < GUEST; k++) 
    {
        sem_init(&registered[k], 0, 0);
        sem_init(&received[k], 0, 0);
        sem_init(&delivered[k], 0, 0);
        
    }

    printf("Simulation starts\n");

    // create employee threads
    for (int i = 0; i < EMPLOYEE; ++i) {
        emp_t[i] = i;
        if (pthread_create(&emp_t[i], NULL, employee, NULL)) {
            perror("error emp_pthread_create\n");
            return 1;
        }
    }

    // create bellhop threads
    for (int i = 0; i < BELLHOP; ++i) {
        emp_t[i] = i;
        if (pthread_create(&bell_t[i], NULL, bellhop, NULL)) {
            perror("error emp_pthread_create\n");
            return 1;
        }
    }

    // create guest threads
    for (int i = 0; i < GUEST; ++i) {
        guest_t[i] = i;
        if (pthread_create(&guest_t[i], NULL, guest, NULL)) {
            perror("error guest_pthread_create");
            return 1;
        }
    }

    // join guest threads
    for (int i = 0; i < GUEST; ++i) {
        if (pthread_join(guest_t[i], NULL) != 0) {
            perror("error emp_pthread_join\n");
            return 1;
        }
        printf("Guest %d joined\n", i);
    }
    
    printf("Simulation ends\n");

    return 0; 
} 