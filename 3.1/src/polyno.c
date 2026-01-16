#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <sys/times.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <mpi.h>

#define MAX_MESSAGE_SIZE (2* grade * sizeof(long long int) + sizeof(int))

int RandomNumber(){
    return rand() % 100;
}

void SerialMult(long long int* A, long long int* B, long long int* C, int n){
    // arr[0]*x^n + arr[1]*x^n-1 ...
    for (int i = 0; i <= n; i++) {
        for (int j = 0; j <= n; j++) {
            C[i + j] += A[i] * B[j];
        }
    }
    return;
}

void printPol(long long int *A, int n, int mod){
    // printf("Polynomial %d: ", name);
    FILE*fp;
    if (mod == 1)
        fp=fopen("bin/polynomial.txt","w");
    else if (mod == 2)
        fp=fopen("bin/polynomial.txt","a");
    else if (mod == 3){
        fp=fopen("bin/res","w");
        for (int i=0; i < n + 1; i++){
            fprintf(stdout, "%lld\n", A[i]);
        }
        fprintf(stdout, "\n");
        fclose(fp);
        return;
    }
    else
        fp = stdout;
    for (int i=0; i < n + 1; i++){
        fprintf(fp, "%lldx^%d", A[i], n-i);
        if (i != n) {
            fprintf(fp, " + ");
        }
    }
    fprintf(fp, "\n");
    if (mod == 1 || mod == 2)
        fclose(fp);
}



void ThreadMult(long long int *A, long long int *B, long long int *C, long long int start, long long int grade2, long long int n) {
    long long end = start + n;
    for (long long i = start; i < end; i++) {
        long long int sum = 0;
        for (long long int j = 0; j < grade2; j++) {
            long long int temp = i - j;
            if (temp >= 0 && temp < grade2) {
                sum += A[j] * B[temp];
            }
        }
        C[i] = sum;
    }

}




int main(int argc, char **argv) {
    srand(63);
    MPI_Init(&argc, &argv);
    
    int comm_size;
    int my_rank;
    MPI_Comm_size(MPI_COMM_WORLD, &comm_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    long long int *C2;
    int grade = -1;
    // long long int* C;
    // int serial_flag = 0;

    double local_compute_time = 0;
    double max_compute_time = 0;

    double reduce_time = 0;
    double total_start = 0;
    double total_end = 0;
    double broadcast_time = 0;

    double t1, t2;
    if (my_rank == 0) {
        char opt;
        if (argc != 3){
            fprintf(stderr, "Usage prog.c -n <grade of polynomial>\n");
            return 1;
        }
        while ((opt = getopt(argc, argv, "n:")) != -1){ 
            switch (opt)
            {
            case 'n': // grade of polynomial
                grade = atoi(optarg);
                break;
            default:
                fprintf(stderr, "Usage: %s -n <grade of polynomial>\n", argv[0]);
                return 1;
                break;
            }
            
        }

        if(grade < 0){
            fprintf(stderr, "Usage: %s -n <grade of polynomial>\n", argv[0]);
            return 1;
        }

        
        
        // Creating polynomials
        long long int* arr = calloc((grade+1), sizeof(long long int)); 
        long long int* arr2 = calloc((grade+1), sizeof(long long int));

        for (int i = 0; i < grade + 1; i++){
            arr[i] = RandomNumber();
            arr2[i] = RandomNumber();
        }

        // C = calloc((grade*2 + 1), sizeof(long long int));
        // if (serial_flag == 1)
        //     SerialMult(arr, arr2, C, grade);



        total_start = MPI_Wtime();  // Αρχή συνολικού χρόνου
        
        t1 = MPI_Wtime();
        MPI_Bcast(&grade, 1, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Bcast(arr, grade + 1, MPI_LONG_LONG_INT, 0, MPI_COMM_WORLD);
        MPI_Bcast(arr2, grade + 1, MPI_LONG_LONG_INT, 0, MPI_COMM_WORLD);
        t2 = MPI_Wtime();
        broadcast_time = t2 - t1;
        
        // Master process: do its share of work
        C2 = calloc((grade * 2 + 1), sizeof(long long int));
        int result_size = 2 * grade + 1;
        int chunk_size = result_size / comm_size;
        
        int start = my_rank * chunk_size;
        int n = (my_rank == comm_size - 1) ? (result_size - start) : chunk_size;
        double compute_start = MPI_Wtime();
        ThreadMult(arr, arr2, C2, start, grade + 1, n);
        local_compute_time = MPI_Wtime() - compute_start;

    }else if (my_rank != 0) {
        // Worker processes: allocate buffer to receive data
        MPI_Bcast(&grade, 1, MPI_INT, 0, MPI_COMM_WORLD);
        fflush(stdout);
        long long int* arr = calloc((grade+1), sizeof(long long int));
        long long int* arr2 = calloc((grade+1), sizeof(long long int));
        MPI_Bcast(arr, grade + 1, MPI_LONG_LONG_INT, 0, MPI_COMM_WORLD);
        MPI_Bcast(arr2, grade + 1, MPI_LONG_LONG_INT, 0, MPI_COMM_WORLD);

        // int grade;
        C2 = calloc((grade*2 + 1), sizeof(long long int));

        // Calculate work distribution
        int result_size = 2 * grade + 1;
        int chunk_size = result_size / comm_size;
        
        int start = my_rank * chunk_size;
        int n = (my_rank == comm_size - 1) ? (result_size - start) : chunk_size;
        double compute_start = MPI_Wtime();
        ThreadMult(arr, arr2, C2, start, grade + 1, n);
        local_compute_time = MPI_Wtime() - compute_start;
        free(arr);
        free(arr2);
    }

    // Gather results from all processes
    double t3, t4;
    t3 = MPI_Wtime();
    MPI_Reduce(&local_compute_time, &max_compute_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    
    MPI_Reduce(my_rank == 0 ? MPI_IN_PLACE : C2, C2, (grade * 2 + 1), MPI_LONG_LONG_INT, MPI_SUM, 0, MPI_COMM_WORLD);
    t4 = MPI_Wtime();
    reduce_time = t4 - t3;
    total_end = t4;
    
    // double reduce_time = 
    // printf("%lf,", t2 - t1); // print time for parallel multiplication
    // printf("%lf\n", t4 - t3); // print time for reduce
    MPI_Finalize();

    if (my_rank != 0) {
        free(C2);
        return 0;
    }

    printf("Broadcast time: %lf\n", broadcast_time);
    printf("Compute time: %lf\n", max_compute_time);
    printf("Reduce time: %lf\n", reduce_time);
    printf("Total time: %lf\n", total_end - total_start);
    // if (serial_flag == 1) {
    //     int ret =  memcmp(C, C2, (grade * 2 + 1) * sizeof(long long int));
    //     ret == 0 ? printf("%d\n", 1) : printf("%d\n", 0);
    // }

    // free(arr);
    // free(arr2);
    // free(C);
    free(C2);
    return 0;
}