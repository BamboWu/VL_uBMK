#ifndef _BOOST_QLOCK_H__
#define _BOOST_QLOCK_H__  1

#ifdef __cplusplus
extern "C"
{
#endif

void *get_boost_qlock();
void boost_qlock_acquire(void *lock);
void boost_qlock_release(void *lock);

#ifdef __cplusplus
}
#endif

#endif
