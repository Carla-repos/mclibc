
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define STACK_SIZE (64 * 1024)

__asm__(
    ".text\n"
    ".globl ctx_switch\n"
    "ctx_switch:\n"
    "    pushq %rbp\n"
    "    pushq %rbx\n"
    "    pushq %r12\n"
    "    pushq %r13\n"
    "    pushq %r14\n"
    "    pushq %r15\n"
    "    movq %rsp, (%rdi)\n"
    "    movq %rsi, %rsp\n"
    "    popq %r15\n"
    "    popq %r14\n"
    "    popq %r13\n"
    "    popq %r12\n"
    "    popq %rbx\n"
    "    popq %rbp\n"
    "    ret\n"
);

void ctx_switch(void **save_sp, void *load_sp);

typedef enum {
    RUNNABLE,
    RUNNING,
    WAITING,
    DEAD
} State;

typedef struct Goroutine {
    void *sp;
    char *stack;

    void (*fn)(void *);
    void *arg;

    State state;

    struct Goroutine *next;
} Goroutine;

typedef struct {
    Goroutine *head;
    Goroutine *tail;
} Queue;

static Queue runq;

static void *sched_sp;
static Goroutine *current;

static int live = 0;

static void push(Queue *q, Goroutine *g) {
    g->next = NULL;

    if (q->tail)
        q->tail->next = g;
    else
        q->head = g;

    q->tail = g;
}

static Goroutine *pop(Queue *q) {
    Goroutine *g = q->head;

    if (!g)
        return NULL;

    q->head = g->next;

    if (!q->head)
        q->tail = NULL;

    return g;
}

static void ready(Goroutine *g) {
    g->state = RUNNABLE;
    push(&runq, g);
}

static void switch_to_sched(void) {
    ctx_switch(&current->sp, sched_sp);
}

void yield(void) {
    ready(current);
    switch_to_sched();
}

static void g_entry(void) {
    current->fn(current->arg);
    current->state = DEAD;
    live--;

    switch_to_sched();
    abort();
}

void go(void (*fn)(void *), void *arg) {
    Goroutine *g = calloc(1, sizeof(Goroutine));

    g->fn = fn;
    g->arg = arg;

    g->stack = malloc(STACK_SIZE);

    uintptr_t top = ((uintptr_t)g->stack + STACK_SIZE) & ~(uintptr_t)15;

    void **sp = (void **)top;

    *--sp = NULL;
    *--sp = (void *)g_entry;

    for (int i = 0; i < 6; i++)
        *--sp = NULL;

    g->sp = sp;

    live++;

    ready(g);
}

void sched_run(void) {
    Goroutine *g;

    while ((g = pop(&runq))) {
        current = g;
        g->state = RUNNING;

        ctx_switch(&sched_sp, g->sp);

        if (g->state == DEAD) {
            free(g->stack);
            free(g);
        }
    }

    if (live > 0) {
        fprintf(stderr, "deadlock!\n");
        exit(1);
    }
}

void routine(void *arg) {
    const char *name = arg;

    int counter = 0;
    while (counter < 3) {
        printf("[%s] counter = %d\n", name, counter);
        counter++;
        yield();
    }

    printf("[%s] finished\n", name);
}

int main(void) {
    go(routine, "Alice");
    go(routine, "Bob");

    sched_run();

    printf("[main] all routines finished\n");

    return 0;
}
