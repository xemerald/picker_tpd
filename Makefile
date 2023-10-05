#
#
#

# Compile rule for Earthworm version under 7.9
#
main: libs echo_msg
	@(cd ./src; make -f makefile.unix;);

#
#
libs: echo_msg_libraries
	@(cd ./src/libsrc; make -f makefile.unix;);

#
#
echo_msg:
	@echo "------------------------------";
	@echo "- Making main program for EW -";
	@echo "------------------------------";
echo_msg_libraries:
	@echo "----------------------------------";
	@echo "-        Making libraries        -";
	@echo "----------------------------------";

# Clean-up rules
clean:
	@(cd ./src; make -f makefile.unix clean;);
	@(cd ./src/libsrc; make -f makefile.unix clean; make -f makefile.unix clean_lib;);

clean_bin:
	@(cd ./src; make -f makefile.unix clean_bin;);
