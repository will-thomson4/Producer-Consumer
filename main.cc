/******************************************************************
 * The Main program with the two functions. A simple
 * example of creating and using a thread is provided.
 ******************************************************************/

#include "helper.h"
#include <random>
#include <fstream>


void *producer (void *id);
void *consumer (void *id);

int semIds = sem_create(SEM_KEY, 3);

int frontOfQueue = 0;
int backOfQueue = 0;

int const TIMEOUT = 20;

struct ProducerArguments {
  int threadId = 0;
  int jobsPerProducer = 0;
  int queueSize = 0;
  int* queue;
};

struct ConsumerArguments {
  int threadId = 0;
  int queueSize = 0;
  int* queue;
};

fstream file;

int main (int argc, char **argv)
{
  cout << "Running... Output will be put in output.txt" << endl;

  //--------------Initialise Variables-----------
  //Initialise cmd line inputs
  int queueSize = check_arg(argv[1]);
  int jobsPerProducer = check_arg(argv[2]);
  int numberOfProducers = check_arg(argv[3]);
  int numberOfConsumers = check_arg(argv[4]);
  if(queueSize < 0 || jobsPerProducer < 0 || numberOfProducers < 0 || numberOfConsumers < 0) {
    cerr << "There was an error with cmd line inputs!" << endl;
  }

  int queue[queueSize];

  file.open("output.txt");

  file << "Queue size = " << queueSize << endl;
  file << "Number of Items produced per producer = " << jobsPerProducer << endl;
  file << "Number of producers = " << numberOfProducers << endl;
  file << "Number of consumers = " << numberOfConsumers << endl;

  //Set up producer ids and consumer ids in arrays
  pthread_t producers[numberOfProducers];
  pthread_t consumers[numberOfConsumers];

  //Initialise semaphores
  if(semIds == -1) {
    cerr << "Error creating semaphore Ids" << endl;
    exit(1);
  }
  //Semaphore keeps track of the number of empty slots in the queue, initially this should be the queue max-size
  if(sem_init(semIds, 0, queueSize) < 0) {
    cerr << "Error initialising emptySlots semaphore" << endl;
    if(sem_close(semIds) < 0) {
      cerr << "Error closing semaphore, may still be active semaphores in the system" << endl;
    }
    exit(1);
  };
  
  //Semaphore keeps track of the number of used up slots in the queue, intially this should be zero
  if(sem_init(semIds, 1, 0) < 0) {
    cerr << "Error initialising fullSlots semaphore" << endl;
    if(sem_close(semIds) < 0) {
      cerr << "Error closing semaphore, may still be active semaphores in the system" << endl;
    }
    exit(1);
  };
  
  //Binary semaphore to keep to track of mutual exclusion to queue
  if(sem_init(semIds, 2, 1) < 0) {
    cerr << "Error initialising MUTEX semaphore" << endl;      
    if(sem_close(semIds) < 0) {
      cerr << "Error closing semaphore, may still be active semaphores in the system" << endl;
    }
    exit(1);
  };

  //----------------Create Threads--------------
  //Create producer threads
  for(int i = 0; i < numberOfProducers; i++) {
    ProducerArguments* producerArguments = new ProducerArguments();
    producerArguments->jobsPerProducer = jobsPerProducer;
    producerArguments->threadId = i;
    producerArguments->queueSize = queueSize;
    producerArguments->queue = queue;

    if(pthread_create (&producers[i], NULL, producer, (void*) producerArguments) != 0) {
      cerr << "Error creating producer threads" << endl;
      exit(1);
    }
  }

  //Create consumer threads
  for(int i = 0; i < numberOfConsumers; i++) {
    ConsumerArguments* consumerArguments = new ConsumerArguments();
    consumerArguments->threadId = i;
    consumerArguments->queueSize = queueSize;
    consumerArguments->queue = queue;
    
    if(pthread_create (&consumers[i], NULL, consumer, (void*) consumerArguments) != 0) {
      cerr << "Error creating consumer threads" << endl;
      exit(1);
    }
  }

  //-----------Process Cleanup-----------------
  //Joining producer threads
  for(int i = 0; i < numberOfProducers; i++) {
    if(pthread_join (producers[i], NULL) != 0) {
      cerr << "Error joining threads" << endl;
      exit(1);
    }
  }
  //Joining consumer threads
  for(int i = 0; i < numberOfConsumers; i++) {
    if(pthread_join (consumers[i], NULL) != 0) {
      cerr << "Error joining threads" << endl;
      exit(1);
    }
  }
  //Destroy semaphores
  if(sem_close(semIds) < 0) {
    cerr << "Error closing semaphore, may still be active semaphores in the system" << endl;
    exit(1);
  }

  cout << "Complete" << endl;
  return 0;
}

void *producer (void* args) 
{

  ProducerArguments* arguments = (ProducerArguments*) args;
  
  //For the number of jobs per producer, put each job into the queue
  for(int i = 0; i < arguments->jobsPerProducer; i++) {
    //Generate random numbers
    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_int_distribution<std::mt19937::result_type> dist10(1,10);
    std::uniform_int_distribution<std::mt19937::result_type> dist5(1,10);
  
    int randomNumber = dist10(rng);
    int randomWait = dist5(rng);

    //Wait until the number of empty slots in the queue is above 0 and then decrement it
    if(sem_wait(semIds, 0, TIMEOUT) < 0) {
      break;
    }

    //----------Critical Section---------
    sem_wait(semIds, 2);
    
    arguments->queue[backOfQueue] = randomNumber;
    int currentJob = backOfQueue;
    backOfQueue++;	
    
    if(backOfQueue >= arguments->queueSize) {
	    backOfQueue = backOfQueue % arguments->queueSize;
    }

    sem_signal(semIds, 2);
    //--------Critical Section End--------

    file << "Producer(" << arguments->threadId << "): Job Id " << currentJob << " created with duration " << randomNumber << endl;
    
    //We just added an element, so now increment the number of full slots in the queue
    sem_signal(semIds, 1);

    sleep(randomWait);
  }

  delete arguments;

  pthread_exit(0);
}

void *consumer (void* args) 
{  
  ConsumerArguments* arguments = (ConsumerArguments*) args;

  while(1) {	  
    //Wait until there is a slot taken up in the queue and then decrement it (time out after 20 seconds)
    if(sem_wait(semIds, 1, TIMEOUT) < 0) {
      break;
    }
    
    //----------Critical Section---------
    sem_wait(semIds, 2);
    int currentJob = frontOfQueue;
    int valueFromQueue = arguments->queue[frontOfQueue];
    frontOfQueue++;
    
    if(frontOfQueue >= arguments->queueSize) {
	    frontOfQueue = frontOfQueue % arguments->queueSize;
    }

    sem_signal(semIds, 2);
    //--------Critical Section End--------

    //Increment the number of empty slots in the queue 
    sem_signal(semIds, 0); 

    file << "Consumer(" << arguments->threadId << "): Job Id " << currentJob << " executing sleep duration " << valueFromQueue << endl;
    
    sleep(valueFromQueue);

    file << "Consumer(" << arguments->threadId << "): Job Id " << currentJob << " with duration " << valueFromQueue <<" completed" << endl;

  }

  delete arguments;

  pthread_exit (0);
}
