#include "pnl_internal.h"

#if defined(_WIN32)

#include <windows.h>

typedef char pnl_lock_fits[sizeof(CRITICAL_SECTION) <= sizeof(union pnl_storage) ? 1 : -1];

static DWORD WINAPI pnl_thread_entry(LPVOID arg)
{
    struct pnl_thread *thread = arg;

    thread->fn(thread->arg);
    return 0;
}

int pnl_thread_start(struct pnl_thread *thread, void (*fn)(void *arg), void *arg)
{
    thread->fn = fn;
    thread->arg = arg;
    thread->handle = CreateThread(NULL, 0, pnl_thread_entry, thread, 0, NULL);
    return thread->handle != NULL ? PNL_OK : PNL_ERR_NOMEM;
}

void pnl_thread_join(struct pnl_thread *thread)
{
    WaitForSingleObject((HANDLE)thread->handle, INFINITE);
    CloseHandle((HANDLE)thread->handle);
}

int pnl_lock_init(struct pnl_lock *lock)
{
    CRITICAL_SECTION *cs = (CRITICAL_SECTION *)&lock->storage;

    InitializeCriticalSection(cs);
    return PNL_OK;
}

void pnl_lock_destroy(struct pnl_lock *lock)
{
    DeleteCriticalSection((CRITICAL_SECTION *)&lock->storage);
}

void pnl_lock(struct pnl_lock *lock)
{
    EnterCriticalSection((CRITICAL_SECTION *)&lock->storage);
}

void pnl_unlock(struct pnl_lock *lock)
{
    LeaveCriticalSection((CRITICAL_SECTION *)&lock->storage);
}

#else

#include <pthread.h>

typedef char pnl_lock_fits[sizeof(pthread_mutex_t) <= sizeof(union pnl_storage) ? 1 : -1];
typedef char pnl_thread_fits[sizeof(pthread_t) <= sizeof(union pnl_storage) ? 1 : -1];

static void *pnl_thread_entry(void *arg)
{
    struct pnl_thread *thread = arg;

    thread->fn(thread->arg);
    return NULL;
}

int pnl_thread_start(struct pnl_thread *thread, void (*fn)(void *arg), void *arg)
{
    pthread_t *handle = (pthread_t *)&thread->storage;

    thread->fn = fn;
    thread->arg = arg;
    return pthread_create(handle, NULL, pnl_thread_entry, thread) == 0 ? PNL_OK : PNL_ERR_NOMEM;
}

void pnl_thread_join(struct pnl_thread *thread)
{
    pthread_join(*(pthread_t *)&thread->storage, NULL);
}

int pnl_lock_init(struct pnl_lock *lock)
{
    return pthread_mutex_init((pthread_mutex_t *)&lock->storage, NULL) == 0 ? PNL_OK : PNL_ERR_NOMEM;
}

void pnl_lock_destroy(struct pnl_lock *lock)
{
    pthread_mutex_destroy((pthread_mutex_t *)&lock->storage);
}

void pnl_lock(struct pnl_lock *lock)
{
    pthread_mutex_lock((pthread_mutex_t *)&lock->storage);
}

void pnl_unlock(struct pnl_lock *lock)
{
    pthread_mutex_unlock((pthread_mutex_t *)&lock->storage);
}

#endif
