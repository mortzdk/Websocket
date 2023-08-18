#pragma once 

#ifndef WSS_EVENT_EPOLL_H
#define WSS_EVENT_EPOLL_H

#include "event.h"

#ifdef WSS_KQUEUE

#include <sys/event.h>
#include <sys/time.h>

#endif // WSS_KQUEUE
#endif // WSS_EVENT_KQUEUE_H
