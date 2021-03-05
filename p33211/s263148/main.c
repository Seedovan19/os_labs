//A=266;B=0xB46BBDF2;C=malloc;D=30;E=149;F=block;G=50;H=random;I=139;J=avg;K=flock

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <errno.h>

typedef struct {
  int *address;
  FILE* input;
  int chunk;
} malloc_data;

typedef struct {
  char *start;
  size_t block;
  int file;
  int upperBound;
} writeF_data;

typedef struct {
  int file;
  int *acc;
  int *ct;
  off_t offset;
} readF_data;

#define A 266
#define A_BYTES (266 * 1000 * 1000) /* 266 MB */
#define D 30
#define E 149
#define E_BYTES (149 * 1000 * 1000) /* 149 MB */
#define G 50 /* 50 B */
#define I 139
#define NUM_OF_FILES 2

pthread_mutex_t countMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mallocMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t randomMutex = PTHREAD_MUTEX_INITIALIZER;
void *fillMalloc(void* arg);
void *writeFile(void* arg);
void *readFile(void* arg);

int main(int argc, char* argv[]){
    for ( ; ; ) {
      printf("Начало программы\n");
      FILE* rFile = fopen("/dev/urandom", "r");
      pthread_t mallocThr[D];
      malloc_data mallocThr_data[D];
      int* start = (int*) malloc(A_BYTES), *next = start;
      int numOfInts = A_BYTES / sizeof(int), length = numOfInts / D;
      if (start != NULL) {
          for (int i = 0; i < D; i++) {
            mallocThr_data[i].input = rFile;
            mallocThr_data[i].address = next;
            next += length;
            if (i == D - 1) { mallocThr_data[i].chunk = length + numOfInts % D; }
            else { mallocThr_data[i].chunk = length; }
            pthread_create(&mallocThr[i], NULL, fillMalloc, &mallocThr_data[i]);
          }
          //ожидаем выполнения потоков
          for (int i = 0; i < D; i++) { pthread_join(mallocThr[i], NULL); }
          fclose(rFile);
          printf("Запись в память окончена\n");
          
          pthread_t writeF[I], readF[I];
          writeF_data writeF_data[I];
          readF_data readF_data[I];
          int fileId, files[NUM_OF_FILES];
          int allSum = 0, allCt = 0;
    
          //создаем файлы
          for (int f = 0; f < NUM_OF_FILES; f++) {
            char *name = (char*) malloc(sizeof(char));
            sprintf(name, "%i", f);
            files[f] = open(name, O_RDWR | O_APPEND, 0644);
            free(name);
          }
          
          for (int i = 0; i < I; i++) {
            fileId = i % NUM_OF_FILES;
            writeF_data[i].file = files[fileId];
            writeF_data[i].start = (char*) start;
            writeF_data[i].block = (size_t) G;
            writeF_data[i].upperBound = numOfInts;

            readF_data[fileId].file = files[fileId];
            readF_data[fileId].acc = &allSum;
            readF_data[fileId].ct = &allCt;
            readF_data[fileId].offset = (off_t) G;
          }
          
          //создаем потоки на запись/чтение
          for (int j = 1; j < E_BYTES / (I / NUM_OF_FILES * G); j++) {
            for (int i = 0; i < I; i++) {
                //один блок G за поток
                pthread_create(&writeF[i], NULL, writeFile, &writeF_data[i]);
            }
            for (int k = 0; k < NUM_OF_FILES; k++) {
                pthread_create(&readF[k], NULL, readFile, &readF_data[k]);
            }
            //выполняются оставшиеся потоки
            for (int a = I - 1; a >= 0; a--) { pthread_join(writeF[a], NULL); }
            for (int b = 0; b < NUM_OF_FILES; b++) { pthread_join(readF[b], NULL); }
            if (j % 300 == 0) {
                printf("Сумма чисел = %i, Количество чисел = %i, avg = %f\n", allSum, allCt, (double)allSum / (double)allCt);
            }
          }
      }
      else {
          printf("Не получилось аллоцировать память\n");
      }
    free(start);
  }
  return EXIT_SUCCESS;
}

void *fillMalloc(void* arg) {
  malloc_data *data = (malloc_data *) arg;
  pthread_mutex_lock(&mallocMutex);
  for (int i = 0; i < data->chunk; i++) {
    *(data->address) = getw(data->input);
    (data->address)++;
  }
  pthread_mutex_unlock(&mallocMutex);
  pthread_exit(NULL);
}

void *writeFile(void* arg) {
  writeF_data *data = (writeF_data *) arg;
  long int result;
  
  //адрес из которого берем данные для записи в файл получаем рандомно, rand() возвращает псевдослучайное число
  pthread_mutex_lock(&randomMutex);
  char* random = data->start + rand() % data->upperBound - data->block;
  pthread_mutex_unlock(&randomMutex);

  flock(data->file, LOCK_EX);
  lseek(data->file, 0L, SEEK_END);
  result = write(data->file, random, data->block);
  flock(data->file, LOCK_UN);
    
  if (result == -1) { printf("Write thread failed: %s\n", strerror(errno)); }
  pthread_exit(NULL);
}


void *readFile(void* arg) {
  readF_data *data = (readF_data *) arg;
  char* buf = (char *) malloc(data->offset);
  if (buf == NULL) {
    printf("Could not allocate read buffer\n");
    pthread_exit(NULL);
  }

  flock(data->file, LOCK_EX);
  lseek(data->file, -(data->offset), SEEK_END);
  long int result = read(data->file, buf, data->offset);
  flock(data->file, LOCK_UN);

  if (result == -1) {
    printf("Read thread failed: %s\n", strerror(errno));
    free(buf);
    pthread_exit(NULL);
  }

  pthread_mutex_lock(&countMutex);
  for (int i = 0; i < data->offset; i++) {
      *(data->acc) += (int) buf[i];
  }
  *(data->ct) += data->offset;
  pthread_mutex_unlock(&countMutex);
  free(buf);

  pthread_exit(NULL);
}
