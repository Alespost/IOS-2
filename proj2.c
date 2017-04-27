#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>

#define semSIZE sizeof(sem_t)
#define intSIZE sizeof(int)


int A, C;
pid_t adultGen, childGen, adult, child;
int *line, *waitingEnter, *left;

void simulAdult(int id, int WT, sem_t *enter, sem_t *leave, sem_t *finish, int *adultsInCenter, int *childsInCenter, int *totalAdults){
    int pid = fork();

    //kontrola zda byl fork uspesny
    if(pid < 0){
        perror("fork");
        exit(2);
    }

    //kod pro proces adult
    if(pid == 0) {
        fprintf(stdout,"%d\tA %d\t: started\n", (*line)++, id);fflush(stdout);
        fprintf(stdout,"%d\tA %d\t: enter\n", (*line)++, id);fflush(stdout);
        (*adultsInCenter)++;
        usleep(WT);

        if(*childsInCenter < *adultsInCenter * 3) {
            for (int i = *waitingEnter; i > 0; i--) {
                sem_post(enter);
            }
        }

        fprintf(stdout,"%d\tA %d\t: trying to leave\n", (*line)++, id);fflush(stdout);

        if(*childsInCenter > (*adultsInCenter-1) * 3) {
            fprintf(stdout, "%d\tA %d\t: waiting: %d: %d\n", (*line)++, id, *adultsInCenter, *childsInCenter);
            fflush(stdout);
            sem_wait(leave);
        }

        fprintf(stdout,"%d\tA %d\t: leave\n", (*line)++, id);fflush(stdout);
        (*adultsInCenter)--;
        (*totalAdults)++;
        (*left)++;

        if(*left == A+C){
            for (int i = 0; i < A+C; ++i) {
                sem_post(finish);
            }
        }
        sem_wait(finish);
        fprintf(stdout,"%d\tA %d\t: finished\n", (*line)++, id);fflush(stdout);
        exit(0);
    } else{
        adult = pid;
    }
}

void simulChild(int id, int WT, sem_t *enter, sem_t *leave, sem_t *finish, int *adultsInCenter, int *childsInCenter, int *totalAdults){
    int pid = fork();

    //kontrola zda byl fork uspesny
    if(pid < 0){
        perror("fork");
        exit(2);
    }

    //kod pro proces child
    if(pid == 0) {
        fprintf(stdout,"%d\tC %d\t: started\n", (*line)++, id);fflush(stdout);

        if(*childsInCenter == *adultsInCenter * 3 && *totalAdults < A){
            fprintf(stdout,"%d\tC %d\t: waiting: %d: %d\n", (*line)++, id, *adultsInCenter, *childsInCenter);fflush(stdout);
            (*waitingEnter)++;
            sem_wait(enter);
        }

        fprintf(stdout,"%d\tC %d\t: enter\n", (*line)++, id);fflush(stdout);
        (*childsInCenter)++;
        usleep(WT);

        fprintf(stdout,"%d\tC %d\t: trying to leave\n", (*line)++, id);fflush(stdout);
        fprintf(stdout,"%d\tC %d\t: leave\n", (*line)++, id);fflush(stdout);
        (*childsInCenter)--;
        (*left)++;

        if(*childsInCenter <= (*adultsInCenter-1) * 3) {
            sem_post(leave);
        }

        if(*left == A+C){
            for (int i = 0; i < A+C; ++i) {
                sem_post(finish);
            }
        }
        sem_wait(finish);
        fprintf(stdout,"%d\tC %d\t: finished\n", (*line)++, id);fflush(stdout);
        exit(0);
    }else{
        child = pid;
    }
}


//------------MAIN-----------------
int main(int argc, char **argv) {
    if (argc != 7){
        fprintf(stderr, "Neocekavane argumenty.\n");
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        for (int j = 0; argv[i][j] != '\0' ; j++) {
            if(argv[i][j] < '0' || argv[i][j] > '9' ){
                if(!(j == 0 && argv[i][j] == '-')) {
                    fprintf(stderr, "Neocekavane argumenty.\n");
                    return 1;
                }
            }
        }
    }

    A = atoi(argv[1]); //pocet procesu adult, ktere maji byt vygenerovany
    C = atoi(argv[2]); //pocet procesu child, ktere maji byt vygenerovany
    int AGT = atoi(argv[3]); //max doba po ktere je generovan adult
    int CGT = atoi(argv[4]); //max doba po ktere je generovan child
    int AWT = atoi(argv[5]); //max doba simulace cinnosti pro adult
    int CWT = atoi(argv[6]); //max doba simulace cinnosti pro child
    int WT = 0; //cas, po ktery bude proces simulovat cinnost

    int pid;//, i, status, AdultWT, AdultGT, ChildWT, ChildGT;
    /*sem_t *sem;
    int *adultsInCentre;
    int *childsInCentre;
    int *lineNumber;
*/

    line = mmap(NULL, intSIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    *line = 1;

    waitingEnter = mmap(NULL, intSIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    *waitingEnter = 0;

    left = mmap(NULL, intSIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    *left = 0;

    int *adultsInCenter;
    adultsInCenter = mmap(NULL, intSIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    *adultsInCenter = 0;

    int *childsInCenter;
    childsInCenter = mmap(NULL, intSIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    *childsInCenter = 0;

    int *totalAdults;
    totalAdults = mmap(NULL, intSIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    *totalAdults = 0;

    sem_t *enter;
    enter = mmap(NULL, semSIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    sem_init(enter, 1, 0);
    sem_t *leave;
    leave = mmap(NULL, semSIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    sem_init(leave, 1, 0);
    sem_t *finish;
    finish = mmap(NULL, semSIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    sem_init(finish, 1, 0);

    srand(time(NULL));


    //setbuf(stdout,NULL);
    //setbuf(stderr,NULL);

    pid = fork();
    if(pid < 0){
        perror("fork");
        exit(2);
    }

    if(pid == 0){
        //proces generujici dospele
        for (int i = 1; i <= A; ++i) {
            if(AGT) usleep(rand()%(AGT+1)*1000);
            if(AWT) WT = rand()%(AWT+1)*1000;
            simulAdult(i, WT, enter, leave, finish, adultsInCenter, childsInCenter, totalAdults);
        }

        waitpid(adult, NULL, 0);
        exit(0);
    }else{
        //hlavni proces

        adultGen = pid;

        pid = fork();
        if(pid < 0){
            perror("fork");
            exit(2);
        }

        if(pid == 0){
            //proces generujici deti
            for (int i = 1; i <= C; ++i) {
                if(CGT) usleep(rand() % (CGT + 1) * 1000);
                if(CWT)WT = rand()%(CWT+1)*1000;
                simulChild(i, WT, enter, leave, finish, adultsInCenter, childsInCenter, totalAdults);
            }

            waitpid(child, NULL, 0);
            exit(0);
        }else{
            //hlavni proces
            childGen = pid;
        }
    }

    waitpid(adultGen, NULL, 0);
    waitpid(childGen, NULL, 0);
    return 0;
}