api_test: api_test.c
	(cd ../.. && ./autogen.sh)
	(cd ../.. && make so XCFLAGS="-DCLUSTER")
	pwd
	gcc -fPIC -I../.. api_test.c -DCLUSTER=1 -DGHOSTPDL=1 -lgpdl -L../../sobin -o api_test

run_api_test: api_test
	LD_LIBRARY_PATH=../../sobin ./api_test

post_api_test:
	md5sum apitest*
	rm apitest*

multi_test: multi_test.c
	(cd ../.. && ./autogen.sh)
	(cd ../.. && make so XCFLAGS="-DCLUSTER")
	pwd
	gcc -fPIC -I../.. multi_test.c -DCLUSTER=1 -DGHOSTPDL=1 -lgpdl -lpthread -L../../sobin -o multi_test

run_multi_test: multi_test
	LD_LIBRARY_PATH=../../sobin ./multi_test

post_multi_test:
	md5sum multitest*
	rm multitest*
	rm multi_out*
