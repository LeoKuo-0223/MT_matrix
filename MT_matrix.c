#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <time.h>

unsigned long **MTa, **MTb, **Res;
int *RC_a;
int *RC_b; 
FILE *retf;
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
pid_t pid;
struct timespec start, finish;
double elapsed;
int first=1;

typedef struct Multi_info{
    int rStart, rEnd, cStart, cEnd;
}Multi_info;




unsigned long ** getMT(char *filename,int *RC){
    char *tok;
    char MT_info[16];
    unsigned long  **Arr;
    FILE *f = fopen(filename, "r");

    if(f==NULL){
        printf("fail to open file\n");
    }else{
        if(fgets(MT_info, 16, f) != NULL){
            tok = strtok(MT_info," \n");
            int i=0;
            while(tok!=NULL){
                RC[i] = atoi(tok);
                tok = strtok(NULL, " \n");
                i++;
            }
        }else {
            printf("MT file format error\n");
            return NULL;
        }

        //create matrix
        Arr = (unsigned long **)malloc( RC[0] *sizeof(unsigned long *));
        for(int i=0; i< RC[0]; i++){
            Arr[i] = (unsigned long *)malloc( RC[1] *sizeof(unsigned long ));
        }

        for(int i = 0; i < RC[0]; i++){
            for(int j = 0; j < RC[1]; j++){
                fscanf(f, "%lu", &Arr[i][j]);
            }
        }
    
        fclose(f);
        return Arr;
    }
}


void *mult_MT(void* arg){
    Multi_info *d = (Multi_info *)arg;
    pid_t tid = gettid();
    // printf("thread: %d, start\n", tid);
    FILE *pFile,*f;
    unsigned long  result=0;
    for(int i=d->rStart ; i<d->rEnd ; i++){
        // printf("thread: %d, Row%d\n", tid, i );
        for(int j=d->cStart ; j< d->cEnd ; j++){
            result=0;
            for(int k=0;k<RC_a[1];k++){
                result+=MTa[i][k] * MTb[k][j];
            }
            Res[i][j] = result;
        }
    }

    //finish calculating
    pthread_mutex_lock(&m);   // 上鎖
    pFile = fopen("/proc/thread_info","w");
    if(pFile==NULL){
        perror("fopen");
        printf("fail to open /proc/thread_info\n");
    }else{
        fprintf(pFile,"%d",tid);
        fclose(pFile);
    }
    
    pthread_mutex_unlock(&m); // 解鎖
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    pid = getpid();
    int num_thds = atoi(argv[1]);
    pthread_t *t_ptr;
    retf = fopen("result.txt", "w"); //prepare result txt file
    if(retf==NULL){
        printf("Fail to create result file\n");
        return 0;
    }
    //if create file successfully
    RC_b = malloc(2*sizeof(int));
    RC_a = malloc(2*sizeof(int));
    //get matrix infomation
    MTa = getMT(argv[2],RC_a);
    MTb = getMT(argv[3],RC_b);
    Res= (unsigned long **)malloc( RC_a[0] *sizeof(unsigned long *));     //used to store the result of multiplication
    for(int i=0; i< RC_a[0]; i++) Res[i] = (unsigned long *)malloc( RC_b[1] *sizeof(unsigned long ));
    
    int num_entries  = RC_a[0]*RC_b[1];
    char buffer[16];
    sprintf(buffer, "%d %d\n", RC_a[0], RC_b[1]);   //answer's dimension
    fwrite(buffer ,sizeof(char), strlen(buffer), retf); //write into result file

    t_ptr = malloc(num_thds*sizeof(pthread_t));     //create thread array
    Multi_info *data= malloc(num_thds * sizeof(Multi_info)); //data array

    int remainder,quotient;
    //calculate how much work should do by a thread
    if(RC_b[1]>=RC_a[0]){   //calculate average of column for a thread (column is longer)
        remainder = RC_b[1]%num_thds;
        quotient = (RC_b[1]-remainder)/num_thds;

        //start allocate work
        int i=0;
        for(int k=0;k<RC_b[1];k+=quotient){
            data[i].rStart=0;
            data[i].rEnd = RC_a[0];
            data[i].cStart = k;
            // printf("create one thread\n");
            if(i==num_thds-1) {
                data[i].cEnd = RC_b[1]; //the last thread do all the remaining works,
                if(first) {clock_gettime(CLOCK_MONOTONIC, &start);first=0;} //record the clock
                pthread_create(&t_ptr[i], NULL, mult_MT, (void*)(&data[i]));
                break;
            }
            else data[i].cEnd = k + quotient;
            if(first) {clock_gettime(CLOCK_MONOTONIC, &start);first=0;} //record the clock
            pthread_create(&t_ptr[i], NULL, mult_MT, (void*)(&data[i]));

            i++;
        }

    }else{  //calculate average of row for a thread (row is longer)
        remainder = RC_a[0]%num_thds;
        quotient = (RC_a[0]-remainder)/num_thds;
        //start allocate work
        int i=0;
        for(int k=0;k<RC_a[0];k+=quotient){
            data[i].cStart=0;
            data[i].cEnd = RC_b[1];
            data[i].rStart = k;
            if(i==num_thds-1) {
                data[i].rEnd = RC_a[0]; //the last thread do all the remaining works,
                if(first) {clock_gettime(CLOCK_MONOTONIC, &start);first=0;} //record the clock
                pthread_create(&t_ptr[i], NULL, mult_MT, (void*)(&data[i]));
                break;
            }
            else data[i].rEnd = k + quotient;
            if(first) {clock_gettime(CLOCK_MONOTONIC, &start);first=0;} //record the clock
            pthread_create(&t_ptr[i], NULL, mult_MT, (void*)(&data[i]));

            i++;
        }
    }

    //wait for all thread finish the jobs
    for(int i=0;i<num_thds;i++){
        pthread_join(t_ptr[i],  NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &finish); //record the clock
    elapsed = (finish.tv_sec - start.tv_sec);


//write into result file
    for(int i=0;i< RC_a[0];i++){
        for(int j=0; j<RC_b[1]; j++){
            sprintf(buffer, "%lu", Res[i][j]);
            fwrite(buffer ,sizeof(char), strlen(buffer), retf);
            if( j == RC_b[1]-1) fwrite("\n" ,sizeof(char), strlen("\n"), retf);
            else fwrite(" " ,sizeof(char), strlen(" "), retf);
        }
    }
    //remember to close the result file
    fclose(retf);
    
    // read the result
    FILE *pFile;
    char context_switch[16];
    char threadID[16];
    char time[16];
    char elapsed_time[16];
    pFile = fopen("/proc/thread_info", "r");
    if(pFile==NULL){
        printf("open /proc/thread_info fail\n");
    }else{
        printf("PID:%d\n",getpid()); 
        for(int i=0;i<num_thds;i++){
            fscanf(pFile,"%s %s %s", threadID, time,context_switch);
            printf("\tthreadID: %s Time:%s(ms) context switch times:%s\n", threadID, time,context_switch);
        }
    }
    fclose(pFile);

    //elapsed time
    printf("Elapsed time: %d\n", (int)elapsed);


    //destroy lock
    pthread_mutex_destroy(&m); // 銷毀鎖
    // free memory
    for(int i=0; i< RC_a[0] ;i++) { free(MTa[i]); free(Res[i]);}
    for(int i=0; i< RC_b[0] ;i++) free(MTb[i]);
    free(Res);
    free(MTa);free(MTb);
    free(RC_a);free(RC_b);

    return 0;
}

