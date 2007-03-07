#/usr/bin/env python

import os
import time
import sys

try:
  from mpi4py import MPI
except ImportError:
  class DummyMPI:
    'a dummy MPI class for conditionals in a serial job'
    size = 1
    rank = 0
  MPI = DummyMPI()

class Conf:
  'class for holding various configuration parameters'
  # output format, set to False for interactive
  batch = False
  # should we update the md5sum database to new values?
  update = True
conf = Conf()

# results of tests are stored as classes

class TestResult:
  'generic test result class'
  def __init__(self, msg=None):
    self.msg = msg
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
    return 'new (%s)' % self.msg

class SelfTest:
  'generic class for self tests'
  def __init__(self):
    self.result = None
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
    self.errors = []
    self.news = []
    self.elapsed = 0.0
  def addTest(self, test):
    self.tests.append(test)
  def addResult(self, test):
    if test:
      if not conf.batch:
        print test.description() + ' ... ' + str(test.result)
      self.tests.append(test)
      if isinstance(test.result, ErrorResult):
        self.errors.append(test)
      elif isinstance(test.result, NewResult):
        self.news.append(test)
      elif not isinstance(test.result, OKResult):
        # treat everything else as a failure
        self.fails.append(test)
  def run(self):
    'run each test in sequence'
    starttime = time.time()
    tests = self.tests
    self.tests = []
    for test in tests:
      test.run()
      self.addResult(test)
    self.elapsed = time.time() - starttime
    self.report()
  def report(self):
    if not conf.batch:
      print '-'*72
    print 'ran %d tests in %.3f seconds on %d nodes\n' % \
	(len(self.tests), self.elapsed, MPI.size)
    if self.fails:
      print 'FAILED %d of %d tests' % \
	(len(self.fails),len(self.tests))
      if conf.batch:
        for test in self.fails:
          print '  ' + test.file
        print
    if self.errors:
      print 'ERROR running %d of %d tests' % \
	(len(self.errors),len(self.tests))
      if conf.batch:
        for test in self.errors:
          print '  ' + test.description()
          print test.result.msg
        print
    if not self.fails and not self.errors:
      print 'PASSED all %d tests' % len(self.tests)
    if self.news:
      print '%d NEW files with no previous result' % len(self.news)
    print

class MPITestSuite(SelfTestSuite):
  def run(self):
    '''use MPI to dispatch tests to worker nodes
    each of which will run the run the actual tests
    and return the result'''
    starttime = time.time()
    # broadcast the accumulated tests to all nodes
    self = MPI.COMM_WORLD.Bcast(self, 0)
    if MPI.rank > 0:
      # daughter nodes run requested tests
      test = None
      while True:
        MPI.COMM_WORLD.Send(test, dest=0)
        test = MPI.COMM_WORLD.Recv(source=0)
        if not test:
          break
        test.run()
    else:
      # mother node hands out work and reports
      tests = self.tests
      self.tests = []
      while tests:
        status = MPI.Status()
        test = MPI.COMM_WORLD.Recv(source=MPI.ANY_SOURCE, status=status)
        self.addResult(test)
        MPI.COMM_WORLD.Send(tests.pop(), dest=status.source)
      # retrieve outstanding results and tell the nodes we're finished
      for node in xrange(1, MPI.size):
        test = MPI.COMM_WORLD.Recv(source=MPI.ANY_SOURCE)
	self.addResult(test)
        MPI.COMM_WORLD.Send(None, dest=node)
    stoptime = time.time()
    self.elapsed = stoptime - starttime
    if MPI.rank == 0:
      self.report()

class md5Test(SelfTest):
  '''test class for running a file and comparing the output to an
  expected value.'''
  def __init__(self, file, md5sum, dpi=300):
    SelfTest.__init__(self)
    self.file = file
    self.md5sum = md5sum
    self.dpi = dpi
    self.exe = './language_switch/obj/pspcl6'
    #self.exe = './bin/gs -q -I../fonts'
    self.opts = "-dNOPAUSE -sDEVICE=ppmraw -r%d" % dpi
    self.opts += " -dSAFER -dBATCH"
    self.psopts = '-dMaxBitmap=40000000 -dJOBSERVER ./lib/gs_cet.ps'
  def description(self):
    return 'Checking ' + self.file
  def run(self):
    scratch = os.path.join('/tmp', os.path.basename(self.file) + '.md5')
    # add psopts if it's a postscript file
    if self.file[-3:].lower() == '.ps' or \
	self.file[-4:].lower() == '.eps':
      cmd = '%s %s -sOutputFile="|md5sum>%s" %s - < %s ' % \
	(self.exe, self.opts, scratch, self.psopts, self.file)
    else:
      cmd = '%s %s -sOutputFile="|md5sum>%s" %s' % \
	(self.exe, self.opts, scratch, self.file)
    run = os.popen(cmd)
    msg = run.readlines()
    code = run.close()
    if code:
      self.result = ErrorResult(''.join(msg))
      return
    try:
      checksum = open(scratch)
      md5sum = checksum.readline().split()[0]
      checksum.close()
      os.unlink(scratch)
    except IOError:
      self.result = ErrorResult('no output')
      return
    if not self.md5sum:
      self.result = NewResult(md5sum)
      return
    if self.md5sum == md5sum:
      self.result = OKResult(md5sum)
    else:
      self.result = FailResult(md5sum)

class DB:
  '''class representing an md5 sum database'''
  def __init__(self):
    self.store = None
    self.db = {}
  def load(self, store='reg_baseline.txt'):
    self.store = store
    try:
      f = open(self.store)
    except IOError:
      print 'WARNING: could not open baseline database', self.store
      return
    for line in f.readlines():
      if line[:1] == '#': continue
      fields = line.split()
      try:
        file = fields[0].strip()
        md5sum = fields[1].strip()
        self.db[file] = md5sum
      except IndexError:
        pass
    f.close()
  def save(self, store=None):
    if not store:
      store = self.store
    f = open(store, 'w')
    f.write('# regression test baseline\n')
    for key in self.db.keys():
      f.write(str(key) + ' ' + str(self.db[key]) + '\n')
    f.close()
  def __getitem__(self, key):
    try:
      value = self.db[key]
    except KeyError:
      value = None
    return value
  def __setitem__(self, key, value):
    self.db[key] = value

def run_regression():
  'run normal set of regressions'
  from glob import glob
  if MPI.size > 1:
    suite = MPITestSuite()
  else:
    suite = SelfTestSuite()
  if MPI.rank == 0:
    db = DB()
    db.load()
    testdir = '../tests/'
    tests = ['pcl/pcl5cfts/fts.*',
	'pcl/pcl5efts/fts.*', 
	'pcl/pcl5ccet/*.BIN',
	'ps/ps3cet/*.PS']
    pstests = ['ps/ps3fts/*.ps','ps/ps3cet/*.PS']
    for test in tests:
      for file in glob(testdir + test):
        suite.addTest(md5Test(file, db[file]))
    if MPI.size > 1:
      print('running tests on %d nodes...' % MPI.size)
  suite.run()
  if MPI.rank == 0:
    # update the database with new files and save
    for test in suite.news:
      db[test.file] = test.result.msg
    if conf.update:
      for test in suite.fails:
        db[test.file] = test.result.msg
    db.save()


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
