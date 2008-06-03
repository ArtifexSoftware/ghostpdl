#/usr/bin/env python

import os
import time
import sys

try:
  from mpi4py import MPI
except ImportError:
  class DummyMPI:
    '''A dummy MPI class for running serial jobs.'''
    size = 1
    rank = 0
  MPI = DummyMPI()

class Conf:
  def __init__(self):
    # set defaults
    self.batch = False
    self.update = False
    self.verbose = False
    self.testpath = os.environ['HOME']
    #self.exe = './language_switch/obj/pspcl6'
    self.exe = './bin/gs -q -I$HOME/fonts'
    self.device = 'ppmraw'
    self.dpi = 600

  def parse(self, args):
    '''Parse the command line for configuration switches

    For example:
      conf = Conf()
      conf.parse(sys.argv)
    '''

    for index in xrange(1,len(args)):
      arg = args[index]
      if arg[:2] == '--':

        # support generic '--opt=val'
        sep = arg.find('=')
        if sep > 0:
          opt = arg[2:sep]
          val = arg[sep+1:]
        else:
          opt = arg[2:]

          # for select options support '--opt val'
          if opt in ('exe', 'test'):
            try:
	      val = args[index+1]
            except IndexError:
	      print 'Warning:', opt, 'requires a specific value.'
	      val = None
          else:
	    # default to postitive boolean value
            val = True

        # for select options, accumulate the values
        if opt in ('test'):
          opt += 's' # pluralize collections
          if not hasattr(self, opt):
            self.__dict__[opt] = []
          self.__dict__[opt].append(val)
        else:
	  # set an attribute on ourselves with the option value
          self.__dict__[opt] = val
 
    # finally, set defaults for unset accumulating options
    if not hasattr(self, 'tests'):
      self.tests = []
      # guess appropriate defaults based on the executable
      basename = os.path.basename(self.exe.split()[0])
      if basename.find('pcl') >= 0:
	# public test suite
	self.tests += ['tests_public/pcl/*']
	# Quality Logic suites
        self.tests += ['tests_private/pcl/pcl5cfts/fts.*',
	'tests_private/pcl/pcl5efts/fts.*', 
	'tests_private/pcl/pcl5ccet/*.BIN',
        'test_private/xl/pcl6cet/*.bin',
        'test_private/xl/pxlfts3.0/*.BIN']
      if basename.find('pspcl') >= 0 or basename.find('gs') >= 0:
	# public test suite
	self.tests += ['tests_public/ps/*', 'tests_public/pdf/*']
	# the normal comparefiles suite
        self.tests += ['tests_private/comparefiles/*.ps', 
		'tests_private/comparefiles/*.pdf', 
		'tests_private/comparfiles/*.ai']
	# Quality Logic CED suite
	self.tests += ['tests_private/ps/ps3cet/*.PS']
      if basename.find('xps') >= 0:
	# Quality Logic suites
	self.tests += ['tests_private/xps/xpsfts-a4/*.xps',
		'tests_private/xps/atx/*.xps']

# global configuration instance
conf = Conf()
conf.parse(sys.argv)


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
  '''Generic class for running a collection of SelfTest instances.'''

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
    '''Run each test in sequence.'''
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
    if not self.fails and not self.errors and not self.news:
      print 'PASSED all %d tests' % len(self.tests)
    if self.news:
      print '%d NEW files with no previous result' % len(self.news)
    print

class MPITestSuite(SelfTestSuite):
  '''Use MPI to run multiple tests in parallel.'''

  def run(self):
    starttime = time.time()
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
        MPI.COMM_WORLD.Send(tests.pop(0), dest=status.source)
      # retrieve outstanding results and tell the nodes we're finished
      for node in xrange(1, MPI.size):
        test = MPI.COMM_WORLD.Recv(source=MPI.ANY_SOURCE)
	self.addResult(test)
        MPI.COMM_WORLD.Send(None, dest=node)
    stoptime = time.time()
    self.elapsed = stoptime - starttime
    if MPI.rank == 0:
      self.report()

# specific code for our needs

class md5Test(SelfTest):
  '''Test class for running a file and comparing the output to an
  expected value.'''

  def __init__(self, file, md5sum, dpi=600, device="ppmraw"):
    SelfTest.__init__(self)
    self.file = file
    self.md5sum = md5sum
    self.dpi = dpi
    self.exe = conf.exe
    self.opts = "-dQUIET -dSAFER -dNOPAUSE -dBATCH -K1000000"
    self.opts += " -dSAFER -dBATCH -dMaxBitmap=30000000"
    self.opts += " -Z:@"
    self.opts += " -sDEVICE=%s -r%d" % (device, dpi)
    self.psopts = '-dJOBSERVER'

  def description(self):
    return 'Checking ' + self.file

  def run(self):
    scratch = os.path.join('/tmp', os.path.basename(self.file) + '.md5')
    # add psopts if it's a postscript file
    if self.file[-3:].lower() == '.ps' or \
	self.file[-4:].lower() == '.eps' :
      cmd = '%s %s -sOutputFile="|md5sum>%s" %s - < %s ' % \
	(self.exe, self.opts, scratch, self.psopts, self.file)
    else:
      cmd = '%s %s -sOutputFile="|md5sum>%s" %s' % \
	(self.exe, self.opts, scratch, self.file)
    sys.stderr.write("Running: %s\n" % cmd)
    run = os.popen(cmd)
    msg = run.readlines()
    code = run.close()
    sys.stderr.write("Finished: %s\n" % cmd)
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

  # provide a dictionary interface
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
    for test in conf.tests:
      for file in glob(os.path.join(conf.testpath,test)):
        suite.addTest(md5Test(file, db[file], conf.dpi, conf.device))
    if MPI.size > 1 and not conf.batch:
      print 'running tests on %d nodes...' % MPI.size
  suite.run()
  if MPI.rank == 0:
    # update the database with new files and save
    for test in suite.news:
      db[test.file] = test.result.msg
    if conf.update:
      if len(suite.fails):
        print 'Updating baselines for the failed tests.'
      for test in suite.fails:
        db[test.file] = test.result.msg
    db.save()


# self test routines for the self test classes

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


# Do someting useful when executed directly

if __name__ == '__main__':
  #test_ourselves()
  run_regression()
