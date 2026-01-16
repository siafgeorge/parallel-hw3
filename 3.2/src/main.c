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


typedef struct sparse_matrix {
    int *V;
    int *col_index;
    int *row_index;
    long long non_zero_v;
    long long size; 
}* SparseMatrix;


void convert_to_sparse(int *array, long long size, long long non_zero_v, SparseMatrix sparse_matrix) {
    sparse_matrix->size = size;
    sparse_matrix->non_zero_v = non_zero_v;
    sparse_matrix->V = malloc(non_zero_v * sizeof(int));
    sparse_matrix->col_index = malloc(non_zero_v * sizeof(int));
    sparse_matrix->row_index = malloc((size + 1) * sizeof(int));
    long long count = 0;
    sparse_matrix->row_index[0] = 0;
    for (long long i = 0; i < size; i++){
        for (long long j = 0; j < size; j++){
            if (array[i*size + j]){
                sparse_matrix->V[count] = array[i*size + j];
                sparse_matrix->col_index[count] = j;
                sparse_matrix->row_index[i + 1]++;
                count++;
            }
        }
        sparse_matrix->row_index[i + 1] += sparse_matrix->row_index[i];
    }
    return;
}


void addElement(int *array, long long size) {
    int i = rand() % size;
    int j = rand() % size;
    int value = rand() % 10 + 1; // avoiding value 0 by adding 1
    while(array[i * size + j]){
        i = (i+1) % size;
        if (!i)
            j = (j+1) % size;
    }
    array[i * size + j] = value;
    
    return;
}

void create_dense_matrix(int *array, long long size, int zeros) {
    int num_zeros = ((size * size) * zeros) / 100; // size^2 * zeros%
    for (int i = 0; i < num_zeros; i++) {
        addElement(array, size);
    }

    return;
}

void parallelMultSparseMatrixWithVector(long long *vector, SparseMatrix sparse_matrix, long long *result_vector, int comm_size, int my_rank) {
    long long rows_per_process = sparse_matrix->size / comm_size;
    long long start_row = my_rank * rows_per_process;
    long long end_row = (my_rank == comm_size - 1) ? sparse_matrix->size : start_row + rows_per_process;
    
    for (long long i = start_row; i < end_row; i++) {
        for (long long k = sparse_matrix->row_index[i]; k < sparse_matrix->row_index[i + 1]; k++) {
            result_vector[i] += sparse_matrix->V[k] * vector[sparse_matrix->col_index[k]];
        }
    }
    
    return;
}

void serialMultSparseMatrixWithVector(long long *vector, SparseMatrix sparse_matrix, long long *result_vector) {
    for (int i = 0; i < sparse_matrix->size; i++) {
        for (int k = sparse_matrix->row_index[i]; k < sparse_matrix->row_index[i + 1]; k++) {
            result_vector[i] += sparse_matrix->V[k] * vector[sparse_matrix->col_index[k]];
        }
    }
    
    return;
}

void serialMultDenseMatrixWithVector(int *dense_matrix, long long size, long long *vector, long long *result_vector) {
    for (long long i = 0; i < size; i++) {
        result_vector[i] = 0;
    }
    for (long long i = 0; i < size; i++) {
        for (long long j = 0; j < size; j++) {
            result_vector[i] += dense_matrix[i*size + j] * vector[j];
        }
    }
    return;
}

void parallelMultDenseMatrixWithVector(int *dense_matrix, long long size, long long *vector, long long *result_vector, int comm_size, int my_rank) {
    long long rows_per_process = size / comm_size;
    long long start_row = my_rank * rows_per_process;
    long long end_row = (my_rank == comm_size - 1) ? size : start_row + rows_per_process;
    
    for (long long i = start_row; i < end_row; i++) {
        result_vector[i] = 0;
        for (long long j = 0; j < size; j++) {
            result_vector[i] += dense_matrix[i * size + j] * vector[j];
        }
    }
    
    return;
}

int main(int argc, char *argv[]) {
    srand(63); // same seed for reproducibility
    MPI_Init(&argc, &argv);
    int comm_size;
    int my_rank;
    MPI_Comm_size(MPI_COMM_WORLD, &comm_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    
    int opt;
    long long size = -1;
    int zeros = -1;
    int multiplications = -1;
    int dense_mode = 0;
    
    int *Array = NULL;
    long long *vector;
    long long *result_vector;
    SparseMatrix sparse_matrix;
    if (my_rank == 0) {
        while((opt = getopt(argc, argv, "s:z:m:d")) != -1) {
            switch(opt) {
                case 's':
                    size = atoll(optarg);
                    break;
                case 'z':
                    zeros = atoi(optarg);
                    zeros = 100 - zeros; // converting to percentage of non-zeros                    
                    break;
                case 'm':
                    multiplications = atoi(optarg);
                    break;
                case 'd':
                    dense_mode = 1;
                    break;
                default:
                    fprintf(stderr, "Usage: %s -s size -z percentage zeros -m multiplications -t num_threads\n", argv[0]);
                    exit(EXIT_FAILURE);
            }
        }
        if (size <= 0 || zeros < 0 || multiplications <= 0) {
            fprintf(stderr, "All parameters must be positive integers.\n");
            exit(EXIT_FAILURE);
        }
        
        Array = calloc(size * size, sizeof(int));
        create_dense_matrix(Array, size, zeros);

        result_vector = calloc(size, sizeof(long long));
        vector = calloc(size, sizeof(long long));
        for (long long i = 0; i < size; i++){
            vector[i] = rand() % 10;
        }
        MPI_Bcast(&dense_mode, 1, MPI_INT, 0, MPI_COMM_WORLD);
        printf("size = %lld\n", size);
        if(dense_mode){
            printf("dense mode %d\n", my_rank);
            MPI_Bcast(&size, 1, MPI_LONG_LONG_INT, 0, MPI_COMM_WORLD);
            MPI_Bcast(Array, size*size, MPI_INT, 0, MPI_COMM_WORLD);
            MPI_Bcast(&multiplications, 1, MPI_LONG_LONG_INT, 0, MPI_COMM_WORLD);
            
            // Save original vector for comparison
            long long *original_vector = calloc(size, sizeof(long long));
            memcpy(original_vector, vector, size * sizeof(long long));
            
            long long * res1 = calloc(size, sizeof(long long));
            memcpy(res1, vector, size * sizeof(long long));
            // Run serial multiplication 'multiplications' times
            for (int i = 0; i < multiplications; i++) {
                memset(result_vector, 0, sizeof(long long) * size);
                serialMultDenseMatrixWithVector(Array, size, res1, result_vector);
                memcpy(res1, result_vector, size * sizeof(long long));
            }
    
            printf("result vector from serial multiplication:\n");
            for (int i = 0; i < size; i++) {
                printf("%lld ", result_vector[i]);
            }
            printf("\n");
            
            // Restore original vector for parallel computation
            memcpy(vector, original_vector, size * sizeof(long long));
            free(original_vector);
            
            long long* reduce_buffer = calloc(size, sizeof(long long));
            for (int i = 0; i < multiplications; i++) {
                memset(result_vector, 0, sizeof(long long) * size);
                MPI_Bcast(vector, size, MPI_LONG_LONG_INT, 0, MPI_COMM_WORLD);
                parallelMultDenseMatrixWithVector(Array, size, vector, result_vector, comm_size, my_rank);
                memset(reduce_buffer, 0, sizeof(long long) * size);
                MPI_Reduce(result_vector, reduce_buffer, size, MPI_LONG_LONG_INT, MPI_SUM, 0, MPI_COMM_WORLD);
                memcpy(vector, reduce_buffer, size * sizeof(long long));
            }
            memcpy(result_vector, vector, size * sizeof(long long));
            free(reduce_buffer);
            free(res1);

        }else{
            printf("sparse mode %d\n", my_rank);
            SparseMatrix sparse_matrix = malloc(sizeof(struct sparse_matrix));
            convert_to_sparse(Array, size, (size*size*zeros)/100, sparse_matrix);
            MPI_Bcast(&size, 1, MPI_LONG_LONG_INT, 0, MPI_COMM_WORLD);
            MPI_Bcast(Array, size*size, MPI_INT, 0, MPI_COMM_WORLD);
            MPI_Bcast(&multiplications, 1, MPI_LONG_LONG_INT, 0, MPI_COMM_WORLD);
            MPI_Bcast(&sparse_matrix->non_zero_v, 1, MPI_LONG_LONG_INT, 0, MPI_COMM_WORLD);
            MPI_Bcast(sparse_matrix->V, sparse_matrix->non_zero_v, MPI_INT, 0, MPI_COMM_WORLD);
            MPI_Bcast(sparse_matrix->col_index, sparse_matrix->non_zero_v, MPI_INT, 0, MPI_COMM_WORLD);
            MPI_Bcast(sparse_matrix->row_index, size + 1, MPI_INT, 0, MPI_COMM_WORLD);
            
            // Save original vector for comparison
            long long *original_vector = calloc(size, sizeof(long long));
            memcpy(original_vector, vector, size * sizeof(long long));
    
            long long int * res2 = calloc(size, sizeof(long long));
            memcpy(res2, vector, size * sizeof(long long));
            // Run serial multiplication 'multiplications' times
            for (int i = 0; i < multiplications; i++) {
                memset(result_vector, 0, sizeof(long long) * size);
                serialMultSparseMatrixWithVector(res2, sparse_matrix, result_vector);
                memcpy(res2, result_vector, size * sizeof(long long));
            }
    
            printf("result vector from serial multiplication:\n");
            for (int i = 0; i < size; i++) {
                printf("%lld ", result_vector[i]);
            }
            printf("\n");
            
            // Restore original vector for parallel computation
            memcpy(vector, original_vector, size * sizeof(long long));
            free(original_vector);
            
            long long* reduce_buffer = calloc(size, sizeof(long long));
            for (int i = 0; i < multiplications; i++) {
                memset(result_vector, 0, sizeof(long long) * size);
                MPI_Bcast(vector, size, MPI_LONG_LONG_INT, 0, MPI_COMM_WORLD);
                parallelMultSparseMatrixWithVector(vector, sparse_matrix, result_vector, comm_size, my_rank);
                memset(reduce_buffer, 0, sizeof(long long) * size);
                MPI_Reduce(result_vector, reduce_buffer, size, MPI_LONG_LONG_INT, MPI_SUM, 0, MPI_COMM_WORLD);
                memcpy(vector, reduce_buffer, size * sizeof(long long));
            }
            memcpy(result_vector, vector, size * sizeof(long long));
            free(reduce_buffer);
            free(res2);
        }
        
        
        // printf("result vector from parallel multiplication:\n");
        // for (int i = 0; i < size; i++) {
        //     printf("%lld ", result_vector[i]);
        // }
        // printf("\n");

    } else {
        size = 0;
        zeros = 0;
        multiplications = 0;
        dense_mode = 0;
        MPI_Bcast(&dense_mode, 1, MPI_INT, 0, MPI_COMM_WORLD);
        if (dense_mode) {
            printf("dense mode %d\n", my_rank);
            // Worker processes: initialize variables
            MPI_Bcast(&size, 1, MPI_LONG_LONG_INT, 0, MPI_COMM_WORLD);
            printf("size received: %lld\n", size);
            Array = calloc(size * size, sizeof(int));
            MPI_Bcast(Array, size*size, MPI_INT, 0, MPI_COMM_WORLD);
            printf("Array received:\n");
            for (long long i = 0; i < size; i++) {
                for (long long j = 0; j < size; j++) {
                    printf("%d ", Array[i * size + j]);
                }
                printf("\n");
            }
            MPI_Bcast(&multiplications, 1, MPI_LONG_LONG_INT, 0, MPI_COMM_WORLD);
            printf("multiplications received: %d\n", multiplications);
            vector = calloc(size, sizeof(long long));
            result_vector = calloc(size, sizeof(long long));
            for(int i=0; i < multiplications; i++) {
                
                memset(result_vector, 0, sizeof(long long) * size);
                
                MPI_Bcast(vector, size, MPI_LONG_LONG_INT, 0, MPI_COMM_WORLD);
                printf("vector received %d: my rank: %d", i, my_rank);
                for (int k = 0; k < size; k++) {
                    printf("%lld ", vector[k]);
                }
                printf("\n");
                parallelMultDenseMatrixWithVector(Array, size, vector, result_vector, comm_size, my_rank);
                MPI_Reduce(result_vector, NULL, size, MPI_LONG_LONG_INT, MPI_SUM, 0, MPI_COMM_WORLD);
            }
        }
        else {
            printf("sparse mode %d\n", my_rank);
            // Worker processes: initialize variables
            sparse_matrix = malloc(sizeof(struct sparse_matrix));
            MPI_Bcast(&size, 1, MPI_LONG_LONG_INT, 0, MPI_COMM_WORLD);
            sparse_matrix->size = size;
            Array = calloc(size * size, sizeof(int));
            MPI_Bcast(Array, size*size, MPI_INT, 0, MPI_COMM_WORLD);
            MPI_Bcast(&multiplications, 1, MPI_LONG_LONG_INT, 0, MPI_COMM_WORLD);  // Moved to match root order
    
            MPI_Bcast(&sparse_matrix->non_zero_v, 1, MPI_LONG_LONG_INT, 0, MPI_COMM_WORLD);
            
            sparse_matrix->col_index = malloc(sparse_matrix->non_zero_v * sizeof(int));
            sparse_matrix->row_index = malloc((size + 1) * sizeof(int));
            sparse_matrix->V = malloc(sparse_matrix->non_zero_v * sizeof(int));
    
            MPI_Bcast(sparse_matrix->V, sparse_matrix->non_zero_v, MPI_INT, 0, MPI_COMM_WORLD);
            MPI_Bcast(sparse_matrix->col_index, sparse_matrix->non_zero_v, MPI_INT, 0, MPI_COMM_WORLD);
            MPI_Bcast(sparse_matrix->row_index, size + 1, MPI_INT, 0, MPI_COMM_WORLD);
            // Removed duplicate MPI_Bcast(&multiplications, ...) from here
            printf("Non-zero values: ");
            for (long long i = 0; i < sparse_matrix->non_zero_v; i++) {
                printf("%d ", sparse_matrix->V[i]);
            }
            printf("\nColumn indices: ");
            for (long long i = 0; i < sparse_matrix->non_zero_v; i++) {
                printf("%d ", sparse_matrix->col_index[i]);
            }
            printf("\nRow indices: ");
            for (long long i = 0; i < size + 1; i++) {
                printf("%d ", sparse_matrix->row_index[i]);
            }
            printf("\n");
            vector = calloc(size, sizeof(long long));
            result_vector = calloc(size, sizeof(long long));
            for(int i=0; i < multiplications; i++) {
                memset(result_vector, 0, sizeof(long long) * size);
                MPI_Bcast(vector, size, MPI_LONG_LONG_INT, 0, MPI_COMM_WORLD);
                parallelMultSparseMatrixWithVector(vector, sparse_matrix, result_vector, comm_size, my_rank);
                MPI_Reduce(result_vector, NULL, size, MPI_LONG_LONG_INT, MPI_SUM, 0, MPI_COMM_WORLD);
            }
        }
    }

    // long long *reduce_buffer = NULL;
    // if (my_rank == 0) {
    //     reduce_buffer = calloc(size, sizeof(long long));
    //     MPI_Reduce(result_vector, reduce_buffer, size, MPI_LONG_LONG_INT, MPI_SUM, 0, MPI_COMM_WORLD);
    //     // Copy result back to result_vector
    //     memcpy(result_vector, reduce_buffer, size * sizeof(long long));
    //     free(reduce_buffer);
    // } else {
    //     MPI_Reduce(result_vector, NULL, size, MPI_LONG_LONG_INT, MPI_SUM, 0, MPI_COMM_WORLD);
    // }

    if (my_rank == 0) {
        printf("Final result vector after reduction:\n");
        for (int i = 0; i < size; i++) {
            printf("%lld ", result_vector[i]);
        }
        printf("\n");
    }

    MPI_Finalize();
    
    
    return 0;
}