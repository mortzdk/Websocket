# TODO LIST
 1. Backwards Specification Compability
	 - hybi-06?
	 - hybi-05?
	 - hybi-04?
	 - hixie-76?
	 - hixie-75?
 2. Performance Improvements
     - Realloc the double size of the current
     - Refactor wss_frame_t structure away, that is use the frames as byte
       strings instead.
     - Callgrind
     - Cachegrind
 3. Automatical Documentation Creation
     - https://gist.github.com/francesco-romano/351a6ae457860c14ee7e907f2b0fc1a5
 4. Unit testing (INPROGRESS)
 5. Fuzz testing
 6. Fix TODO's in code
 7. Look at khash or tommy_hashdyn instead of uthash
 8. Rate limiting clients messages/frames
 9. Use Autoconf to check dependencies
 10. OpenCollective.com
 11. Test on FreeBSD/MacOS
 12. Support HTTP2
