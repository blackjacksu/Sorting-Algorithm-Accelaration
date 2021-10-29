//
// Sorts a list using multiple threads
//

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <limits.h>

#define MAX_THREADS     65536
#define MAX_LIST_SIZE   100000000

#define DEBUG 0

// Thread variables
//
// VS: ... declare thread variables, mutexes, condition varables, etc.,
// VS: ... as needed for this assignment 
//
typedef struct thread_arg{
    int q;
    int level;
    int np;
    int my_id;
    int * ptr;
};

pthread_cond_t sort_complete_cond;
pthread_mutex_t sort_complete_mutex;

pthread_mutex_t level_count_mutex;
int level_count;

pthread_cond_t level_complete_cond;
pthread_mutex_t level_complete_mutex;

typedef struct thread_node
{
    int insert_count;
    pthread_t thread;
    pthread_mutex_t insert_count_mutex;
    pthread_cond_t insert_complete_cond;
    pthread_mutex_t insert_complete_mutex;
};

struct thread_node threads[MAX_THREADS];
// Global variables
int num_threads;		// Number of threads to create - user input 
int list_size;			// List size
int *list;			// List of values
int *work;			// Work array
int *list_orig;			// Original list of values, used for error checking

// Print list - for debugging
void print_list(int *list, int list_size) {
    int i;
    for (i = 0; i < list_size; i++) {
        printf("[%d] \t %16d\n", i, list[i]); 
    }
    printf("--------------------------------------------------------------------\n"); 
}

// Comparison routine for qsort (stdlib.h) which is used to 
// a thread's sub-list at the start of the algorithm
int compare_int(const void *a0, const void *b0) {
    int a = *(int *)a0;
    int b = *(int *)b0;
    if (a < b) {
        return -1;
    } else if (a > b) {
        return 1;
    } else {
        return 0;
    }
}

// Return index of first element larger than or equal to v in sorted list
// ... return last if all elements are smaller than v
// ... elements in list[first], list[first+1], ... list[last-1]
//
//   int idx = first; while ((v > list[idx]) && (idx < last)) idx++;
//
int binary_search_lt(int v, int *list, int first, int last) {
   
    // Linear search code
    // int idx = first; while ((v > list[idx]) && (idx < last)) idx++; return idx;

    int left = first; 
    int right = last-1; 

    if (list[left] >= v) return left;
    if (list[right] < v) return right+1;
    int mid = (left+right)/2; 
    while (mid > left) {
        if (list[mid] < v) {
	    left = mid; 
	} else {
	    right = mid;
	}
	mid = (left+right)/2;
    }
    return right;
}
// Return index of first element larger than v in sorted list
// ... return last if all elements are smaller than or equal to v
// ... elements in list[first], list[first+1], ... list[last-1]
//
//   int idx = first; while ((v >= list[idx]) && (idx < last)) idx++;
//
int binary_search_le(int v, int *list, int first, int last) {

    // Linear search code
    // int idx = first; while ((v >= list[idx]) && (idx < last)) idx++; return idx;
 
    int left = first; 
    int right = last-1; 

    if (list[left] > v) return left; 
    if (list[right] <= v) return right+1;
    int mid = (left+right)/2; 
    while (mid > left) {
        if (list[mid] <= v) {
	    left = mid; 
	} else {
	    right = mid;
	}
	mid = (left+right)/2;
    }
    return right;
}

void * sort_list_parallel(struct thread_arg * arg)
{
    int q = arg->q;
    int level = arg->level;
    int np = arg->np;
    int my_id = arg->my_id;
    int * ptr = arg->ptr;

    int my_own_blk, my_own_idx;
    int my_blk_size, my_search_blk, my_search_idx, my_search_idx_max;
    int my_write_blk, my_write_idx;
    int my_search_count; 
    int i, idx, i_write; 

if (DEBUG) print_list(list, list_size); 

    // Sort list
    for (level = 0; level < q; level++) {

	    my_blk_size = np * (1 << level); 

	    my_own_blk = ((my_id >> level) << level);
	    my_own_idx = ptr[my_own_blk];

	    my_search_blk = ((my_id >> level) << level) ^ (1 << level);
	    my_search_idx = ptr[my_search_blk];
	    my_search_idx_max = my_search_idx+my_blk_size;

	    my_write_blk = ((my_id >> (level+1)) << (level+1));
	    my_write_idx = ptr[my_write_blk];

	    idx = my_search_idx;
	    
	    my_search_count = 0;


	    // Binary search for 1st element
	    if (my_search_blk > my_own_blk) {
               idx = binary_search_lt(list[ptr[my_id]], list, my_search_idx, my_search_idx_max); 
	    } else {
               idx = binary_search_le(list[ptr[my_id]], list, my_search_idx, my_search_idx_max); 
	    }
	    my_search_count = idx - my_search_idx;
	    i_write = my_write_idx + my_search_count + (ptr[my_id]-my_own_idx); 
	    work[i_write] = list[ptr[my_id]];
        pthread_mutex_lock(&threads[my_id/2*2].insert_count_mutex);
        threads[my_id/2*2].insert_count++;
        pthread_mutex_unlock(&threads[my_id/2*2].insert_count_mutex);

	    // Linear search for 2nd element onwards
	    for (i = ptr[my_id]+1; i < ptr[my_id+1]; i++) {
	        if (my_search_blk > my_own_blk) {
		        while ((list[i] > list[idx]) && (idx < my_search_idx_max)) {
		            idx++; 
                    my_search_count++;
		        }
		    } 
            else {
		        while ((list[i] >= list[idx]) && (idx < my_search_idx_max)) {
		            idx++; 
                    my_search_count++;
		        }
		    }
		    i_write = my_write_idx + my_search_count + (i-my_own_idx); 
		    work[i_write] = list[i];
            pthread_mutex_lock(&threads[my_id/2*2].insert_count_mutex);
            threads[my_id/2*2].insert_count++;
            pthread_mutex_unlock(&threads[my_id/2*2].insert_count_mutex);
	    }

        // wait here until all element is inserted in work
        if (threads[my_id/2*2].insert_count == (my_blk_size*2))
        {
            // The insertion in work list is done
            threads[my_id/2*2].insert_count = 0;
            pthread_mutex_lock(&level_count_mutex);
            level_count++;
            pthread_mutex_unlock(&level_count_mutex);
            pthread_cond_signal(&threads[my_id].insert_complete_cond);
        }
        else
        {
            // The insertion in work list is not yet done
            // Please kindly wait here for a while
            pthread_mutex_lock(&threads[my_id].insert_complete_mutex);
            pthread_cond_wait(&threads[my_id].insert_complete_cond, &threads[my_id].insert_complete_mutex);
            pthread_mutex_unlock(&threads[my_id].insert_complete_mutex);
        }

        // wait here until all thread are done with sorting
        if (level_count == list_size/my_blk_size)
        {
            // The insertion in work list is done
            level_count = 0;
            pthread_cond_signal(&level_complete_cond);
        }
        else
        {
            // The insertion in work list is not yet done
            // Please kindly wait here for a while
            pthread_mutex_lock(&level_complete_mutex);
            pthread_cond_wait(&level_complete_cond, &level_complete_mutex);
            pthread_mutex_unlock(&level_complete_mutex);
        }

        // Copy work into list for next itertion
	    for (i = ptr[my_id]; i < ptr[my_id+1]; i++) 
        {
	        list[i] = work[i];
	    }

        // terminate those thread that is not needed
        if ((my_id % (1 << (level+1))) == 0)
        {
            pthread_exit(0);
        }
	}

    if (level == q)
    {
        // The sorting is completed
        pthread_cond_signal(&sort_complete_cond);
    }

if (DEBUG) print_list(list, list_size); 

}

// Sort list via parallel merge sort
//
// VS: ... to be parallelized using threads ...
//
void sort_list(int q) {

    struct thread_arg * arg = (struct thread_arg *) malloc(sizeof(struct thread_arg));
    int my_id; 
    int np, my_list_size; 
    int ptr[num_threads+1];
    int thread_status;

    np = list_size/num_threads; 	// Sub list size 

    // Initialize starting position for each sublist
    for (my_id = 0; my_id < num_threads; my_id++) {
        ptr[my_id] = my_id * np;
    }
    ptr[num_threads] = list_size;

    // Sort local lists
    for (my_id = 0; my_id < num_threads; my_id++) 
    {
        my_list_size = ptr[my_id+1]-ptr[my_id];
        qsort(&list[ptr[my_id]], my_list_size, sizeof(int), compare_int);
    }

    for (my_id = 0; my_id < num_threads; my_id++) 
    {
        arg->level = 0;
        arg->my_id = my_id;
        arg->np = np;
        arg->ptr = ptr;
        arg->q = q;

        thread_status = pthread_create(&threads[my_id].thread, NULL, sort_list_parallel, (void *)arg);

        if (thread_status != 0)
        {
            printf("Thread %d created fail, error:%d\n", my_id, thread_status);
        }
    }
    // wait sorting complete signal
    pthread_mutex_lock(&sort_complete_mutex);

    pthread_cond_wait(&sort_complete_cond, &sort_complete_mutex);

    pthread_mutex_unlock(&sort_complete_mutex);
}



// Main program - set up list of random integers and use threads to sort the list
//
// Input: 
//	k = log_2(list size), therefore list_size = 2^k
//	q = log_2(num_threads), therefore num_threads = 2^q
//
int main(int argc, char *argv[]) {

    struct timespec start, stop, stop_qsort;
    double total_time, time_res, total_time_qsort;
    int k, q, j, error; 
    int i, thread_init;

    // Read input, validate
    if (argc != 3) {
	    printf("Need two integers as input \n"); 
	    printf("Use: <executable_name> <log_2(list_size)> <log_2(num_threads)>\n"); 
	    exit(0);
    }
    k = atoi(argv[argc-2]);
    if ((list_size = (1 << k)) > MAX_LIST_SIZE) {
	    printf("Maximum list size allowed: %d.\n", MAX_LIST_SIZE);
	    exit(0);
    }; 
    q = atoi(argv[argc-1]);
    if ((num_threads = (1 << q)) > MAX_THREADS) {
	    printf("Maximum number of threads allowed: %d.\n", MAX_THREADS);
	    exit(0);
    }; 
    if (num_threads > list_size) {
	    printf("Number of threads (%d) < list_size (%d) not allowed.\n", 
	    num_threads, list_size);
	    exit(0);
    }; 

    // Allocate list, list_orig, and work

    list = (int *) malloc(list_size * sizeof(int));
    list_orig = (int *) malloc(list_size * sizeof(int));
    work = (int *) malloc(list_size * sizeof(int));

//
// VS: ... May need to initialize mutexes, condition variables, 
// VS: ... and their attributes
//
    for (i = 0; i < MAX_THREADS; i++)
    {
        threads[i].insert_count = 0;

        thread_init = pthread_mutex_init(&threads[i].insert_count_mutex, NULL);

        if (thread_init != 0)
        {
            printf("Thread %d mutex init failed, error:%d\n", i, thread_init);
        }

        thread_init = pthread_cond_init(&threads[i].insert_complete_cond, NULL);

        if (thread_init != 0)
        {
            printf("Thread %d cond init failed, error:%d\n", i, thread_init);
        }

        thread_init = pthread_mutex_init(&threads[i].insert_complete_mutex, NULL);

        if (thread_init != 0)
        {
            printf("Thread %d cond mutex init failed, error:%d\n", i, thread_init);
        }
    }

    level_count = 0;

    thread_init = pthread_mutex_init(&sort_complete_mutex, NULL);

    if (thread_init != 0)
    {
        printf("Sorting completing mutex init failed, error:%d\n", thread_init);
    }

    thread_init = pthread_cond_init(&sort_complete_cond, NULL);

    if (thread_init != 0)
    {
        printf("Sorting completing cond init failed, error:%d\n", thread_init);
    }

    thread_init = pthread_mutex_init(&level_complete_mutex, NULL);

    if (thread_init != 0)
    {
        printf("Level completing mutex init failed, error:%d\n", thread_init);
    }

    thread_init = pthread_cond_init(&level_complete_cond, NULL);

    if (thread_init != 0)
    {
        printf("Level completing cond init failed, error:%d\n", thread_init);
    }

    thread_init = pthread_mutex_init(&level_count_mutex, NULL);

    if (thread_init != 0)
    {
        printf("Level count mutex init failed, error:%d\n", thread_init);
    }

    // Initialize list of random integers; list will be sorted by 
    // multi-threaded parallel merge sort
    // Copy list to list_orig; list_orig will be sorted by qsort and used
    // to check correctness of multi-threaded parallel merge sort
    srand48(0); 	// seed the random number generator
    for (j = 0; j < list_size; j++) {
	list[j] = (int) lrand48();
	list_orig[j] = list[j];
    }
    // duplicate first value at last location to test for repeated values
    list[list_size-1] = list[0]; list_orig[list_size-1] = list_orig[0];

    // Create threads; each thread executes find_minimum
    clock_gettime(CLOCK_REALTIME, &start);

//
// VS: ... may need to initialize mutexes, condition variables, and their attributes
//

// Serial merge sort 
// VS: ... replace this call with multi-threaded parallel routine for merge sort
// VS: ... need to create threads and execute thread routine that implements 
// VS: ... parallel merge sort

    sort_list(q);

    // Compute time taken
    clock_gettime(CLOCK_REALTIME, &stop);
    total_time = (stop.tv_sec-start.tv_sec)
	+0.000000001*(stop.tv_nsec-start.tv_nsec);

    // Check answer
    qsort(list_orig, list_size, sizeof(int), compare_int);
    clock_gettime(CLOCK_REALTIME, &stop_qsort);
    total_time_qsort = (stop_qsort.tv_sec-stop.tv_sec)
	+0.000000001*(stop_qsort.tv_nsec-stop.tv_nsec);

    error = 0; 
    for (j = 1; j < list_size; j++) {
	if (list[j] != list_orig[j]) error = 1; 
    }

    if (error != 0) {
	printf("Houston, we have a problem!\n"); 
    }

    // Print time taken
    printf("List Size = %d, Threads = %d, error = %d, time (sec) = %8.4f, qsort_time = %8.4f\n", 
	    list_size, num_threads, error, total_time, total_time_qsort);

// VS: ... destroy mutex, condition variables, etc.

    free(list); free(work); free(list_orig); 

}

