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


void convert_to_sparse(int *array, long long size, SparseMatrix sparse_matrix) {
    // First pass: count non-zeros
    long long non_zero_count = 0;
    for (long long i = 0; i < size * size; i++) {
        if (array[i] != 0) {
            non_zero_count++;
        }
    }
    
    sparse_matrix->size = size;
    sparse_matrix->non_zero_v = non_zero_count;
    sparse_matrix->V = malloc(non_zero_count * sizeof(int));
    sparse_matrix->col_index = malloc(non_zero_count * sizeof(int));
    sparse_matrix->row_index = malloc((size + 1) * sizeof(int));
    
    long long count = 0;
    sparse_matrix->row_index[0] = 0;
    for (long long i = 0; i < size; i++){
        for (long long j = 0; j < size; j++){
            if (array[i*size + j]){
                sparse_matrix->V[count] = array[i*size + j];
                sparse_matrix->col_index[count] = j;
                count++;
            }
        }
        sparse_matrix->row_index[i + 1] = count;
    }
    return;
}


void create_dense_matrix(int *array, long long size, int non_zero_percentage) {
    long long total_elements = size * size;
    long long num_non_zeros = (total_elements * non_zero_percentage) / 100;
    long long added = 0;
    
    while (added < num_non_zeros) {
        long long i = rand() % size;
        long long j = rand() % size;
        
        if (array[i * size + j] == 0) {
            array[i * size + j] = rand() % 10 + 1;
            added++;
        }
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
    srand(63);
    MPI_Init(&argc, &argv);
    int comm_size;
    int my_rank;
    MPI_Comm_size(MPI_COMM_WORLD, &comm_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    
    int opt;
    long long size = -1;
    int non_zero_percentage = -1;
    int multiplications = -1;
    
    int *Array = NULL;
    long long *vector = NULL;
    long long *result_vector = NULL;
    long long *original_vector = NULL;
    SparseMatrix sparse_matrix = NULL;
    
    // Timing variables
    double csr_construction_time = 0.0;
    double broadcast_time_csr = 0.0;
    double compute_time_csr = 0.0;
    double total_time_csr = 0.0;
    double total_time_dense = 0.0;
    double start_time, end_time;
    double compute_time_dense = 0.0;
    double total_start_time_csr, total_start_time_dense;
    
    if (my_rank == 0) {
        while((opt = getopt(argc, argv, "s:z:m:")) != -1) {
            switch(opt) {
                case 's':
                    size = atoll(optarg);
                    break;
                case 'z':
                    non_zero_percentage = 100 - atoi(optarg);
                    break;
                case 'm':
                    multiplications = atoi(optarg);
                    break;
                default:
                    fprintf(stderr, "Usage: %s -s size -z percentage_zeros -m multiplications\n", argv[0]);
                    MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
            }
        }
        if (size <= 0 || non_zero_percentage < 0 || multiplications <= 0) {
            fprintf(stderr, "All parameters must be positive integers.\n");
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }
        
        Array = calloc(size * size, sizeof(int));
        create_dense_matrix(Array, size, non_zero_percentage);

        result_vector = calloc(size, sizeof(long long));
        vector = calloc(size, sizeof(long long));
        original_vector = calloc(size, sizeof(long long));
        for (long long i = 0; i < size; i++){
            vector[i] = rand() % 10;
            original_vector[i] = vector[i];
        }
        
        // ========== CSR MODE ==========
        total_start_time_csr = MPI_Wtime();
        
        // CSR construction time
        start_time = MPI_Wtime();
        sparse_matrix = malloc(sizeof(struct sparse_matrix));
        convert_to_sparse(Array, size, sparse_matrix);
        end_time = MPI_Wtime();
        csr_construction_time = end_time - start_time;
        
        // Broadcast time for CSR
        start_time = MPI_Wtime();
        MPI_Bcast(&size, 1, MPI_LONG_LONG_INT, 0, MPI_COMM_WORLD);
        MPI_Bcast(&multiplications, 1, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Bcast(&sparse_matrix->non_zero_v, 1, MPI_LONG_LONG_INT, 0, MPI_COMM_WORLD);
        MPI_Bcast(sparse_matrix->V, sparse_matrix->non_zero_v, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Bcast(sparse_matrix->col_index, sparse_matrix->non_zero_v, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Bcast(sparse_matrix->row_index, size + 1, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Bcast(Array, size*size, MPI_INT, 0, MPI_COMM_WORLD);
        end_time = MPI_Wtime();
        broadcast_time_csr = end_time - start_time;
        
        // Restore original vector for CSR computation
        memcpy(vector, original_vector, size * sizeof(long long));
        
        // Compute time for CSR parallel
        start_time = MPI_Wtime();
        long long* reduce_buffer = calloc(size, sizeof(long long));
        long long* final_result_csr = calloc(size, sizeof(long long));
        for (int i = 0; i < multiplications; i++) {
            memset(result_vector, 0, sizeof(long long) * size);
            MPI_Bcast(vector, size, MPI_LONG_LONG_INT, 0, MPI_COMM_WORLD);
            parallelMultSparseMatrixWithVector(vector, sparse_matrix, result_vector, comm_size, my_rank);
            memset(reduce_buffer, 0, sizeof(long long) * size);
            MPI_Reduce(result_vector, reduce_buffer, size, MPI_LONG_LONG_INT, MPI_SUM, 0, MPI_COMM_WORLD);
            memcpy(vector, reduce_buffer, size * sizeof(long long));
        }
        memcpy(final_result_csr, vector, size * sizeof(long long));
        end_time = MPI_Wtime();
        compute_time_csr = end_time - start_time;
        
        total_time_csr = MPI_Wtime() - total_start_time_csr;
        free(reduce_buffer);
        
        // ========== DENSE MODE ==========
        total_start_time_dense = MPI_Wtime();
        
        // Signal workers to start dense mode
        // int dense_signal = 1;
        // MPI_Bcast(&dense_signal, 1, MPI_INT, 0, MPI_COMM_WORLD);
        
        // Restore original vector for Dense computation
        memcpy(vector, original_vector, size * sizeof(long long));
        
        // Compute time for Dense parallel
        start_time = MPI_Wtime();
        long long* reduce_buffer_dense = calloc(size, sizeof(long long));
        long long* final_result_dense = calloc(size, sizeof(long long));
        for (int i = 0; i < multiplications; i++) {
            memset(result_vector, 0, sizeof(long long) * size);
            MPI_Bcast(vector, size, MPI_LONG_LONG_INT, 0, MPI_COMM_WORLD);
            parallelMultDenseMatrixWithVector(Array, size, vector, result_vector, comm_size, my_rank);
            memset(reduce_buffer_dense, 0, sizeof(long long) * size);
            MPI_Reduce(result_vector, reduce_buffer_dense, size, MPI_LONG_LONG_INT, MPI_SUM, 0, MPI_COMM_WORLD);
            memcpy(vector, reduce_buffer_dense, size * sizeof(long long));
        }
        memcpy(final_result_dense, vector, size * sizeof(long long));
        end_time = MPI_Wtime();
        compute_time_dense = end_time - start_time;
        
        total_time_dense = MPI_Wtime() - total_start_time_dense;
        free(reduce_buffer_dense);
        
        // Print timing results
        printf("=== Timing Results ===\n");
        printf("CSR construction time: %f\n", csr_construction_time);
        printf("Broadcast time (CSR): %f\n", broadcast_time_csr);
        printf("Compute time (CSR): %f\n", compute_time_csr);
        printf("Total time (CSR): %f\n", total_time_csr);
        printf("Compute time (Dense): %f\n", compute_time_dense);
        printf("Total time (Dense): %f\n", total_time_dense);
        
        // Print result vectors
        printf("\n=== Result Vectors ===\n");
        printf("CSR Result Vector (first 10 elements):\n");
        for (long long i = 0; i < (size < 10 ? size : 10); i++) {
            printf("%lld ", final_result_csr[i]);
        }
        printf("\n\nDense Result Vector (first 10 elements):\n");
        for (long long i = 0; i < (size < 10 ? size : 10); i++) {
            printf("%lld ", final_result_dense[i]);
        }
        printf("\n\n");
        
        // Verify results match
        int results_match = 1;
        for (long long i = 0; i < size; i++) {
            if (final_result_csr[i] != final_result_dense[i]) {
                results_match = 0;
                break;
            }
        }
        printf("Results match: %s\n", results_match ? "YES" : "NO");
        
        // Cleanup
        free(final_result_csr);
        free(final_result_dense);
        free(Array);
        free(vector);
        free(result_vector);
        free(original_vector);
        free(sparse_matrix->V);
        free(sparse_matrix->col_index);
        free(sparse_matrix->row_index);
        free(sparse_matrix);
        
    } else {
        // Worker processes
        
        // Receive broadcast data
        MPI_Bcast(&size, 1, MPI_LONG_LONG_INT, 0, MPI_COMM_WORLD);
        MPI_Bcast(&multiplications, 1, MPI_INT, 0, MPI_COMM_WORLD);
        
        sparse_matrix = malloc(sizeof(struct sparse_matrix));
        sparse_matrix->size = size;
        
        MPI_Bcast(&sparse_matrix->non_zero_v, 1, MPI_LONG_LONG_INT, 0, MPI_COMM_WORLD);
        
        sparse_matrix->V = malloc(sparse_matrix->non_zero_v * sizeof(int));
        sparse_matrix->col_index = malloc(sparse_matrix->non_zero_v * sizeof(int));
        sparse_matrix->row_index = malloc((size + 1) * sizeof(int));
        
        MPI_Bcast(sparse_matrix->V, sparse_matrix->non_zero_v, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Bcast(sparse_matrix->col_index, sparse_matrix->non_zero_v, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Bcast(sparse_matrix->row_index, size + 1, MPI_INT, 0, MPI_COMM_WORLD);
        
        Array = calloc(size * size, sizeof(int));
        MPI_Bcast(Array, size*size, MPI_INT, 0, MPI_COMM_WORLD);
        
        vector = calloc(size, sizeof(long long));
        result_vector = calloc(size, sizeof(long long));
        
        // CSR parallel computation
        for(int i = 0; i < multiplications; i++) {
            memset(result_vector, 0, sizeof(long long) * size);
            MPI_Bcast(vector, size, MPI_LONG_LONG_INT, 0, MPI_COMM_WORLD);
            parallelMultSparseMatrixWithVector(vector, sparse_matrix, result_vector, comm_size, my_rank);
            MPI_Reduce(result_vector, NULL, size, MPI_LONG_LONG_INT, MPI_SUM, 0, MPI_COMM_WORLD);
        }
        
        // Receive dense signal
        // int dense_signal;
        // MPI_Bcast(&dense_signal, 1, MPI_INT, 0, MPI_COMM_WORLD);
        
        // Dense parallel computation
        for(int i = 0; i < multiplications; i++) {
            memset(result_vector, 0, sizeof(long long) * size);
            MPI_Bcast(vector, size, MPI_LONG_LONG_INT, 0, MPI_COMM_WORLD);
            parallelMultDenseMatrixWithVector(Array, size, vector, result_vector, comm_size, my_rank);
            MPI_Reduce(result_vector, NULL, size, MPI_LONG_LONG_INT, MPI_SUM, 0, MPI_COMM_WORLD);
        }

        // Cleanup
        free(Array);
        free(vector);
        free(result_vector);
        free(sparse_matrix->V);
        free(sparse_matrix->col_index);
        free(sparse_matrix->row_index);
        free(sparse_matrix);
    }
    
    MPI_Finalize();
    return 0;
}