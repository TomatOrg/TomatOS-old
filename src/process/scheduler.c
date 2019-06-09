#include <buf.h>
#include <cpu/msr.h>
#include <string.h>
#include <interrupts/timer.h>
#include <cpu/fpu.h>
#include "scheduler.h"
#include "thread.h"
#include "process.h"

thread_t* running_thread;

static size_t i = 0;
static thread_t** threads;

static error_t do_context_switch(registers_t* regs, thread_t* new) {
    error_t err = NO_ERROR;

    // no need to load a new context if using the same address space
    if(running_thread == new) {
        goto cleanup;
    }

    // only do this if there was a running thread
    if(running_thread != NULL) {
        // save the state of the running thread
        running_thread->state.cpu = *regs;
        CHECK_AND_RETHROW(_fxsave(running_thread->state.fpu));

        // TODO: If switching to a different mode (user/supervisor) we will need to change the syscall registers

        running_thread->status = THREAD_STATUS_READY;
    }

    // load the new thread state
    *regs = new->state.cpu;
    CHECK_AND_RETHROW(_fxrstor(new->state.fpu));
    new->status = THREAD_STATUS_RUNNING;

    // set the new as the running thread
    running_thread = new;

cleanup:
    return err;
}

static error_t scheduler_timer(registers_t* regs) {
    error_t err = NO_ERROR;

    // choose new thread
    thread_t* new_thread = NULL;
    do {
        new_thread = threads[i % buf_len(threads)];
        i = (i + 1) % buf_len(threads);
    } while(new_thread->status != THREAD_STATUS_READY && new_thread->status != THREAD_STATUS_RUNNING);

    // do context switch
    CHECK_AND_RETHROW(do_context_switch(regs, new_thread));

cleanup:
    return err;
}

error_t scheduler_add(thread_t* thread) {
    // increment refcount
    thread->refcount++;

    for(size_t i = 0; i < buf_len(threads); i++) {
        if(threads[i] == NULL) {
            threads[i] = thread;
            goto cleanup;
        }
    }

    buf_push(threads, thread);

cleanup:
    return NO_ERROR;
}

error_t scheduler_init() {
    error_t err = NO_ERROR;

    log_info("Initializing scheduler");
    CHECK_AND_RETHROW(timer_add(scheduler_timer, 10));

cleanup:
    return err;
}