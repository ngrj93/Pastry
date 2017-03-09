#ifndef SEMA_H_INCLUDED
#define SEMA_H_INCLUDED
#include <mutex>
#include <condition_variable>

using namespace std;

/* Semaphore class */

class Sema
{
	public:

	Sema (int ct = 0);
	void post ();  /* Semaphore up */
	void wait ();  /* Semaphore down */

	private:

	mutex mt;
	condition_variable cv;
	int ct;     	/* Semaphore count */
};

#endif
