typedef void thread_callback(void *);
typedef void *thread_arg;

struct arg_wrapper {
	thread_callback *callback;
	thread_arg arg;
};


#ifdef _WIN64 // windows

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef HANDLE thread_handle;
typedef HANDLE event_handle;

DWORD WINAPI callback_wrapper(void *arg_)
{
	struct arg_wrapper *arg = arg_;
	arg->callback(arg->arg);
	return 0;
}

thread_handle create_thread(thread_callback *callback, thread_arg arg)
{
	struct arg_wrapper arg_ = {0};
	arg_.callback = callback;
	arg_.arg = arg;
	return CreateThread(0, 0, callback_wrapper, &arg_, 0, 0);
}

void wait_for_multiple_threads(thread_handle handles[], int count)
{
	WaitForMultipleObjects(count, handles, true, INFINITE);
}

void close_thread(thread_handle handle)
{
	CloseHandle(handle);
}

long interlocked_increment(long *i)
{
	return InterlockedIncrement(i);
}

event_handle create_event(void)
{
	return CreateEvent(NULL,  // default security attributes
	                   true,  // manual-reset event
	                   false, // initial state is nonsignaled
	                   NULL); // object name
}

void set_event(event_handle handle)
{
	SetEvent(handle);
}

void reset_event(event_handle handle)
{
	ResetEvent(handle);
}

void wait_for_event(event_handle handle)
{
	WaitForSingleObject(handle, INFINITE);
}

#define popen _popen
#define pclose _pclose

int platform_thread_count(void)
{
	SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
	return sysinfo.dwNumberOfProcessors;
}

#else // linux

#include <pthread.h>

typedef pthread_t thread_handle;

void *callback_wrapper(void *arg_)
{
	struct arg_wrapper *arg = arg_;
	arg->callback(arg->arg);
	free(arg_);
	pthread_exit(NULL);
}

thread_handle create_thread(thread_callback *callback, thread_arg arg)
{
	struct arg_wrapper *arg_ = malloc(sizeof(struct arg_wrapper));
	arg_->callback = callback;
	arg_->arg = arg;
	thread_handle handle;
	pthread_create(&handle, NULL, callback_wrapper, (void *)arg_);
	return handle;
}

void wait_for_multiple_threads(thread_handle handles[], int count)
{
	for (int i = 0; i < count; ++i)
		pthread_join(handles[i], NULL);
}

void close_thread(thread_handle handle)
{
	// should be done autmoatically
}

long interlocked_increment(long *i)
{
	static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
	pthread_mutex_lock(&lock);
	long result = ++*i;
	pthread_mutex_unlock(&lock);
	return result;
}

struct event_handle_base {
	bool triggered;
	pthread_mutex_t lock;
	pthread_cond_t cond;
};
typedef struct event_handle_base *event_handle;

event_handle create_event(void)
{
	event_handle handle = malloc(sizeof(struct event_handle_base));
	pthread_mutex_init(&handle->lock, NULL);
	pthread_cond_init(&handle->cond, NULL);
	handle->triggered = false;
	return handle;
}

void set_event(event_handle handle)
{
	pthread_mutex_lock(&handle->lock);
	handle->triggered = true;
	pthread_cond_broadcast(&handle->cond);
	pthread_mutex_unlock(&handle->lock);
}

void reset_event(event_handle handle)
{
	pthread_mutex_lock(&handle->lock);
	handle->triggered = false;
	pthread_mutex_unlock(&handle->lock);
}

void wait_for_event(event_handle handle)
{
	pthread_mutex_lock(&handle->lock);
	while (!handle->triggered)
		pthread_cond_wait(&handle->cond, &handle->lock);
	pthread_mutex_unlock(&handle->lock);
}

#include <unistd.h>
int platform_thread_count(void)
{
	return (int)sysconf(_SC_NPROCESSORS_ONLN);
}

#endif
