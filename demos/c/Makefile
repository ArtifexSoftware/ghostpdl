api_test: api_test.c
	(cd ../.. && ./autogen.sh)
	(cd ../.. && make so)
	pwd
	gcc -fPIC -I../.. api_test.c -DGHOSTPDL=1 -lgpdl -L../../sobin -o api_test

run_api_test: api_test
	LD_LIBRARY_PATH=../../sobin ./api_test

post_api_test:
	md5sum apitest*
	rm apitest*
