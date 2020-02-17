# TODO LIST

 1. Implement subprotocols 
	 - Broadcast subprotocol (Not in registry)
	 - WAMP (MPSC ringbuffer on EPOLL_CTL_MOD)
 2. Cleanup Sessions Thread 
	 - Have cleanup thread, that occationally scans through connection list, pings if no activity within certain time and closes those that do not respond with a pong, within a certain time.
 3. Implement extensions 
	 - Per-Message Deflate
	 - bbf-usp-protocol
 4. Memory Allocator 
	 - Use another memory allocator to increase performance ([rpmalloc](https://github.com/mjansson/rpmalloc/))
 5. Backwards Specification Compability
	 - hybi-10
	 - hybi-07
	 - hixie-76
	 - hixie-75
	 - Others?
 6. OS Compability
	 - Linux
	 - MacOS (kqueue)
 7. Performance Improvements
 8. Automatical Documentation Creation
     - https://gist.github.com/francesco-romano/351a6ae457860c14ee7e907f2b0fc1a5
 9. Client authentication
 10. Fuzz testing
 11. Fragmentation on outgoing data
