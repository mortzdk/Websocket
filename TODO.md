# TODO LIST

 1. Implement subprotocols 
	 - Broadcast subprotocol (Not in registry, MPSC ringbuffer on EPOLL_CTL_MOD)
	 - WAMP (MPSC ringbuffer on EPOLL_CTL_MOD)
 2. Cleanup Sessions Thread 
	 - Have cleanup thread, that occationally scans through connection list, pings if no activity within certain time and closes those that do not respond with a pong, within a certain time.
 3. Memory Allocator 
	 - Use another memory allocator to increase performance ([rpmalloc](https://github.com/mjansson/rpmalloc/))
 4. Backwards Specification Compability
	 - hybi-10
	 - hybi-07
	 - hixie-76
	 - hixie-75
	 - Others?
 5. OS Compability
	 - Linux
	 - MacOS (kqueue)
        * https://stackoverflow.com/questions/51793399/how-do-special-epoll-flags-correspond-to-kqueue-ones
 6. Performance Improvements
 7. Automatical Documentation Creation
     - https://gist.github.com/francesco-romano/351a6ae457860c14ee7e907f2b0fc1a5
 8. Client authentication
 9. Fuzz testing
 10. Fragmentation on outgoing data
 11. Migrate to C18? Fallback to POSIX
