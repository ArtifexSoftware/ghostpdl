#!/usr/bin/env python

# print a url's to stdout

from urllib import *
from urlparse import *

debug = 0

def error(msg):
	print sys.argv[0], "error: ", msg
        print "\t", sys.argv[0], "http://host[:port]/path"
        print "\t", sys.argv[0], "ftp://username:password@host/dir/file"
	print "\t", sys.argv[0], "file:/usr/dict/words"
	sys.exit(1)

try:
	for url in sys.argv[1:]:

		scheme, netloc, url, params, query, fragment = urlparse(url)
		if debug:
			print "scheme", "=", scheme
			print "netloc", "=",  netloc
			print "url", "=", url
			print "params", "=", params
			print "query", "=", query
			print "fragment", "=", fragment

except:
	error("all arguments must be urls")


for url in sys.argv[1:]:
	try:
		fn, h = urlretrieve(url)
	except:
		error("Couldn't retrive url")

	fp = open(fn, 'rb')
	sys.stdout.write(fp.read())
	fp.close()
