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
$begin pthread_simple_ad.cpp$$
$spell
	Cygwin
	pthread
	pthreads
	CppAD
$$

$section Simple Pthread AD: Example and Test$$

$index pthread, simple AD$$
$index AD, simple pthread$$
$index simple, AD pthread$$
$index thread, pthread simple AD$$

$head Purpose$$
This example demonstrates how CppAD can be used with multiple pthreads.

$head Discussion$$
The function $code arc_tan$$ below
is an implementation of the inverse tangent function where the
$cref/operation sequence/glossary/Operation/Sequence/$$ depends on the
argument values. 
Hence the function needs to be re-taped 
for different argument values.
The $cref/atan2/$$ function uses $cref/CondExp/$$ operations
to avoid the need to re-tape.

$head pthread_exit Bug in Cygwin$$
$index bug, cygwin pthread_exit$$
$index cygwin, bug in pthread_exit$$
$index pthread_exit, bug in cygwin$$ 
There is a bug in $code pthread_exit$$,
using cygwin 5.1 and g++ version 4.3.4,
whereby calling $code pthread_exit$$ is not the same as returning from
the corresponding routine.
To be specific, destructors for the vectors are not called
and a memory leaks result.
Search for $code pthread_exit$$ in the source code below to
see how to demonstrate this bug.

$head Source Code$$
$code
$verbatim%multi_thread/pthread/simple_ad.cpp%0%// BEGIN PROGRAM%// END PROGRAM%1%$$
$$

$end
------------------------------------------------------------------------------
*/
// BEGIN PROGRAM
# include <pthread.h>
# include <cppad/cppad.hpp>
# include "../arc_tan.hpp"

# define NUMBER_THREADS            4
# define DEMONSTRATE_BUG_IN_CYGWIN 0

namespace {
	// ===================================================================
	// General purpose code for linking CppAD to pthreads
	// -------------------------------------------------------------------
	// alternative name for NUMBER_THREADS
	size_t number_threads = NUMBER_THREADS; 

	// The master thread switches the value of this variable
	static bool multiple_threads_may_be_active = false;

	// Barrier used to wait for all thread identifiers to be set
	pthread_barrier_t wait_for_thread_id;

	// general purpose vector with information for each thread
	typedef struct {
		// pthread unique identifier for thread that uses this struct
		pthread_t       thread_id;
		// cppad unique identifier for thread that uses this struct
		size_t          thread_index;
		// true if no error for this thread, false otherwise.
		bool            ok;
	} thread_info_t;
	thread_info_t thread_vector[NUMBER_THREADS];

	// ---------------------------------------------------------------------
	// in_parallel()
	bool in_parallel(void)
	{	return multiple_threads_may_be_active; }

	// ---------------------------------------------------------------------
	// thread_num()
	size_t thread_num(void)
	{
		// pthread unique identifier for this thread
		pthread_t thread_this = pthread_self();

		// convert thread_this to the corresponding thread index
		size_t index = 0;
		for(index = 0; index < number_threads; index++)
		{	// pthread unique identifier for this index
			pthread_t thread_compare = thread_vector[index].thread_id;

			// check for a match
			if( pthread_equal(thread_this, thread_compare) )
				return index;
		}
		// no match error (thread_this is not in thread_vector).
		assert(false);
		return number_threads;
	}
	// ====================================================================
	// code for specific problem we are solving
	// --------------------------------------------------------------------
	using CppAD::AD;

	// vector with specific information for each thread
	typedef struct {
		// angle for this work 
		double          theta;
		// False if error related to this work, true otherwise.
		bool            ok;
	} work_info_t;
	work_info_t work_vector[NUMBER_THREADS];
	// --------------------------------------------------------------------
	// function that does the work for each thread
	void* thread_work(void* thread_info_vptr)
	{	using CppAD::NearEqual;
		// start as true
		bool ok = true;

		// ----------------------------------------------------------------
		// Setup for this thread

		// general purpose information for this thread
		thread_info_t* thread_info = 
			static_cast<thread_info_t*>(thread_info_vptr);

		// index to problem specific information for this thread
		size_t index = thread_info->thread_index;

		// Set pthread unique identifier for this thread
		thread_vector[index].thread_id = pthread_self();

		// Wait for other threads to do the same.
		int rc = pthread_barrier_wait(&wait_for_thread_id);
		ok    &= (rc == 0 || rc == PTHREAD_BARRIER_SERIAL_THREAD);
		// ----------------------------------------------------------------
		// Work for this thread
		// (note that work[index] is only used by this thread)

		// CppAD::vector uses the CppAD fast multi-threading allocator
		CppAD::vector< AD<double> > Theta(1), Z(1);
		Theta[0] = work_vector[index].theta;
		Independent(Theta);
		AD<double> x = cos(Theta[0]);
		AD<double> y = sin(Theta[0]);
		Z[0]  = arc_tan( x, y );
		CppAD::ADFun<double> f(Theta, Z); 

		// Check function value corresponds to the identity 
		// (must get a lock before changing thread_info).
		double eps = 10. * CppAD::epsilon<double>();
		ok        &= NearEqual(Z[0], Theta[0], eps, eps);

		// Check derivative value corresponds to the identity.
		CppAD::vector<double> d_theta(1), d_z(1);
		d_theta[0] = 1.;
		d_z        = f.Forward(1, d_theta);
		ok        &= NearEqual(d_z[0], 1., eps, eps);

		// pass back ok information for this thread
		work_vector[index].ok = ok;

		// It this is not the master thread, then terminate it.
# if DEMONSTRATE_BUG_IN_CYGWIN
		if( ! pthread_equal(
			thread_info->thread_id, thread_vector[0].thread_id) )
		{	void* no_status = 0;
			pthread_exit(no_status);
		}
# endif
	
		// return null pointer
		return 0;
	}
}

// This test routine is only called by the master thread (index = 0).
bool simple_ad(void)
{	bool all_ok = true;
	using CppAD::thread_alloc;

	// Check that no memory is in use or avialable at start
	// (using thread_alloc in sequential mode)
	size_t index;
	for(index = 0; index < number_threads; index++)
	{	all_ok &= thread_alloc::inuse(index) == 0; 
		all_ok &= thread_alloc::available(index) == 0; 
	}

	// initialize the thread_vector and work_vector
	int    rc;
 	double pi = 4. * atan(1.);
	for(index = 0; index < number_threads; index++)
	{	// thread_id (initialze all as same as master thread)
		thread_vector[index].thread_id    = pthread_self();
		// thread_index
		thread_vector[index].thread_index = index;
		// ok
		thread_vector[index].ok           = true;
		work_vector[index].ok             = false;
		// theta 
		work_vector[index].theta          = index * pi / number_threads;
	}

	// Setup for using AD<double> in parallel mode
	thread_alloc::parallel_setup(number_threads, in_parallel, thread_num);
	CppAD::parallel_ad<double>();

	// Now change in_parallel() to return true.
	multiple_threads_may_be_active = true;

	// initialize barrier as waiting for number_threads
	pthread_barrierattr_t *no_barrierattr = 0;
	rc = pthread_barrier_init(
		&wait_for_thread_id, no_barrierattr, number_threads); 
	all_ok &= (rc == 0);
	
	// structure used to cread the threads
	pthread_t      thread[NUMBER_THREADS];
	pthread_attr_t attr;
	void*          thread_info_vptr;
	//
	rc      = pthread_attr_init(&attr);
	all_ok &= (rc == 0);
	rc      = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	all_ok &= (rc == 0);

	// This master thread is already running, we need to create
	// number_threads - 1 more threads
	for(index = 1; index < number_threads; index++)
	{
		// Create the thread with thread_num equal to index
		thread_info_vptr = static_cast<void*> ( &(thread_vector[index]) );
		rc = pthread_create(
				&thread[index] , 	
				&attr              ,
				thread_work        ,
				thread_info_vptr
		);
		all_ok &= (rc == 0);
	}

	// Now, while other threads are working, do work in master thread also
	thread_info_vptr = static_cast<void*> ( &(thread_vector[0]) );
	thread_work(thread_info_vptr);

	// now wait for the other threads to finish
	for(index = 1; index < number_threads; index++)
	{	void* no_status = 0;
		rc      = pthread_join(thread[index], &no_status);
		all_ok &= (rc == 0);
	}

	// Tell thread_alloc that we are in sequential execution mode.
	multiple_threads_may_be_active = false;

	// Summarize results.
	for(index = 0; index < number_threads; index++)
	{	all_ok &= thread_vector[index].ok;
		all_ok &= work_vector[index].ok;
	}

	// Free up the pthread resources that are no longer in use.
	rc      = pthread_attr_destroy(&attr);
	all_ok &= (rc == 0);
	rc      = pthread_barrier_destroy(&wait_for_thread_id);
	all_ok &= (rc == 0);

	// Check that no memory currently in use, and free avialable memory.
	for(index = 0; index < number_threads; index++)
	{	all_ok &= thread_alloc::inuse(index) == 0; 
		thread_alloc::free_available(index); 
	}

	// return memory allocator to single threading mode
	number_threads = 1;
	thread_alloc::parallel_setup(number_threads, in_parallel, thread_num);

	return all_ok;
}
// END PROGRAM