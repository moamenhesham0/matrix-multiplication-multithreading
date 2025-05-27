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
}multiplication_method;

typedef struct
{
    int rowSize;
    int colSize;

}dimensions;

typedef struct
{
    int** data;
    dimensions dim;

}Matrix;

typedef struct
{
    Matrix* matrix1;
    Matrix* matrix2;
    Matrix* outputMatrix;

}thread_per_matrix_args;

typedef struct
{
    int*outputRowData;
    int* rowData;
    Matrix* matrix;

}thread_per_row_args;

typedef struct
{
    int* outputRowElement;
    int* rowData1;
    int col;
    Matrix* matrix;

}thread_per_element_args;


int* readMatrixRow(FILE* file , const int colSize)
{
    char buffer[MAX_BUFFER_SIZE];

    // Allocate row array with the size of given columns
    int* rowData = (int*) calloc(colSize, sizeof(int));

    fgets(buffer , MAX_BUFFER_SIZE , file);

    //Extract the first row element
    char* rowElementString = strtok(buffer , "\t");

    //Extract each row element
    for(int i = 0 ; i<colSize && rowElementString!=NULL ; ++i)
    {
        rowData[i] = atoi(rowElementString);
        rowElementString = strtok(NULL , "\t");
    }

    return rowData;
}

dimensions readMatrixDimensions(FILE* file)
{
    dimensions dim;
    char buffer[MAX_BUFFER_SIZE];
    fgets(buffer , MAX_BUFFER_SIZE , file);


    // extracts "row=$row" & "col=$col"
    char* rowAssignment = strtok(buffer , " ");
    char* columnAssignment = strtok(NULL , " ");

    //Skips "row=" to "$row" part
    strtok(rowAssignment , "=");
    char* rowStringVal = strtok(NULL , " ");

    //extracts row int value
    dim.rowSize = atoi(rowStringVal);


    //Skips "col=" to "$col" part
    strtok(columnAssignment , "=");
    char* columnStringVal = strtok(NULL , " ");

    // extracts col int value
    dim.colSize = atoi(columnStringVal);


    return dim;
}



Matrix* readMatrixFile(char* filePrefix)
{
    char fileName[MAX_BUFFER_SIZE];
    strcpy(fileName , filePrefix );
    strcat(fileName , ".txt");


    FILE* file = fopen(fileName , "r");



    // Handle non-existing file
    if(file == NULL)
    {
        printf("Error opening file %s\n" , fileName);
        exit(1);
    }

    // Allocate memory for new matrix
    Matrix* matrix = malloc(sizeof(Matrix));

    //Reads matrix dimensions
    matrix->dim = readMatrixDimensions(file);


    // Allocate memory for Matrix data
    int** matrixData = (int**) malloc(sizeof(int*) * matrix->dim.rowSize);

    //Read row elements
    for(int i = 0 ; i<matrix->dim.rowSize ; ++i)
    {
        matrixData[i] = readMatrixRow(file , matrix->dim.colSize);
    }

    //Assign matrix data to our matrix struct
    matrix->data = matrixData;

    fclose(file);

    return matrix;
}

void writeOutputMatrix(Matrix* matrix , char* fileName)
{
    FILE* file = fopen(fileName , "w");

    if(file == NULL )
    {
        printf("Write Operation Failed");
        return;
    }

    // Writes dimensions in the  file first line
    fprintf(file , "row=%d col=%d\n" , matrix->dim.rowSize , matrix->dim.colSize);

    /*Writes the matrix data*/
    for(int i = 0 ; i<matrix->dim.rowSize ; ++i)
    {
        /*Writes the matrix row data , each seperated with \t */
        for(int j = 0 ; j<matrix->dim.colSize-1 ; ++j)
        {
            fprintf(file , "%d\t" , matrix->data[i][j]);
        }

        // Writes the last element in the row followed by new line
        fprintf(file,"%d\n" , matrix->data[i][matrix->dim.colSize-1]);
    }
    fclose(file);
}

void writeOutputMatrixWrapper(Matrix* matrix , char* fileName , const char* suffix)
{
    char fullFileName[MAX_BUFFER_SIZE];
    strcpy(fullFileName, fileName);
    strcat(fullFileName, suffix);

    //Writes the output matrix in the given file
    writeOutputMatrix(matrix , fullFileName);
}

void* thread_per_matrix_multiplication(void* args)
{
    thread_per_matrix_args* threadPerMatrixArgs = (thread_per_matrix_args*) args;

    //Extract the operation arguments
    Matrix* matrix1 = threadPerMatrixArgs->matrix1;
    Matrix* matrix2 = threadPerMatrixArgs->matrix2;
    Matrix* outputMatrix = threadPerMatrixArgs->outputMatrix;

    for(int i = 0 ; i< matrix1->dim.rowSize ; ++i)
    {
        for(int j = 0 ; j< matrix2->dim.colSize ; ++j)
        {
            for(int k = 0 ; k< matrix1->dim.colSize ; ++k)
            {
                outputMatrix->data[i][j] += matrix1->data[i][k] * matrix2->data[k][j];
            }
        }
    }

}
void thread_per_matrix_helper(Matrix* outputMatrix , Matrix* matrix1 , Matrix* matrix2)
{
    pthread_t thread;

    /* Set up thread operation args struct*/
    thread_per_matrix_args threadPerMatrixArgs;
    threadPerMatrixArgs.matrix1 = matrix1;
    threadPerMatrixArgs.matrix2 = matrix2;
    threadPerMatrixArgs.outputMatrix = outputMatrix;

    pthread_create(&thread , NULL , thread_per_matrix_multiplication , (void*)&threadPerMatrixArgs);
    pthread_join(thread , NULL);
}


void* thread_per_row_multiplication(void* args)
{
    thread_per_row_args* threadPerRowArgs = (thread_per_row_args*) args;

    /*Extract the operation arguments*/
    Matrix* matrix     = threadPerRowArgs->matrix;
    int* rowData       = threadPerRowArgs->rowData;
    int* outputRowData = threadPerRowArgs->outputRowData;

    for(int i = 0 ; i<matrix->dim.colSize ; ++i)
    {
        for(int j = 0 ; j<matrix->dim.rowSize ; ++j)
        {
            outputRowData[i] += rowData[j]* matrix->data[j][i];
        }
    }
}


void thread_per_row_helper(Matrix* outputMatrix , Matrix* matrix1 , Matrix* matrix2)
{
    const int rows = outputMatrix->dim.rowSize;

    pthread_t threads[rows];

    // Thread operations args for each thread through out its operation
    thread_per_row_args thread_operation_args[rows];

    for(int i = 0 ; i<rows ; ++i)
    {
        /* Sets current thread args*/
        thread_operation_args[i].matrix = matrix2;
        thread_operation_args[i].rowData = matrix1->data[i];
        thread_operation_args[i].outputRowData = outputMatrix->data[i];


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
    thread_per_element_args* threadPerElementArgs = (thread_per_element_args*) args;

    //Extract the operation arguments
    Matrix* matrix        = threadPerElementArgs->matrix;
    int* rowData          = threadPerElementArgs->rowData1;
    int* outputRowElement = threadPerElementArgs->outputRowElement;
    int col               = threadPerElementArgs->col;

    for(int i = 0 ; i <matrix->dim.rowSize ;++i )
    {
        *outputRowElement+= rowData[i] * matrix->data[i][col];
    }
}

void thread_per_element_helper(Matrix* outputMatrix , Matrix* matrix1 , Matrix* matrix2)
{
    const int rows = outputMatrix->dim.rowSize;
    const int cols = outputMatrix->dim.colSize;
    const int threadsCount = rows * cols;

    pthread_t threads[threadsCount];

    // Thread operations args for each thread through out its operation
    thread_per_element_args thread_operation_args[threadsCount];


    for(int i = 0 ; i<rows ; ++i)
    {


        for(int j = 0 ; j<cols ; ++j)
        {
            const int currentThread = i*cols+j;

            /* Sets current thread args*/
            thread_operation_args[currentThread].matrix= matrix2;
            thread_operation_args[currentThread].col = j;
            thread_operation_args[currentThread].outputRowElement = outputMatrix->data[i] + j;
            thread_operation_args[currentThread].rowData1 = matrix1->data[i];

            pthread_create(&threads[currentThread],
                            NULL,
                            thread_per_element_multiplication,
                            (void*)&thread_operation_args[currentThread]
            );
        }
    }

    /* Waits for working threads after the end of the loop */
    for(int k = 0 ; k<threadsCount ; ++k)
    {
        pthread_join(threads[k] ,NULL);
    }
}

Matrix* create_output_matrix(dimensions dim1 , dimensions dim2 )
{
    //Initilaize the output matrix dimensions
    dimensions outputDim;
    outputDim.rowSize = dim1.rowSize;
    outputDim.colSize = dim2.colSize;

    //Allocate memory for output matrix
    Matrix* outputMatrix = malloc(sizeof(Matrix));

    //Assign output matrix dimensions
    outputMatrix->dim = outputDim;

    //Create the matrix data
    outputMatrix->data = (int**) malloc(sizeof(int*) * outputMatrix->dim.rowSize);
    for(int i = 0 ; i< outputMatrix->dim.rowSize ; ++i)
    {
        outputMatrix->data[i] = (int*) calloc(outputMatrix->dim.colSize , sizeof(int));
    }

    return outputMatrix;
}

void free_matrix(Matrix* matrix)
{
    for(int i = 0 ; i<matrix->dim.rowSize ; ++i)
    {
        free(matrix->data[i]);
    }

    free(matrix->data);
    free(matrix);
}


void run_test(Matrix* outputMatrix , Matrix* matrix1 , Matrix* matrix2 , const char* multiplicationMethod , multiplication_method method)
{
    struct timeval start, stop;
    void (*helperFunction)(Matrix*, Matrix*, Matrix*);
    int threads_used = 0;

    switch (method) {
        case THREAD_PER_MATRIX:
            helperFunction = thread_per_matrix_helper;
            threads_used = 1;
            break;
        case THREAD_PER_ROW:
            helperFunction = thread_per_row_helper;
            threads_used = matrix1->dim.rowSize;
            break;
        case THREAD_PER_ELEMENT:
            helperFunction = thread_per_element_helper;
            threads_used = matrix1->dim.rowSize * matrix2->dim.colSize;
            break;
        default:
            printf("Invalid multiplication method\n");
            return;
    }

    gettimeofday(&start, NULL);
    helperFunction(outputMatrix, matrix1, matrix2);
    gettimeofday(&stop, NULL);

    unsigned long elapsed_time = TO_MICROSECONDS((stop.tv_sec - start.tv_sec)) + (stop.tv_usec - start.tv_usec);

    PRINT_UNDERLINE("Matrix Multiplication : Thread %s\n\n", multiplicationMethod);
    printf("Time : %lu us\n" , elapsed_time);
    printf("Threads Created : %d threads\n\n", threads_used);
}

int main(int argc , char* argv[])
{
    static char* filesPrefixes[] = {"a" , "b", "c"};
    static char* multiplicationMethods[] = {"per matrix", "per row", "per element"};

    /*Sets the files prefixes with the passed arguments*/
    for(int i = 1; i < argc; i++)
    {
        filesPrefixes[i-1] = argv[i];
    }

    /* Read Both Input Arrays from their respective files*/
    Matrix* matrix1 = readMatrixFile(filesPrefixes[0]);
    Matrix* matrix2 = readMatrixFile(filesPrefixes[1]);

    /* Validates ability to proceed with multiplication */
    if(matrix1->dim.colSize != matrix2->dim.rowSize)
    {
        printf("Error Matrix Multiplication Failed : AxB (cols A != rows B)");
        free_matrix(matrix1);
        free_matrix(matrix2);
        return 0;
    }


    /*Init output matrices for each multiplication method*/
    Matrix* outputMatrix_thread_per_matrix = create_output_matrix(matrix1->dim , matrix2->dim);
    Matrix* outputMatrix_thread_per_row = create_output_matrix(matrix1->dim , matrix2->dim);
    Matrix* outputMatrix_thread_per_element = create_output_matrix(matrix1->dim , matrix2->dim);


    /*Single Thread Matrix multiplication*/
    run_test(outputMatrix_thread_per_matrix , matrix1 , matrix2 , multiplicationMethods[THREAD_PER_MATRIX] , THREAD_PER_MATRIX);
    /*--------------------------------------------------------------------------------------*/

    /*Thread per row Matrix multiplication*/
    run_test(outputMatrix_thread_per_row , matrix1 , matrix2 , multiplicationMethods[THREAD_PER_ROW] , THREAD_PER_ROW);
    /*--------------------------------------------------------------------------------------*/

    /*Thread per element Matrix multiplication*/
    run_test(outputMatrix_thread_per_element , matrix1 , matrix2 , multiplicationMethods[THREAD_PER_ELEMENT] , THREAD_PER_ELEMENT);
    /*--------------------------------------------------------------------------------------*/

    /* Set output files names*/
    writeOutputMatrixWrapper(outputMatrix_thread_per_matrix , filesPrefixes[2] , "_per_matrix.txt");
    writeOutputMatrixWrapper(outputMatrix_thread_per_row , filesPrefixes[2] , "_per_row.txt");
    writeOutputMatrixWrapper(outputMatrix_thread_per_element , filesPrefixes[2] , "_per_element.txt");

    /*Frees used data*/
    free_matrix(matrix1);
    free_matrix(matrix2);
    free_matrix(outputMatrix_thread_per_element);
    free_matrix(outputMatrix_thread_per_row);
    free_matrix(outputMatrix_thread_per_matrix);

    return 0;
}