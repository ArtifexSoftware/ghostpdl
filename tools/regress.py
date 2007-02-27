#/usr/bin/env python

import os, re
import sys, time

# results of tests are stored as classes

class TestResult:
  'generic test result class'
  def __init__(self, test=None, msg=None):
    self.test = test
    self.msg = None
  def __str__(self):
    return 'no result'

class OKResult(TestResult):
  'result class for successful tests'
  def __str__(self):
    return 'ok'

class FailResult(TestResult):
  'result class for failed tests'
  def __str__(self):
    return 'FAIL'

class ErrorResult(TestResult):
  'result class for tests that did not complete'
  def __str__(self):
    return 'ERROR'

class NewResult(TestResult):
  'result class for tests that are new and have no expected result'
  def __str__(self):
    return 'new'

class SelfTest:
  'generic class for self tests'
  def __init__(self):
    self.result = None
    self.msg = ''
  def description(self):
    'returns a short name for the test'
    return "generic self test"
  def run(self):
    'call this to execute the test'
    self.result = OKResult()

class SelfTestSuite:
  'generic class for running a collection of SelfTest instances'
  def __init__(self, stream=sys.stderr):
    self.stream = stream
    self.tests = []
    self.fails = []
  def addTest(self, test):
    self.tests.append(test)
  def run(self):
    starttime = time.time()
    for test in self.tests:
      self.stream.write("%s ... " % test.description())
      test.run()
      if not isinstance(test.result, OKResult):
        self.fails.append(test)
      self.stream.write("%s\n" % test.result)
    stoptime = time.time()
    self.stream.write('-'*72 + '\n')
    self.stream.write('ran %d tests in %.3f seconds\n\n' % 
	(len(self.tests), stoptime - starttime))
    if len(self.fails):
      self.stream.write('FAILED %d of %d tests\n' % 
	(len(self.fails),len(self.tests)))
    else:
      self.stream.write('PASSED all %d tests\n' % len(self.tests))

class lsTest(SelfTest):
  '''test class for running the language switch build
  pass in a database for comparison and a path to a file to check'''
  def __init__(self, db, file, dpi=300):
    SelfTest.__init__(self)
    self.db = db
    self.file = file
    self.dpi = dpi
    self.exe = './language_switch/obj/pspcl6'
    self.opts = "-dNOPAUSE -sDEVICE=ppmraw -r%d" % dpi
    self.psopts = '-dMaxBitmap=40000000 ./gs/lib/gs_cet.ps'
  def description(self):
    return 'Checking ' + self.file
  def run(self):
    scratch = os.path.basename(self.file) + '.md5'
    # add psopts if it's a postscript file
    if self.file[-3:].lower() == '.ps' or \
	self.file[-4:].lower() == '.eps':
      cmd = '%s %s -sOutputFile="|md5sum>%s" %s %s ' % \
	(self.exe, self.opts, scratch, self.psopts, self.file)
    else:
      cmd = '%s %s -sOutputFile="|md5sum>%s" %s' % \
	(self.exe, self.opts, scratch, self.file)
    run = os.popen(cmd)
    msg = run.readlines()
    code = run.close()
    if code:
      self.result = ErrorResult('\n'.join(msg))
      return
    checksum = open(scratch)
    md5sum = checksum.readline().split()[0]
    checksum.close()
    os.unlink(scratch)
    try:
      oldsum = self.db[self.file]
    except KeyError:
      self.db[self.file] = md5sum
      self.result = NewResult(md5sum)
      return
    if oldsum == md5sum:
      self.result = OKResult(md5sum)
    else:
      self.result = FailResult(md5sum)

def run_regression():
  'run normal set of regressions'
  from glob import glob
  import anydbm
  suite = SelfTestSuite()
  db = anydbm.open('reg_cache.db', 'c')
  testdir = '../tests/'
  tests = ['pcl/pcl5cfts/fts.*',
	'pcl/pcl5efts/fts.*', 
	'pcl/pcl5ccet/*.BIN',
	'ps/ps3cet/*.PS']
  for test in tests:
    for file in glob(testdir + test):
      suite.addTest(lsTest(db, file))
  suite.run()
  db.close()

# self test self tests

class RandomTest(SelfTest):
  'test class with random results for testing'
  def description(self):
    return 'random test result'
  def run(self):
    import random
    options = ( OKResult(), FailResult(), ErrorResult(), TestResult() )
    r = random.Random()
    r.seed()
    self.result = r.choice(options)

def test_ourselves():
  print 'testing a single test:'
  suite = SelfTestSuite()
  suite.addTest(SelfTest())
  suite.run()
  print 'testing a set of tests:'
  suite = SelfTestSuite()
  for count in range(8):
    suite.addTest(SelfTest())
  suite.run()
  print 'testing random results:'
  suite = SelfTestSuite()
  import random
  r = random.Random()
  r.seed()
  for count in range(4 + int(r.random()*12)):
    suite.addTest(RandomTest())
  suite.run()

if __name__ == '__main__':
  #test_ourselves()
  run_regression()
