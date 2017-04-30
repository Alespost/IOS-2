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


pid_t adultGen, childGen; //PID generatoru procesu adult a child
pid_t adult, child; //PID procesu adult a child

int A;//pocet procesu adult, ktere se budou generovat
int C;//pocet procesu child, ktere se budou generovat
int *line;//poradove cislo akce
int *adultsInCenter, *childsInCenter;//pocet procesu adult a child v centru
int *totalAdults;//pocet procesu adult, ktere jiz opustily centrum
int *waitingEnter;//pocet procesu child cekajicich na vstup do centra
int *waitingLeave;//pocet procesu adult cekajicich na odchod z centra
int *left;//celkovy pocet procesu, ktere opustily centrum

//semafory pro rizeni vstupu do centra a vystupu z nej,
//na ukonceni vsech procesu najednou a uzamceni pri pouzivani sdilene pameti
sem_t *enter, *leave, *finish, *lock;

FILE *output;//ukazatel na soubor pro vystup

/**
 * Funkce vytvori sdilenou pamet, inicializuje semafory a otevre soubor pro zapis
 */
void init(){
    line = mmap(NULL, intSIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    *line = 1;

    waitingEnter = mmap(NULL, intSIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    *waitingEnter = 0;

    waitingLeave = mmap(NULL, intSIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    *waitingLeave = 0;

    left = mmap(NULL, intSIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    *left = 0;

    adultsInCenter = mmap(NULL, intSIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    *adultsInCenter = 0;

    childsInCenter = mmap(NULL, intSIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    *childsInCenter = 0;

    totalAdults = mmap(NULL, intSIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    *totalAdults = 0;

    enter = mmap(NULL, semSIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    sem_init(enter, 1, 0);

    leave = mmap(NULL, semSIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    sem_init(leave, 1, 0);

    finish = mmap(NULL, semSIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    sem_init(finish, 1, 0);

    lock = mmap(NULL, semSIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    sem_init(lock, 1, 1);

    if((output = fopen("proj2.out","w")) == NULL){
        fprintf(stderr, "Soubor se nepodarilo otevrit.\n");
    }

}

/**
 * Funkce uvolni sdilene pameti, znici semafory  uzavre souor pro zapis
 */
void destroy(){
    munmap(line, intSIZE);
    munmap(waitingEnter, intSIZE);
    munmap(waitingLeave, intSIZE);
    munmap(left, intSIZE);
    munmap(adultsInCenter, intSIZE);
    munmap(childsInCenter, intSIZE);
    munmap(totalAdults, intSIZE);

    sem_destroy(enter);
    sem_destroy(leave);
    sem_destroy(finish);
    sem_destroy(lock);

    munmap(enter, semSIZE);
    munmap(leave, semSIZE);
    munmap(finish, semSIZE);
    munmap(lock, semSIZE);

    fclose(output);
}

/**
 * Funkce simuluje chovani procesu adult
 * @param id identifikator procesu v ramci procesu adult
 * @param WT cas, po ktery bude proces simulovat cinnost
 */
void simulAdult(int id, int WT){
    int pid = fork();

    //kontrola zda byl fork uspesny
    if(pid < 0){
        perror("fork");
        exit(2);
    }

    //kod pro proces adult
    if(pid == 0) {
        sem_wait(lock);
        fprintf(output,"%d\t: A %d\t: started\n", (*line)++, id);fflush(output);//spusteni procesu adult
        sem_post(lock);

        sem_wait(lock);
        //vstup procesu adult do centra
        fprintf(output,"%d\t: A %d\t: enter\n", (*line)++, id);fflush(output);
        (*adultsInCenter)++;
        sem_post(lock);

        sem_wait(lock);
        //vpusteni cekajicich procesu child
        for (int i = 0; i < 3 && i < *waitingEnter; i++) {
            sem_post(enter);
        }
        sem_post(lock);

        usleep(WT);//uspani procesu (simulace cinnosti)

        sem_wait(lock);
        fprintf(output,"%d\t: A %d\t: trying to leave\n", (*line)++, id);fflush(output);
        sem_post(lock);

        sem_wait(lock);
        //kontrola podminky pri pokusu adultu o odchod
        if(*childsInCenter > (*adultsInCenter-1) * 3) {
            (*waitingLeave)++; //inkrementace poctu adultu cekajicich na odchod
            fprintf(output, "%d\t: A %d\t: waiting: %d: %d\n", (*line)++, id, *adultsInCenter, *childsInCenter);
            fflush(output);
            sem_post(lock);
            sem_wait(leave);//cekani na splneni podminky pro odchod
            (*waitingLeave)--; //dekrementace poctu adultu cekajicich na odchod (adult uz muze odejit)
        }else{
            sem_post(lock);
        }

        sem_wait(lock);
        fprintf(output,"%d\t: A %d\t: leave\n", (*line)++, id);fflush(output);
        (*adultsInCenter)--;//odchod adultu z centra
        (*totalAdults)++;//inkrementace poctu adultu, kteri jiz opustili centrum
        (*left)++;//inkrementace celkoveho poctu procesu, ktere jiz opustily centrum
        sem_post(lock);

        sem_wait(lock);
        //pokud jiz nebudou generovany procesy adult,
        //vpusti se do centra vsechny cekajici procesy child
        if(*totalAdults == A){
            for (int i = *waitingEnter; i > 0; i--) {
                sem_post(enter);
            }
        }
        sem_post(lock);

        sem_wait(lock);
        //ukonceni vsech procesu najednou
        if(*left == A+C){
            for (int i = 0; i < A+C; ++i) {
                sem_post(finish);
            }
        }
        sem_post(lock);

        //cekani az vsechny procesy ukonci svou cinnost v centru
        sem_wait(finish);
        sem_wait(lock);
        fprintf(output,"%d\t: A %d\t: finished\n", (*line)++, id);fflush(output);
        sem_post(lock);
        exit(0);
    } else{
        adult = pid;
    }
}

/**
 * Funkce simuluje chovani procesu child
 * @param id identifikator procesu v ramci procesu child
 * @param WT cas, po ktery bude proces simulovat cinnost
 */
void simulChild(int id, int WT){
    int pid = fork();

    //kontrola zda byl fork uspesny
    if(pid < 0){
        perror("fork");
        exit(2);
    }

    //kod pro proces child
    if(pid == 0) {
        sem_wait(lock);
        fprintf(output,"%d\t: C %d\t: started\n", (*line)++, id);fflush(output);
        sem_post(lock);

        sem_wait(lock);
        //kontrola podminky pro vstup procesu child do centra
        if(*childsInCenter == *adultsInCenter * 3 && *totalAdults < A) {
            (*waitingEnter)++;//inkrementace poctu procesu child cekajicich na vstup
            fprintf(output, "%d\t: C %d\t: waiting: %d: %d\n", (*line)++, id, *adultsInCenter, *childsInCenter);
            fflush(output);
            sem_post(lock);
            sem_wait(enter);//cekani na splneni podminky pro vstup
            (*waitingEnter)--;//dekrementace poctu procesu child cekajicich na vstup (vstup jiz je umoznen)
        }else{
            sem_post(lock);
        }

        sem_wait(lock);
        //vstup procesu child do centra
        fprintf(output,"%d\t: C %d\t: enter\n", (*line)++, id);fflush(output);
        (*childsInCenter)++;
        sem_post(lock);

        usleep(WT);//simulace cinnosti procesu child

        sem_wait(lock);
        fprintf(output,"%d\t: C %d\t: trying to leave\n", (*line)++, id);fflush(output);
        sem_post(lock);

        sem_wait(lock);
        //odchod procesu child z centra
        fprintf(output,"%d\t: C %d\t: leave\n", (*line)++, id);fflush(output);
        (*childsInCenter)--;
        (*left)++;//inkrementace celkoveho poctu procesu, ktere jiz opustily centrum
        //vpusteni cekajiciho procesu child, pokud by nebyla porusena podminka, nebo by nebranil v odchodu procesu adult
        if(*childsInCenter < (*adultsInCenter - *waitingLeave) * 3 && *waitingEnter){
            sem_post(enter);
        }
        sem_post(lock);

        sem_wait(lock);
        //povoleni odchodu procesu adult, pokud neni porusena podminka
        if(*childsInCenter <= (*adultsInCenter-1) * 3) {
            sem_post(leave);
        }
        sem_post(lock);

        sem_wait(lock);
        //ukonceni vsech procesu najednou
        if(*left == A+C){
            for (int i = 0; i < A+C; ++i) {
                sem_post(finish);
            }
        }
        sem_post(lock);

        //cekani az vsechny procesy ukonci svou cinnost v centru
        sem_wait(finish);
        sem_wait(lock);
        fprintf(output,"%d\t: C %d\t: finished\n", (*line)++, id);fflush(output);
        sem_post(lock);
        exit(0);
    }else{
        child = pid;
    }
}


//------------MAIN-----------------
int main(int argc, char **argv) {
    if (argc != 7){
        fprintf(stderr, "Chybny pocet argumentu.\n");
        return 1;
    }

    char *err = NULL;

    A = strtoul(argv[1], &err, 10); //pocet procesu adult, ktere maji byt vygenerovany
    if(*err != 0 || A <= 0){
        fprintf(stderr, "Chybny argument A.\n");
        return 1;
    }

    C = strtoul(argv[2], &err, 10); //pocet procesu child, ktere maji byt vygenerovany
    if(*err != 0 || C <= 0){
        fprintf(stderr, "Chybny argument C.\n");
        return 1;
    }

    int AGT = strtoul(argv[3], &err, 10); //max doba po ktere je generovan adult
    if(*err != 0 || AGT < 0 || AGT > 5000){
        fprintf(stderr, "Chybny argument AGT.\n");
        return 1;
    }

    int CGT = strtoul(argv[4], &err, 10); //max doba po ktere je generovan child
    if(*err != 0 || CGT < 0 || CGT > 5000){
        fprintf(stderr, "Chybny argument CGT.\n");
        return 1;
    }

    int AWT = strtoul(argv[5], &err, 10); //max doba simulace cinnosti pro adult
    if(*err != 0 || AWT < 0 || AWT > 5000){
        fprintf(stderr, "Chybny argument AWT.\n");
        return 1;
    }

    int CWT = strtoul(argv[6], &err, 10); //max doba simulace cinnosti pro child
    if(*err != 0 || CWT < 0 || CWT > 5000){
        fprintf(stderr, "Chybny argument CWT.\n");
        return 1;
    }

    int WT = 0; //cas, po ktery bude proces simulovat cinnost

    int pid;//, i, status, AdultWT, AdultGT, ChildWT, ChildGT;

    init();

    srand(time(NULL));

    pid = fork();//vytvoreni generatoru adultu
    //kontrola uspesnosti forku
    if(pid < 0){
        perror("fork");
        exit(2);
    }

    if(pid == 0){
        //proces generujici adulty
        for (int i = 1; i <= A; ++i) {
            if(AGT) usleep(rand()%(AGT+1)*1000);
            if(AWT) WT = rand()%(AWT+1)*1000;
            simulAdult(i, WT);
        }

        waitpid(adult, NULL, 0);
        exit(0);
    }else{
        //hlavni proces
        adultGen = pid;

        pid = fork();
        //kontrola uspesnosti forku
        if(pid < 0){
            perror("fork");
            exit(2);
        }

        if(pid == 0){
            //proces generujici deti
            for (int i = 1; i <= C; ++i) {
                if(CGT) usleep(rand() % (CGT + 1) * 1000);//vygenerovani nahodneho casu po kterem se vygeneruje proces child
                if(CWT)WT = rand()%(CWT+1)*1000;//vygenerovani casu, po ktery bude child simulovat svou cinnost
                simulChild(i, WT);//vytvoreni procesu child
            }

            waitpid(child, NULL, 0);
            exit(0);
        }else{
            //hlavni proces
            childGen = pid;
        }
    }

    //cekani na ukonceni generatoru procesu adult a child
    waitpid(adultGen, NULL, 0);
    waitpid(childGen, NULL, 0);

    //zniceni semaforu, uvolneni sdilenych pameti a uzavreni souboru
    destroy();
    return 0;
}