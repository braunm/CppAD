// $Id$
/* --------------------------------------------------------------------------
CppAD: C++ Algorithmic Differentiation: Copyright (C) 2003-11 Bradley M. Bell

CppAD is distributed under multiple licenses. This distribution is under
the terms of the 
                    Common Public License Version 1.0.

A copy of this license is included in the COPYING file of this distribution.
Please visit http://www.coin-or.org/CppAD/ for information on other licenses.
-------------------------------------------------------------------------- */
/* 
$begin pthread_team.cpp$$
$spell
	Cygwin
	pthread
$$

$index pthread, AD team$$
$index AD, pthread team$$
$index team, AD pthread$$

$section Pthread Implementation of a Team of AD Threads$$
See $cref thread_team$$ for this routines specifications.

$head Bug in Cygwin$$
$index bug, cygwin pthread_exit$$
$index cygwin, bug in pthread_exit$$
$index pthread_exit, bug in cygwin$$ 
There is a bug in $code pthread_exit$$,
using cygwin 5.1 and g++ version 4.3.4,
whereby calling $code pthread_exit$$ is not the same as returning from
the corresponding routine.
To be specific, destructors for the vectors are not called
and a memory leaks result.
Set the following preprocessor symbol to 1 to demonstrate this bug:
$codep */
# define DEMONSTRATE_BUG_IN_CYGWIN 0
/* $$
$code
$verbatim%multi_thread/pthread/pthread_team.cpp%0%// BEGIN PROGRAM%// END PROGRAM%1%$$
$$

$end
*/
// BEGIN PROGRAM

# include <pthread.h>
# include <cppad/cppad.hpp>

# define MAX_NUMBER_THREADS        48

namespace {
	// number of threads in the team
	size_t num_threads_ = 1; 

	// type of the job currently being done by each thread
	enum thread_job_t { init_enum, work_enum, join_enum } thread_job_;

	// barrier used to wait for other threads to finish work
	pthread_barrier_t wait_for_work_;

	// barrier used to wait for master thread to set next job
	pthread_barrier_t wait_for_job_;

	// Are we in sequential mode; i.e., other threads are waiting for
	// master thread to set up next job ?
	bool sequential_execution_ = true;

	// structure with information for one thread
	typedef struct {
		// pthread unique identifier for thread that uses this struct
		pthread_t       pthread_id;
		// cppad unique identifier for thread that uses this struct
		size_t          thread_num;
		// true if no error for this thread, false otherwise.
		bool            ok;
	} thread_one_t;

	// vector with information for all threads
	thread_one_t thread_all_[MAX_NUMBER_THREADS];

	// pointer to function that does the work for one thread
	void (* worker_)(void) = 0;

	// ---------------------------------------------------------------------
	// in_parallel()
	bool in_parallel(void)
	{	return ! sequential_execution_; }

	// ---------------------------------------------------------------------
	// thread_number()
	size_t thread_number(void)
	{
		// pthread unique identifier for this thread
		pthread_t thread_this = pthread_self();

		// convert thread_this to the corresponding thread_num
		size_t thread_num = 0;
		for(thread_num = 0; thread_num < num_threads_; thread_num++)
		{	// pthread unique identifier for this thread_num
			pthread_t thread_compare = thread_all_[thread_num].pthread_id;

			// check for a match
			if( pthread_equal(thread_this, thread_compare) )
				return thread_num;
		}
		// no match error (thread_this is not in thread_all_).
		std::cerr << "thread_number: unknown pthread id" << std::endl;
		exit(1);

		return 0;
	}
	// --------------------------------------------------------------------
	// function that gets called by pthread_create
	void* thread_work(void* thread_one_vptr)
	{
		// thread management information for this thread
		thread_one_t* thread_one = 
			static_cast<thread_one_t*>(thread_one_vptr);

		// thread_num to problem specific information for this thread
		size_t thread_num = thread_one->thread_num;

		// In the special case where thread_job_ is join_enum,
		// there are no more calls to wait_for_work_ or wait_for_job_.
		while( thread_job_ != join_enum )
		{	switch( thread_job_ )
			{
				case init_enum:
				break;

				case work_enum:
				worker_();
				break;

				default:
				std::cerr << "thread_work: default case" << std::endl;
				exit(1);
			}
			// All threads make a call to wait_for_work_
			int rc = pthread_barrier_wait(&wait_for_work_);
			thread_all_[thread_num].ok &= 
				(rc == 0 || rc == PTHREAD_BARRIER_SERIAL_THREAD);

			// If this is the master, exit the loop.
			// Master thread must make a call to wait_for_job_ elsewhere. 
			if( thread_num == 0 )
				return 0;

			// All but the master thread make a call to wait_for_job_
			rc = pthread_barrier_wait(&wait_for_job_);
			thread_all_[thread_num].ok &= 
				(rc == 0 || rc == PTHREAD_BARRIER_SERIAL_THREAD);
		}
		// It this is not the master thread, then terminate it.
# if DEMONSTRATE_BUG_IN_CYGWIN
		if( ! pthread_equal(
			thread_one->pthread_id, thread_all_[0].pthread_id) )
		{	void* no_status = 0;
			pthread_exit(no_status);
		}
# endif
		// return null pointer
		return 0;
	}
}

bool start_team(size_t num_threads)
{	using CppAD::thread_alloc;
	bool ok = true;;

	if( num_threads > MAX_NUMBER_THREADS )
	{	std::cerr << "start_team: num_threads greater than ";
		std::cerr << MAX_NUMBER_THREADS << std::endl;
		exit(1);
	}
	// check that we currently do not have multiple threads running
	ok  = num_threads_ == 1;
	ok &= sequential_execution_;

	// Set the information for this thread so thread_number will work
	// for call to parallel_setup
	thread_all_[0].pthread_id = pthread_self();
	thread_all_[0].thread_num = 0;
	thread_all_[0].ok         = true;

	// Now that thread_number() has necessary information for the case
	// num_threads_ == 1, and while still in sequential mode,
	// call setup for using CppAD::AD<double> in parallel mode.
	thread_alloc::parallel_setup(num_threads, in_parallel, thread_number);
	CppAD::parallel_ad<double>();

	// now change num_threads_ to its final value.
	num_threads_ = num_threads;

	// initialize two barriers, one for work done, one for new job ready
	pthread_barrierattr_t *no_barrierattr = 0;
	int rc = pthread_barrier_init(
		&wait_for_work_, no_barrierattr, num_threads
	); 
	ok &= (rc == 0);
	rc  = pthread_barrier_init(
		&wait_for_job_, no_barrierattr, num_threads
	); 
	ok &= (rc == 0);
	
	// structure used to create the threads
	pthread_t      pthread_id;
	pthread_attr_t attr;
	void*          thread_one_vptr;
	//
	rc  = pthread_attr_init(&attr);
	ok &= (rc == 0);
	rc  = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	ok &= (rc == 0);

	// initial job for the threads
	thread_job_           = init_enum;
	if( num_threads > 1 )
		sequential_execution_ = false;

	// This master thread is already running, we need to create
	// num_threads - 1 more threads
	size_t thread_num;
	for(thread_num = 1; thread_num < num_threads; thread_num++)
	{	thread_all_[thread_num].ok         = true;
		thread_all_[thread_num].thread_num = thread_num;
		// Create the thread with thread number equal to thread_num
		thread_one_vptr = static_cast<void*> (&(thread_all_[thread_num]));
		rc = pthread_create(
				&pthread_id ,
				&attr       ,
				thread_work ,
				thread_one_vptr
		);
		thread_all_[thread_num].pthread_id = pthread_id;
		ok &= (rc == 0);
	}

	// Done creating threads and hence no longer need this attribute object
	rc  = pthread_attr_destroy(&attr);
	ok &= (rc == 0);

	//  do work using this thread and then wait
	//  until all threads have completed wait_for_work_
	thread_one_vptr = static_cast<void*> (&(thread_all_[0]));
	thread_work(thread_one_vptr);

	// Current state is all threads have completed wait_for_work_,
	// and are at wait_for_job_.
	// This master thread (thread zero) has not completed wait_for_job_
	sequential_execution_ = true;
	for(thread_num = 0; thread_num < num_threads; thread_num++)
		ok &= thread_all_[thread_num].ok;
	return ok;
}

bool work_team(void worker(void))
{
	// Current state is all threads have completed wait_for_work_,
	// and are at wait_for_job_.
	// This master thread (thread zero) has not completed wait_for_job_
	bool ok = sequential_execution_;
	ok     &= thread_number() == 0;

	// set global version of this work routine
	worker_ = worker;

	// reset wait_for_work_ barrier
	int rc = pthread_barrier_destroy(&wait_for_work_);
	ok    &= (rc == 0);
	pthread_barrierattr_t *no_barrierattr = 0;
	rc     = pthread_barrier_init(
		&wait_for_work_, no_barrierattr, num_threads_
	); 
	ok &= (rc == 0);

	// set the new job that other threads are waiting for
	thread_job_ = work_enum;

	// enter parallel exectuion soon as master thread completes wait_for_job_ 
	if( num_threads_ > 1 )
		sequential_execution_ = false;

	// wait until all threads have completed wait_for_job_
	rc  = pthread_barrier_wait(&wait_for_job_);
	ok &= (rc == 0 || rc == PTHREAD_BARRIER_SERIAL_THREAD);

	// reset wait_for_job_
	rc  = pthread_barrier_destroy(&wait_for_job_);
	ok &= (rc == 0);
	rc  = pthread_barrier_init(
		&wait_for_job_, no_barrierattr, num_threads_
	); 
	ok &= (rc == 0);

	// Now do the work in this thread and then wait
	// until all threads have completed wait_for_work_
	void* thread_one_vptr = static_cast<void*> (&(thread_all_[0]));
	thread_work(thread_one_vptr);

	// Current state is all threads have completed wait_for_work_,
	// and are at wait_for_job_.
	// This master thread (thread zero) has not completed wait_for_job_
	sequential_execution_ = true;

	size_t thread_num;
	for(thread_num = 0; thread_num < num_threads_; thread_num++)
		ok &= thread_all_[thread_num].ok;
	return ok;
}

bool stop_team(void)
{	// Current state is all threads have completed wait_for_work_,
	// and are at wait_for_job_.
	// This master thread (thread zero) has not completed wait_for_job_
	bool ok = sequential_execution_;
	ok     &= thread_number() == 0;

	// set the new job that other threads are waiting for
	thread_job_ = join_enum;

	// Enter parallel exectuion soon as master thread completes wait_for_job_ 
	if( num_threads_ > 1 )
			sequential_execution_ = false;
	int rc  = pthread_barrier_wait(&wait_for_job_);
	ok &= (rc == 0 || rc == PTHREAD_BARRIER_SERIAL_THREAD);

	// now wait for the other threads to be destroyed
	size_t thread_num;
	for(thread_num = 1; thread_num < num_threads_; thread_num++)
	{	void* no_status = 0;
		rc      = pthread_join(
			thread_all_[thread_num].pthread_id, &no_status
		);
		ok &= (rc == 0);
	}

	// now we are down to just the master thread (thread zero) 
	sequential_execution_ = true;

	// destroy wait_for_work_
	rc  = pthread_barrier_destroy(&wait_for_work_);
	ok &= (rc == 0);

	// destroy wait_for_job_
	rc  = pthread_barrier_destroy(&wait_for_job_);
	ok &= (rc == 0);

	// check ok before changing num_threads_
	for(thread_num = 0; thread_num < num_threads_; thread_num++)
		ok &= thread_all_[thread_num].ok;

	// now inform CppAD that there is only one thread
	num_threads_ = 1;
	using CppAD::thread_alloc;
	thread_alloc::parallel_setup(num_threads_, in_parallel, thread_number);

	return ok;
}
// END PROGRAM