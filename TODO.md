# TODO LIST

 1. Implement subprotocols 
	 - Broadcast subprotocol (Not in registry, MPSC ringbuffer on EPOLL_CTL_MOD)
 2. Memory Allocator 
	 - Use another memory allocator to increase performance ([rpmalloc](https://github.com/mjansson/rpmalloc/))
       * Might not be possible since all memory must be allocated using rpmalloc for extensions and subprotocols
 3. Backwards Specification Compability
	 - hybi-10
	 - hybi-07
	 - hixie-76
	 - hixie-75
	 - Others?
 4. Performance Improvements
     - Realloc the double size of the current
     - Callgrind
     - Cachegrind
 5. Automatical Documentation Creation
     - https://gist.github.com/francesco-romano/351a6ae457860c14ee7e907f2b0fc1a5
 6. Client authentication
 7. Fuzz testing
 8. Fragmentation on outgoing data
 9. Migrate to C18? Fallback to POSIX
 10. Fix TODO's in code
 11. Look at khash instead of uthash
