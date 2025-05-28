#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>

#define TO_MICROSECONDS(x) (x * (unsigned long)1e6)
#define MAX_BUFFER_SIZE 10000
#define PRINT_UNDERLINE(TEXT , ...) ({printf("\033[4m" TEXT "\033[0m", ##__VA_ARGS__);})


typedef enum
{
    THREAD_PER_MATRIX,
    THREAD_PER_ROW,
    THREAD_PER_ELEMENT
}MultiplicationMethod;

typedef struct
{
    int row_size;
    int col_size;

}Dimensions;

typedef struct
{
    int** data;
    Dimensions dim;

}Matrix;

typedef struct
{
    Matrix* matrix1;
    Matrix* matrix2;
    Matrix* output_matrix;

}ThreadPerMatrixArgs;

typedef struct
{
    int* output_row_data;
    int* row_data;
    Matrix* matrix;

}ThreadPerRowArgs;

typedef struct
{
    int* output_row_element;
    int* row_data1;
    int col;
    Matrix* matrix;

}ThreadPerElementArgs;


int* read_matrix_row(FILE* file , const int col_size)
{
    char buffer[MAX_BUFFER_SIZE];

    // Allocate row array with the size of given columns
    int* row_data = (int*) calloc(col_size, sizeof(int));

    fgets(buffer , MAX_BUFFER_SIZE , file);

    //Extract the first row element
    char* row_element_string = strtok(buffer , "\t");

    //Extract each row element
    for(int i = 0 ; i<col_size && row_element_string!=NULL ; ++i)
    {
        row_data[i] = atoi(row_element_string);
        row_element_string = strtok(NULL , "\t");
    }

    return row_data;
}

Dimensions read_matrix_dimensions(FILE* file)
{
    Dimensions dim;
    char buffer[MAX_BUFFER_SIZE];
    fgets(buffer , MAX_BUFFER_SIZE , file);


    // extracts "row=$row" & "col=$col"
    char* row_assignment = strtok(buffer , " ");
    char* column_assignment = strtok(NULL , " ");

    //Skips "row=" to "$row" part
    strtok(row_assignment , "=");
    char* row_string_val = strtok(NULL , " ");

    //extracts row int value
    dim.row_size = atoi(row_string_val);


    //Skips "col=" to "$col" part
    strtok(column_assignment , "=");
    char* column_string_val = strtok(NULL , " ");

    // extracts col int value
    dim.col_size = atoi(column_string_val);


    return dim;
}



Matrix* read_matrix_file(char* file_prefix)
{
    char file_name[MAX_BUFFER_SIZE];
    strcpy(file_name , file_prefix );
    strcat(file_name , ".txt");


    FILE* file = fopen(file_name , "r");



    // Handle non-existing file
    if(file == NULL)
    {
        printf("Error opening file %s\n" , file_name);
        exit(1);
    }

    // Allocate memory for new matrix
    Matrix* matrix = malloc(sizeof(Matrix));

    //Reads matrix dimensions
    matrix->dim = read_matrix_dimensions(file);


    // Allocate memory for Matrix data
    int** matrix_data = (int**) malloc(sizeof(int*) * matrix->dim.row_size);

    //Read row elements
    for(int i = 0 ; i<matrix->dim.row_size ; ++i)
    {
        matrix_data[i] = read_matrix_row(file , matrix->dim.col_size);
    }

    //Assign matrix data to our matrix struct
    matrix->data = matrix_data;

    fclose(file);

    return matrix;
}

void write_output_matrix(Matrix* matrix , char* file_name)
{
    FILE* file = fopen(file_name , "w");

    if(file == NULL )
    {
        printf("Write Operation Failed");
        return;
    }

    // Writes dimensions in the  file first line
    fprintf(file , "row=%d col=%d\n" , matrix->dim.row_size , matrix->dim.col_size);

    /*Writes the matrix data*/
    for(int i = 0 ; i<matrix->dim.row_size ; ++i)
    {
        /*Writes the matrix row data , each seperated with \t */
        for(int j = 0 ; j<matrix->dim.col_size-1 ; ++j)
        {
            fprintf(file , "%d\t" , matrix->data[i][j]);
        }

        // Writes the last element in the row followed by new line
        fprintf(file,"%d\n" , matrix->data[i][matrix->dim.col_size-1]);
    }
    fclose(file);
}

void write_output_matrix_wrapper(Matrix* matrix , char* file_name , const char* suffix)
{
    char full_file_name[MAX_BUFFER_SIZE];
    strcpy(full_file_name, file_name);
    strcat(full_file_name, suffix);

    //Writes the output matrix in the given file
    write_output_matrix(matrix , full_file_name);
}

void* thread_per_matrix_multiplication(void* args)
{
    ThreadPerMatrixArgs* thread_per_matrix_args = (ThreadPerMatrixArgs*) args;

    //Extract the operation arguments
    Matrix* matrix1 = thread_per_matrix_args->matrix1;
    Matrix* matrix2 = thread_per_matrix_args->matrix2;
    Matrix* output_matrix = thread_per_matrix_args->output_matrix;

    for(int i = 0 ; i< matrix1->dim.row_size ; ++i)
    {
        for(int j = 0 ; j< matrix2->dim.col_size ; ++j)
        {
            for(int k = 0 ; k< matrix1->dim.col_size ; ++k)
            {
                output_matrix->data[i][j] += matrix1->data[i][k] * matrix2->data[k][j];
            }
        }
    }
    return NULL;
}

void thread_per_matrix_helper(Matrix* output_matrix , Matrix* matrix1 , Matrix* matrix2)
{
    pthread_t thread;

    /* Set up thread operation args struct*/
    ThreadPerMatrixArgs thread_per_matrix_args;
    thread_per_matrix_args.matrix1 = matrix1;
    thread_per_matrix_args.matrix2 = matrix2;
    thread_per_matrix_args.output_matrix = output_matrix;

    pthread_create(&thread , NULL , thread_per_matrix_multiplication , (void*)&thread_per_matrix_args);
    pthread_join(thread , NULL);
}


void* thread_per_row_multiplication(void* args)
{
    ThreadPerRowArgs* thread_per_row_args = (ThreadPerRowArgs*) args;

    /*Extract the operation arguments*/
    Matrix* matrix     = thread_per_row_args->matrix;
    int* row_data       = thread_per_row_args->row_data;
    int* output_row_data = thread_per_row_args->output_row_data;

    for(int i = 0 ; i<matrix->dim.col_size ; ++i)
    {
        for(int j = 0 ; j<matrix->dim.row_size ; ++j)
        {
            output_row_data[i] += row_data[j]* matrix->data[j][i];
        }
    }
    return NULL;
}


void thread_per_row_helper(Matrix* output_matrix , Matrix* matrix1 , Matrix* matrix2)
{
    const int rows = output_matrix->dim.row_size;

    pthread_t threads[rows];

    // Thread operations args for each thread through out its operation
    ThreadPerRowArgs thread_operation_args[rows];

    for(int i = 0 ; i<rows ; ++i)
    {
        /* Sets current thread args*/
        thread_operation_args[i].matrix = matrix2;
        thread_operation_args[i].row_data = matrix1->data[i];
        thread_operation_args[i].output_row_data = output_matrix->data[i];


        pthread_create( &threads[i],
                        NULL,
                        thread_per_row_multiplication,
                        (void*)&thread_operation_args[i]
        );
    }

    /*Waits for working threads to finish*/
    for(int j = 0 ; j<rows ; ++j)
    {
        pthread_join(threads[j] , NULL);
    }
}

void* thread_per_element_multiplication(void* args)
{
    ThreadPerElementArgs* thread_per_element_args = (ThreadPerElementArgs*) args;

    //Extract the operation arguments
    Matrix* matrix        = thread_per_element_args->matrix;
    int* row_data          = thread_per_element_args->row_data1;
    int* output_row_element = thread_per_element_args->output_row_element;
    int col               = thread_per_element_args->col;

    for(int i = 0 ; i <matrix->dim.row_size ;++i )
    {
        *output_row_element+= row_data[i] * matrix->data[i][col];
    }
    return NULL;
}

void thread_per_element_helper(Matrix* output_matrix , Matrix* matrix1 , Matrix* matrix2)
{
    const int rows = output_matrix->dim.row_size;
    const int cols = output_matrix->dim.col_size;
    const int threads_count = rows * cols;

    pthread_t threads[threads_count];

    // Thread operations args for each thread through out its operation
    ThreadPerElementArgs thread_operation_args[threads_count];


    for(int i = 0 ; i<rows ; ++i)
    {


        for(int j = 0 ; j<cols ; ++j)
        {
            const int current_thread = i*cols+j;

            /* Sets current thread args*/
            thread_operation_args[current_thread].matrix= matrix2;
            thread_operation_args[current_thread].col = j;
            thread_operation_args[current_thread].output_row_element = output_matrix->data[i] + j;
            thread_operation_args[current_thread].row_data1 = matrix1->data[i];

            pthread_create(&threads[current_thread],
                            NULL,
                            thread_per_element_multiplication,
                            (void*)&thread_operation_args[current_thread]
            );
        }
    }

    /* Waits for working threads after the end of the loop */
    for(int k = 0 ; k<threads_count ; ++k)
    {
        pthread_join(threads[k] ,NULL);
    }
}

Matrix* create_output_matrix(Dimensions dim1 , Dimensions dim2 )
{
    //Initilaize the output matrix dimensions
    Dimensions output_dim;
    output_dim.row_size = dim1.row_size;
    output_dim.col_size = dim2.col_size;

    //Allocate memory for output matrix
    Matrix* output_matrix = malloc(sizeof(Matrix));

    //Assign output matrix dimensions
    output_matrix->dim = output_dim;

    //Create the matrix data
    output_matrix->data = (int**) malloc(sizeof(int*) * output_matrix->dim.row_size);
    for(int i = 0 ; i< output_matrix->dim.row_size ; ++i)
    {
        output_matrix->data[i] = (int*) calloc(output_matrix->dim.col_size , sizeof(int));
    }

    return output_matrix;
}

void free_matrix(Matrix* matrix)
{
    for(int i = 0 ; i<matrix->dim.row_size ; ++i)
    {
        free(matrix->data[i]);
    }

    free(matrix->data);
    free(matrix);
}


void run_test(Matrix* output_matrix , Matrix* matrix1 , Matrix* matrix2 , const char* multiplication_method_name , MultiplicationMethod method)
{
    struct timeval start, stop;
    void (*helper_function)(Matrix*, Matrix*, Matrix*);
    int threads_used = 0;

    switch (method) {
        case THREAD_PER_MATRIX:
            helper_function = thread_per_matrix_helper;
            threads_used = 1;
            break;
        case THREAD_PER_ROW:
            helper_function = thread_per_row_helper;
            threads_used = matrix1->dim.row_size;
            break;
        case THREAD_PER_ELEMENT:
            helper_function = thread_per_element_helper;
            threads_used = matrix1->dim.row_size * matrix2->dim.col_size;
            break;
        default:
            printf("Invalid multiplication method\n");
            return;
    }

    gettimeofday(&start, NULL);
    helper_function(output_matrix, matrix1, matrix2);
    gettimeofday(&stop, NULL);

    unsigned long elapsed_time = TO_MICROSECONDS((stop.tv_sec - start.tv_sec)) + (stop.tv_usec - start.tv_usec);

    PRINT_UNDERLINE("Matrix Multiplication : Thread %s\n\n", multiplication_method_name);
    printf("Time : %lu us\n" , elapsed_time);
    printf("Threads Created : %d threads\n\n", threads_used);
}

int main(int argc , char* argv[])
{
    static char* files_prefixes[] = {"a" , "b", "c"};
    static const char* multiplication_methods[] = {"per matrix", "per row", "per element"};

    /*Sets the files prefixes with the passed arguments*/
    for(int i = 1; i < argc; i++)
    {
        files_prefixes[i-1] = argv[i];
    }

    /* Read Both Input Arrays from their respective files*/
    Matrix* matrix1 = read_matrix_file(files_prefixes[0]);
    Matrix* matrix2 = read_matrix_file(files_prefixes[1]);

    /* Validates ability to proceed with multiplication */
    if(matrix1->dim.col_size != matrix2->dim.row_size)
    {
        printf("Error Matrix Multiplication Failed : AxB (cols A != rows B)");
        free_matrix(matrix1);
        free_matrix(matrix2);
        return 0;
    }


    /*Init output matrices for each multiplication method*/
    Matrix* output_matrix_thread_per_matrix = create_output_matrix(matrix1->dim , matrix2->dim);
    Matrix* output_matrix_thread_per_row = create_output_matrix(matrix1->dim , matrix2->dim);
    Matrix* output_matrix_thread_per_element = create_output_matrix(matrix1->dim , matrix2->dim);


    /*Single Thread Matrix multiplication*/
    run_test(output_matrix_thread_per_matrix , matrix1 , matrix2 , multiplication_methods[THREAD_PER_MATRIX] , THREAD_PER_MATRIX);
    /*--------------------------------------------------------------------------------------*/

    /*Thread per row Matrix multiplication*/
    run_test(output_matrix_thread_per_row , matrix1 , matrix2 , multiplication_methods[THREAD_PER_ROW] , THREAD_PER_ROW);
    /*--------------------------------------------------------------------------------------*/

    /*Thread per element Matrix multiplication*/
    run_test(output_matrix_thread_per_element , matrix1 , matrix2 , multiplication_methods[THREAD_PER_ELEMENT] , THREAD_PER_ELEMENT);
    /*--------------------------------------------------------------------------------------*/

    /* Set output files names*/
    write_output_matrix_wrapper(output_matrix_thread_per_matrix , files_prefixes[2] , "_per_matrix.txt");
    write_output_matrix_wrapper(output_matrix_thread_per_row , files_prefixes[2] , "_per_row.txt");
    write_output_matrix_wrapper(output_matrix_thread_per_element , files_prefixes[2] , "_per_element.txt");

    /*Frees used data*/
    free_matrix(matrix1);
    free_matrix(matrix2);
    free_matrix(output_matrix_thread_per_element);
    free_matrix(output_matrix_thread_per_row);
    free_matrix(output_matrix_thread_per_matrix);

    return 0;
}
