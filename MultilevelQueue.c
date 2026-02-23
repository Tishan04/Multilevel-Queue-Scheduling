#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>
#include <time.h>

#define TIME_QUANTUM 20

typedef struct Process {
    int pid;
    int burst_time;
    int remaining_time;
    int arrival_time;
    int completion_time;
    int turnaround_time;
    int waiting_time;
    int priority_queue;
    bool is_completed;
    bool is_enqueued;
    struct Process *next;
} Process;

typedef struct Queue {
    Process *front;
    Process *rear;
    int count;
    int active_count; //tracks number of non-completed processes in this queue
    char *name;
    int algorithm_type;
} Queue;

Queue queues[4];
int current_time = 0;
int completed_count = 0; //increments each time a process finishes, used to check overall completion

//Initialize the 4 queues
void init_queues() {
    char* names[] = {"Q0 (RR)", "Q1 (SJF Preemptive)", "Q2 (SJF Non-Preemptive)", "Q3 (FIFO)"};
    int algorithms[] = {0, 1, 2, 3};

    for (int i = 0; i < 4; i++) {
        queues[i].front = NULL;
        queues[i].rear = NULL;
        queues[i].count = 0;
        queues[i].active_count = 0;
        queues[i].name = names[i];
        queues[i].algorithm_type = algorithms[i];
    }
}

//Adds a process to the rear of the specified queue
void enqueue(int queue_num, Process* p) {
    p->next = NULL;
    if (queues[queue_num].rear == NULL) {
        queues[queue_num].front = p;
        queues[queue_num].rear = p;
    } else {
        queues[queue_num].rear->next = p;
        queues[queue_num].rear = p;
    }
    queues[queue_num].count++;
    queues[queue_num].active_count++;
}

//Removes and returns the front process of the specified queue
Process* dequeue(int queue_num) {
    if (queues[queue_num].front == NULL) return NULL;

    Process* temp = queues[queue_num].front;
    queues[queue_num].front = queues[queue_num].front->next;
    if (queues[queue_num].front == NULL) {
        queues[queue_num].rear = NULL;
    }
    queues[queue_num].count--;
    temp->next = NULL;
    return temp;
}

//function to remove a specific process from anywhere in the queue.
void remove_process(int queue_num, Process* prev, Process* curr) {
    if (prev == NULL) {
        queues[queue_num].front = curr->next;
    } else {
        prev->next = curr->next;
    }
    if (curr == queues[queue_num].rear) {
        queues[queue_num].rear = prev;
    }
    queues[queue_num].count--;
    curr->next = NULL;
}

//Checks if all processes are done by comparing the global completed_count to n.
bool is_executing_completed(int n) {
    return completed_count == n;
}

//checks if there are any non-completed process inside the queue.
bool queue_has_processes(int queue_num) {
    return queues[queue_num].active_count > 0;
}

//finds and returns the index of the highest priority queue.
int find_highest_priority_queue() {
    for (int i = 0; i < 4; i++) {
        if (queue_has_processes(i)) return i;
    }
    return -1;
}

//Finds the shortest job in the queue using bubble sort on a temporary array.
Process* find_shortest_job(int queue_num, Process** prev_out) {
    //collect all runnable processes into a temporary array
    Process* temp_arr[100];
    int count = 0;
    Process* curr = queues[queue_num].front;

    while (curr != NULL) {
        if (!curr->is_completed && curr->remaining_time > 0) {
            temp_arr[count++] = curr;
        }
        curr = curr->next;
    }

    if (count == 0) return NULL;

    //bubble sort the array by remaining time
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            if (temp_arr[j]->remaining_time > temp_arr[j+1]->remaining_time) {
                Process* swap = temp_arr[j];
                temp_arr[j] = temp_arr[j+1];
                temp_arr[j+1] = swap;
            }
        }
    }

    //the shortest job is at index 0
    Process* target = temp_arr[0];
    Process* prev = NULL;
    curr = queues[queue_num].front;
    while (curr != NULL && curr != target) {
        prev = curr;
        curr = curr->next;
    }

    if (prev_out != NULL) *prev_out = prev;
    return target;
}

void enqueue_arrived_processes(Process processes[], int n) {
    //building a sorted index array by arrival time
    static int sorted[150];
    static bool sorted_ready = false;

    if (!sorted_ready) {
        for (int i = 0; i < n; i++) sorted[i] = i;

        //Bubble sort indices by arrival time
        for (int i = 0; i < n - 1; i++) {
            for (int j = 0; j < n - i - 1; j++) {
                if (processes[sorted[j]].arrival_time > processes[sorted[j+1]].arrival_time) {
                    int tmp = sorted[j];
                    sorted[j] = sorted[j+1];
                    sorted[j+1] = tmp;
                }
            }
        }
        sorted_ready = true;
    }
    static int next_to_check = 0;

    while (next_to_check < n) {
        int i = sorted[next_to_check];
        if (processes[i].arrival_time <= current_time && !processes[i].is_enqueued) {
            enqueue(processes[i].priority_queue, &processes[i]);
            processes[i].is_enqueued = true;
            next_to_check++;
        } else {
            break;
        }
    }
}

//round robin implementation
int execute_rr(int queue_num, int time_slice) {
    if (!queue_has_processes(queue_num)) return 0;

    static int last_pid = -1; //remembers the last executed process PID across calls

    Process* p = NULL;
    Process* prev = NULL;
    Process* p_prev = NULL;

    //find the first runnable process AFTER last_pid
    bool found_last = false;
    Process* curr = queues[queue_num].front;
    Process* candidate = NULL;
    Process* candidate_prev = NULL;

    while (curr != NULL) {
        if (curr->pid == last_pid) {
            found_last = true; //found where we left off
        } else if (found_last && !curr->is_completed && curr->remaining_time > 0) {
            candidate = curr;
            candidate_prev = prev;
            break; //first runnable process after last_pid
        }
        prev = curr;
        curr = curr->next;
    }

    //if nothing found after last_pid, wrap around to the front
    if (candidate == NULL) {
        curr = queues[queue_num].front;
        prev = NULL;
        while (curr != NULL) {
            if (!curr->is_completed && curr->remaining_time > 0) {
                candidate = curr;
                candidate_prev = prev;
                break;
            }
            prev = curr;
            curr = curr->next;
        }
    }

    if (candidate == NULL) return 0;

    p = candidate;
    p_prev = candidate_prev;

    //remove p from the queue, execute it, then re-add to rear if not done
    remove_process(queue_num, p_prev, p);

    int exec_time = (p->remaining_time < time_slice) ? p->remaining_time : time_slice;

    p->remaining_time -= exec_time;
    current_time += exec_time;
    last_pid = p->pid; //remember this process for next round

    if (p->remaining_time <= 0) {
        p->remaining_time = 0;
        p->is_completed = true;
        p->completion_time = current_time;
        queues[queue_num].active_count--;
        completed_count++;
    } else {
        //Not finished, manually re-link to rear without touching active_count
        p->next = NULL;
        if (queues[queue_num].rear == NULL) {
            queues[queue_num].front = p;
            queues[queue_num].rear = p;
        } else {
            queues[queue_num].rear->next = p;
            queues[queue_num].rear = p;
        }
        queues[queue_num].count++;
    }
    return exec_time;
}

//Preemptive SJF implementation
int execute_sjf_preemptive(int queue_num, int time_slice) {
    if (!queue_has_processes(queue_num)) return 0;

    Process* prev = NULL;
    Process* p = find_shortest_job(queue_num, &prev);
    if (p == NULL) return 0;

    int exec_time = (p->remaining_time < time_slice) ? p->remaining_time : time_slice;

    p->remaining_time -= exec_time;
    current_time += exec_time;

    if (p->remaining_time <= 0) {
        p->remaining_time = 0;
        p->is_completed = true;
        p->completion_time = current_time;
        queues[queue_num].active_count--;
        completed_count++;
        remove_process(queue_num, prev, p);
    }
    return exec_time;
}

//Non-Preemptive SJF implementation
int execute_sjf_nonpreemptive(int queue_num, int time_slice) {
    if (!queue_has_processes(queue_num)) return 0;

    Process* prev = NULL;
    Process* p = find_shortest_job(queue_num, &prev);
    if (p == NULL) return 0;

    int exec_time = (p->remaining_time < time_slice) ? p->remaining_time : time_slice;

    p->remaining_time -= exec_time;
    current_time += exec_time;

    if (p->remaining_time <= 0) {
        p->remaining_time = 0;
        p->is_completed = true;
        p->completion_time = current_time;
        queues[queue_num].active_count--;
        completed_count++;
        remove_process(queue_num, prev, p);
    }
    return exec_time;
}

//FIFO execution implementation
int execute_fifo(int queue_num, int time_slice) {
    if (!queue_has_processes(queue_num)) return 0;

    //Clean up any completed processes that may there in the front
    while (queues[queue_num].front != NULL &&
           (queues[queue_num].front->is_completed || queues[queue_num].front->remaining_time <= 0)) {
        dequeue(queue_num);
    }

    Process* p = queues[queue_num].front;
    if (p == NULL) return 0;

    int exec_time = (p->remaining_time < time_slice) ? p->remaining_time : time_slice;
    p->remaining_time -= exec_time;
    current_time += exec_time;

    if (p->remaining_time <= 0) {
        p->remaining_time = 0;
        p->is_completed = true;
        p->completion_time = current_time;
        queues[queue_num].active_count--;
        completed_count++;
        dequeue(queue_num);
    }
    return exec_time;
}

//function to calculate metrics to analyze.
void calculate_metrics(Process processes[], int n) {
    float total_waiting = 0, total_turnaround = 0;

    //calculate Turnaround Time(TAT) and Waiting Time(WT) for all processes.
    for (int i = 0; i < n; i++) {
        processes[i].turnaround_time = processes[i].completion_time - processes[i].arrival_time;
        processes[i].waiting_time = processes[i].turnaround_time - processes[i].burst_time;
        total_waiting += processes[i].waiting_time;
        total_turnaround += processes[i].turnaround_time;
    }

    //find min and max TAT and WT across all processes.
    int min_tat = processes[0].turnaround_time, max_tat = processes[0].turnaround_time;
    int min_wt  = processes[0].waiting_time,    max_wt  = processes[0].waiting_time;

    for (int i = 1; i < n; i++) {
        if (processes[i].turnaround_time < min_tat) min_tat = processes[i].turnaround_time;
        if (processes[i].turnaround_time > max_tat) max_tat = processes[i].turnaround_time;
        if (processes[i].waiting_time < min_wt) min_wt = processes[i].waiting_time;
        if (processes[i].waiting_time > max_wt) max_wt = processes[i].waiting_time;
    }

    printf("\nProcess Details:\n\n");

    for (int i = 0; i < n; i++) {
        char note[30] = "";

        if (processes[i].turnaround_time == min_tat)
            strcpy(note, "Best Turnaround Time");
        else if (processes[i].turnaround_time == max_tat)
            strcpy(note, "Worst Turnaround Time");

        if (processes[i].waiting_time == min_wt)
            strcpy(note, "Best Waiting Time");
        else if (processes[i].waiting_time == max_wt)
            strcpy(note, "Worst Waiting Time");

        printf("Process P%d\n", processes[i].pid);
        printf("  Queue: Q%d\n", processes[i].priority_queue);
        printf("  Burst Time: %d\n", processes[i].burst_time);
        printf("  Arrival Time: %d\n", processes[i].arrival_time);
        printf("  Completion Time: %d\n", processes[i].completion_time);
        printf("  Turnaround Time: %d\n", processes[i].turnaround_time);
        printf("  Waiting Time: %d\n", processes[i].waiting_time);

        if (strlen(note) > 0)
            printf("  Note: %s\n", note);

        printf("\n");
    }
    printf("\nAverage Turnaround Time: %.2f\n", total_turnaround / n);
    printf("Average Waiting Time: %.2f\n", total_waiting / n);
}

int main() {
    int number_of_processes;

    printf("Enter total number of processes: ");
    scanf("%d", &number_of_processes);

    if (number_of_processes <= 0) {
        printf("Invalid number of processes!\n");
        return 1;
    }

    srand(time(NULL)); //seed random number generator for burst time generation
    init_queues();

    Process processes[number_of_processes];
    int pid_count = 0;

    //Collect process details from the user
    for (int i = 0; i < number_of_processes; i++) {
        int burst_time, queue_num, arrival_time;

        printf("\n--- Process %d ---\n", i + 1);

        //Auto-assign PID based on entry order
        processes[i].pid = ++pid_count;
        printf("Auto-assigned PID: %d\n", processes[i].pid);

        //Randomly generate burst time between 1 and 10
        burst_time = (rand() % 10) + 1;
        printf("Auto-generated Burst Time: %d\n", burst_time);

        while (1) {
            printf("Enter arrival time: ");
            scanf("%d", &arrival_time);
            if (arrival_time >= 0) break;
            printf("Arrival time cannot be negative!\n");
        }

        printf("\nQueue Structure:\n");
        printf(" 0 -> Q0 (Highest): Round Robin (RR)\n");
        printf(" 1 -> Q1: Shortest Job First (SJF) - Preemptive\n");
        printf(" 2 -> Q2: Shortest Job First (SJF) - Non-Preemptive\n");
        printf(" 3 -> Q3 (Lowest): First-In-First-Out (FIFO)\n");

        while (1) {
            printf("Enter the queue number (0-3): ");
            scanf("%d", &queue_num);
            if (queue_num >= 0 && queue_num <= 3) break;
            printf("Invalid queue number! Must be 0-3.\n");
        }

        //initialize all process fields
        processes[i].burst_time = burst_time;
        processes[i].remaining_time = burst_time;
        processes[i].arrival_time = arrival_time;
        processes[i].priority_queue = queue_num;
        processes[i].is_completed = false;
        processes[i].is_enqueued = false;
        processes[i].completion_time = 0;
        processes[i].turnaround_time = 0;
        processes[i].waiting_time = 0;
        processes[i].next = NULL;
    }

    //Main scheduling loop, runs until all processes are completed
    while (!is_executing_completed(number_of_processes)) {

        //Add any processes that have arrived by the current time into their queues
        enqueue_arrived_processes(processes, number_of_processes);
        int q = find_highest_priority_queue();

        //If no processes are ready, advance time to the next arrival
        if (q == -1) {
            int next_arrival = INT_MAX;
            for (int i = 0; i < number_of_processes; i++) {
                if (!processes[i].is_completed && !processes[i].is_enqueued &&
                    processes[i].arrival_time > current_time) {
                    if (processes[i].arrival_time < next_arrival) {
                        next_arrival = processes[i].arrival_time;
                    }
                }
            }
            if (next_arrival != INT_MAX) {
                current_time = next_arrival;
                continue;
            }
            break;
        }

        //Give the selected queue up to TIME_QUANTUM units of CPU time
        int time_remaining = TIME_QUANTUM;

        while (time_remaining > 0 && queue_has_processes(q)) {
            int exec_time = 0;

            switch (q) {
                case 0:
                    exec_time = execute_rr(0, time_remaining);
                    break;
                case 1:
                    exec_time = execute_sjf_preemptive(1, time_remaining);
                    break;
                case 2:
                    //Non-preemptive will execute once and exit the inner loop immediately
                    exec_time = execute_sjf_nonpreemptive(2, time_remaining);
                    time_remaining -= exec_time;
                    goto quantum_done;
                case 3:
                    exec_time = execute_fifo(3, time_remaining);
                    break;
            }
            if (exec_time == 0) break;
            time_remaining -= exec_time;

            //check for newly arrived processes after each execution slice
            enqueue_arrived_processes(processes, number_of_processes);

            //If a higher priority queue now has processes, preempt the current queue
            int higher_q = find_highest_priority_queue();
            if (higher_q != -1 && higher_q < q) break;
        }
        quantum_done:;
    }
    calculate_metrics(processes, number_of_processes);
    return 0;
}