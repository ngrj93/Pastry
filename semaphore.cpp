#include "semaphore.h"

using namespace std;

Sema::Sema (int ct)
{
	this->ct = ct;
}

void 
Sema::post ()
{
	unique_lock<mutex> ulock (mt);
	ct++;
	cv.notify_one();
}

void
Sema::wait ()
{
	unique_lock<mutex> ulock (mt);
	while (ct == 0)
		cv.wait (ulock);
	ct--;
}

