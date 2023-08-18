#pragma once 

#ifndef WSS_EVENT_POLL_H
#define WSS_EVENT_POLL_H

#include "event.h"

#ifdef WSS_POLL

#ifndef POLLRDHUP
#define POLLRDHUP  0x2000
#endif

#include <poll.h>
#include <sys/resource.h>
#include <unistd.h>

#endif // WSS_POLL
#endif // WSS_EVENT_POLL_H
