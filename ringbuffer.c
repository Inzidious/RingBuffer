//  This ringbuffer is a single producer, multiple consumer circular
//  queue that uses atomics to maintain lock free read and write
//  functionality.
//  there's a mutex based write function that we use to compare write
//  speeds against the atomic version
#include "ringbuffer.h"

//  rb_write takes in a character and adds it to the buffer
//  single producer model.
//  Uses lock free atomics 
void rb_write(RingBuf* rb, char newChar, int * write_index){
    
    //  load from header, and since only this thread can write to header
    //  we are relaxed
    size_t h = atomic_load_explicit(&rb->head, memory_order_relaxed);

    //  write data
    rb->arr[h] = newChar;
    (*write_index) = h;
    //  fence off the memory write so we dont update the head pointer until we
    //  finish the memory copy
    //  release means nothing can be moved below
    atomic_thread_fence(memory_order_release);
    
    //  move head forward after write and store
    atomic_store_explicit(&rb->head, ++h & BUFFER_MASK, memory_order_relaxed);
    
    //  go through readers and move, if full
    for(int i = 0; i < rb->num_readers; i++){
        //  load from reader, and since the other thread can write 
        //  ensure this load happens before anything
        //  acquire means nothing can happen above
        size_t t = atomic_load_explicit(&rb->readers[i], memory_order_acquire);
        
        //  if h == t after head move, we are full and we need to move the reader pointer over so
        //  we can keep writing
        if ((rb->head & BUFFER_MASK)==(t & BUFFER_MASK)){
            
            //  Read pointer updated, so store new
            //  use release as we want above code to execute before we store
            atomic_store_explicit(&rb->readers[i], ++t & BUFFER_MASK, memory_order_release);
        }
    }
}

//  rb_read reads a character with specified reader id
//  or returns 'E' for empty buffer
char rb_read(RingBuf* rb, int reader_id, int *read_index){
    //  read buffer always either at least 1 ahead of write pointer
    //  this means we should never be reading from a buffer we are writing to
    //  or, if empty, equal. 

    //  load from reader index, and since the other thread can write
    //  ensure this load happens before anything
    //  acquire means nothing can happen above
    size_t t = atomic_load_explicit(&rb->readers[reader_id], memory_order_acquire);
   
    //  load from header, and since the other thread can write to header
    //  ensure this load happens before anything
    //  acquire means nothing can happen above
    size_t h = atomic_load_explicit(&rb->head, memory_order_acquire);
    
    char val = 'E';
    //  If h = t we are empty. else, read
    if (((h & BUFFER_MASK) != (t & BUFFER_MASK))){
        //  make sure t loads before we read
        //  fence off the memory read so we update the reader pointer
        //  before we read
        //  acquire means the read cannot move above the fence
        atomic_thread_fence(memory_order_acquire);
        
        //  read
        *read_index = t;
        val = rb->arr[t];

        //  Read pointer updated, so store new
        //  use release as we want above code to execute before we store
        atomic_store_explicit(&rb->readers[reader_id], (++t & BUFFER_MASK), memory_order_release);
    }
    return val;
}
 
//  reset buffer indexes
void rb_reset(RingBuf* rb){
    rb->head = 0;
    memset(rb->arr, 0, BUFFER_SIZE);
}
 
RingBuf* rb_init(){
    RingBuf* rb = malloc(sizeof(RingBuf));
    rb_reset(rb);
    return rb;
}
 
void rb_free(RingBuf* rb){
    free(rb);
}

char get_rand_char(){
    const size_t max_index = sizeof(charset) - 1; // Exclude null terminator

    // Generate a random index
    int random_index = rand() % max_index;

    return charset[random_index];
}

//  Not ideal to have thread variables as globals, but the void * syntax of
//  thread inputs is confusing and convoluted 
int reader_one; 
int reader_two;
pthread_t t1, t2, t3;
pthread_mutex_t mutex;

//  write mutex takes in a character and writes it to the buffer using
//  mutexes and not atomics. this is mostly for profile purposes
void rb_write_mutex(RingBuf* rb, char newChar){

    pthread_mutex_lock(&mutex);

    //  load from header
    size_t h = rb->head;

    //  write data
    rb->arr[h] = newChar;
    
    rb->head = (++h & BUFFER_MASK);
    
    //  if h == t after head move, we are full and we need to move the read pointer over so
    //  we can keep writing
    for(int i = 0; i < rb->num_readers; i++){
        size_t t = rb->readers[i];

        if (rb->head==t){
            rb->readers[i] = ++t & BUFFER_MASK;
        }
    }

    pthread_mutex_unlock(&mutex);
}

//  Thread for writing to the buffer
void * write_thread(void * rb){
    char c;
    int ret;

    while(true){
        c = get_rand_char();
        rb_write(rb, c, &ret);
        printf("write_one:%c at index:%d\n", c, ret);
        
        sleep(1);
    }
}

//  First thread for buffer read
void * read_thread_one(void * rb){
    int id = reader_one;
    int read_index;
    char c;

    while(true){
        sleep(2);
        c = rb_read(rb, id, &read_index);
        printf("\treader_one:%c at index:%d\n", c, read_index);   
    }
}

//  second thread for buffer read
void * read_thread_two(void * rb){
    int id = reader_two;
    int read_index;
    char c;

    while(true){
        sleep(3);
        c = rb_read(rb, id, &read_index);
        printf("\treader_two:%c at index:%d\n", c, read_index);
    }
}

//  ctrl-c sigint handling
void handle_sigint(int sig) {
    printf("\nCaught Ctrl+C! Performing cleanup...\n");

    pthread_cancel(t1);
    pthread_cancel(t2);
    pthread_cancel(t3);

    pthread_mutex_destroy(&mutex);

    exit(0);
}

//  This function runs over atomic and mutex writes
//  PROFILE_ITERATIONS number of times and compares
//  the difference in clock cycles consumed
void profile(RingBuf* rb){
    clock_t start = clock();
    for(int i =0;i<PROFILE_ITERATIONS;i++){
        rb_write_mutex(rb, 'A');
    }
    clock_t end = clock() ;
    int elapsed_time_mutex = (end-start);

    start = clock();
    int index;
    for(int i =0;i<PROFILE_ITERATIONS;i++){
        rb_write(rb, 'A', &index);
    }
    end = clock();
    int elapsed_time_atomic = (end-start);

    printf("Elapsed_mutex_cycles for %d iterations:%d\n", PROFILE_ITERATIONS, elapsed_time_mutex );
    printf("Elapsed_atomic_cycles for %d iterations:%d\n", PROFILE_ITERATIONS, elapsed_time_atomic);
    printf("Atomic writes are %d less cycles!\n", elapsed_time_mutex - elapsed_time_atomic);
}

//  Register a new reader
int get_reader_id(RingBuf* rb){
    return rb->num_readers++;
}

//  register the function for sigint
void register_sigint(){
    struct sigaction sa;
    
    // Initialize sigaction structure
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &handle_sigint;
    
    sigfillset(&sa.sa_mask);
    
    // Register the handler for SIGINT
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
}

int main(){
    //  Initialize
    register_sigint();
    pthread_mutex_init(&mutex, NULL);
    RingBuf *rb = rb_init();
    
    //  Run write profile first
    profile(rb);

    printf("Now running worker threads. Use CTRL-C to end.\n");

    //  Reset
    rb_reset(rb);

    //  register readers
    reader_one = get_reader_id(rb);
    reader_two = get_reader_id(rb);

    //  create and join worker threads
    pthread_create(&t1, NULL, write_thread, (void *)rb);
    pthread_create(&t2, NULL, read_thread_one, (void *)rb);
    pthread_create(&t3, NULL, read_thread_two, (void *)rb);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    pthread_join(t3, NULL);

    free(rb);
    return 0;
}
