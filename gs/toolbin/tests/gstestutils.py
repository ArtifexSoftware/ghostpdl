#    Copyright (C) 2002 Aladdin Enterprises.  All rights reserved.
# 
# This file is part of AFPL Ghostscript.
# 
# AFPL Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author or
# distributor accepts any responsibility for the consequences of using it, or
# for whether it serves any particular purpose or works at all, unless he or
# she says so in writing.  Refer to the Aladdin Free Public License (the
# "License") for full details.
# 
# Every copy of AFPL Ghostscript must include a copy of the License, normally
# in a plain ASCII text file named PUBLIC.  The License grants you the right
# to copy, modify and redistribute AFPL Ghostscript, but only under certain
# conditions described in the License.  Among other things, the License
# requires that the copyright notice and this notice be preserved on all
# copies.

# $Id$

# Utilities and documentation for Ghostscript testing using the Python
# 'unittest' framework.

# ------------------------ Documentation ------------------------ #

# Ghostscript provides a framework for writing test scripts that can be run
# either individually or as part of a regression test suite.  The purpose of
# this documentation is to help you write new tests within this framework.

# This framework is based on the Python 'unittest' framework, documented at
#	http://www.python.org/doc/current/lib/module-unittest.html

# Each test (or set of closely related tests) is in a separate file named
# check_xxx.py (if it does not run Ghostscript) or gscheck_xxx.py (if it
# does run Ghostscript).  Each test file must include the statement
#
#	from gstestutils import GSTestCase, gsRunTestsMain
#
# The main body of the file must define a 'test case' class derived from
# GSTestCase, e.g.,
#
#	class GSCheckGravity(GSTestCase):
#
# If instances of the test case require any additional parameters (e.g.,
# a directory name), the class must define an __init__ method that stores
# such parameters in instance variables, e.g.,
#
#	def __init__(self, planet):
#	    self.planet = planet
#	    GSTestCase.__init__(self)
#
# The last line above is required as the last line of any such __init__
# method.
#
# The test case class must provide a one-line description of its function.
# This description will be printed as part of any failure message.  If the
# description does not depend on any parameters, it can be included as a
# literal string as the first thing in the runTest method, e.g.,
#
#	def runTest(self):
#	    """The law of gravity must be operating."""
#
# If the description does depend on parameters provided to __init__, a
# separate shortDescription method is required, e.g.,
#
#	def shortDescription(self):
#	    return "Planet %s must have gravity." % self.planet
#
# The main body of the test must be a method called runTest.  To indicate
# a failure, it can call any of the self.failxxx messages defined for class
# unittest.TestCase.  The failure message can be either a string (possibly
# including embedded \n characters) or a list of strings, which will be
# printed on individual lines.  One useful method added in GSTestCase is
# failIfMessages: this takes a list of strings as a parameter, and fails
# if the list is not empty.  You can use it like this:
#
#	def runTest(self):
#	    messages = []
#	    if not self.feathersFall():
#		messages.append("Feathers don't fall.")
#	    if not self.leadWeightsFall():
#		messages.append("Lead weights don't fall.")
#	    self.failIfMessages(messages)
#
# At the end of the file there must be an addTests function (not a class
# method) that adds all the tests defined in the file to a suite, e.g.,
#
#	def addTests(suite, **args):
#	    for planet in ['Earth', 'Mars', 'Dune']:
#		suite.addTest(GSCheckGravity(planet))
#
# addTests may take additional parameters that a test harness will provide,
# which may be either required or optional.  In particular, any test that
# examines the Ghostscript source code must take a parameter named gsroot
# that is the prefix (not the directory) for the top-level Ghostscript
# directory.  The way to define addTests in this case is
#
#	def addTests(suite, gsroot, **args):
#
# The very last thing in the file must be the following lines to allow the
# file to be run stand-alone:
#
#	if __name__ == "__main__":
#	    gsRunTestsMain(addTests)
#
# Among its other functions, gsRunTestsMain parses the command line and
# provides default argument values: see its documentation below.

# ------------------------ End documentation ------------------------ #

import string, types, unittest
# gsRunTestsMain() imports additional modules, see below

# Define the exception that Ghostscript tests will raise when a failure
# occurs.  The result class will recognize this exception and print only
# the failure message rather than a traceback.

class GSTestFailure(AssertionError):
    pass

# Define a TestCase subclass that raises GSTestFailure for failures,
# and provides a method that fails iff an accumulated message string
# is not empty.

class GSTestCase(unittest.TestCase):

    def __init__(self):
        unittest.TestCase.__init__(self)

    failureException = GSTestFailure

    def failIfMessages(self, messages):
        self.failIf(len(messages) > 0, messages)

# Define a TestResult class that recognizes the GSTestFailure exception
# as described above, and a TestRunner class that uses it.
# The TestResult class also accepts a list or tuple of strings as the
# error message.

class _GSTextTestResult(unittest._TextTestResult):

    def printErrorList(self, flavor, errors):
        handoff = []
        for test, err in errors:
            if issubclass(err[0], GSTestFailure):
                self.stream.writeln(self.separator1)
                self.stream.writeln("%s: %s" % (flavor, self.getDescription(test)))
                self.stream.writeln(self.separator2)
                lines = err[1].args[0]
                if type(lines) == types.StringType:
                    lines = string.split(lines, '\n')
                    if lines[-1] == '':
                        del lines[-1]
                for line in lines:
                    self.stream.writeln(line)
            else:
                handoff.append((test, err))
        if len(handoff) > 0:
            unittest._TextTestResult.printErrorList(self, flavor, handoff)

class GSTestRunner(unittest.TextTestRunner):

    def _makeResult(self):
        return _GSTextTestResult(self.stream, self.descriptions, self.verbosity)

# Define a class to make it unnecessary for importers of gstestutils
# to import unittest directly.

class GSTestSuite(unittest.TestSuite):
    pass

# Run a set of tests specified by an addTests procedure, invoked from the
# command line.  Switches of the form --var=value provide arguments for
# addTests.  Also, default gsroot to the value in gsconf.py, gsconf.gsroot,
# if not specified on the command line.

def gsRunTestsMain(addTests):
    import sys
    import gsconf
    args = {'gsroot': gsconf.gsroot}
    gsTestParseArgv(args, sys.argv)
    suite = GSTestSuite()
    addTests(suite, **args)
    GSTestRunner().run(suite)

# Parse sys.argv to extract test args.

def gsTestParseArgv(args, argv):
    import re
    arg_re = re.compile(r'\-\-([a-zA-Z0-9_]+)=(.*)$')
    for arg in argv:
        match = arg_re.match(arg)
        if match:
            args[match.group(1)] = match.group(2)
