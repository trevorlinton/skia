#!/usr/bin/python

"""
Copyright 2014 Google Inc.

Use of this source code is governed by a BSD-style license that can be
found in the LICENSE file.

A wrapper around the standard Python unittest library, adding features we need
for various unittests within this directory.
"""

import os
import subprocess
import unittest


class TestCase(unittest.TestCase):

  def shortDescription(self):
    """Tell unittest framework to not print docstrings for test cases."""
    return None

  def run_command(self, args):
    """Runs a program from the command line and returns stdout.

    Args:
      args: Command line to run, as a list of string parameters. args[0] is the
            binary to run.

    Returns:
      stdout from the program, as a single string.

    Raises:
      Exception: the program exited with a nonzero return code.
    """
    proc = subprocess.Popen(args,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE)
    (stdout, stderr) = proc.communicate()
    if proc.returncode is not 0:
      raise Exception('command "%s" failed: %s' % (args, stderr))
    return stdout

  def find_path_to_program(self, program):
    """Returns path to an existing program binary.

    Args:
      program: Basename of the program to find (e.g., 'render_pictures').

    Returns:
      Absolute path to the program binary, as a string.

    Raises:
      Exception: unable to find the program binary.
    """
    trunk_path = os.path.abspath(os.path.join(os.path.dirname(__file__),
                                              os.pardir, os.pardir))
    possible_paths = [os.path.join(trunk_path, 'out', 'Release', program),
                      os.path.join(trunk_path, 'out', 'Debug', program),
                      os.path.join(trunk_path, 'out', 'Release',
                                   program + '.exe'),
                      os.path.join(trunk_path, 'out', 'Debug',
                                   program + '.exe')]
    for try_path in possible_paths:
      if os.path.isfile(try_path):
        return try_path
    raise Exception('cannot find %s in paths %s; maybe you need to '
                    'build %s?' % (program, possible_paths, program))


def main(test_case_class):
  """Run the unit tests within the given class.

  Raises an Exception if any of those tests fail (in case we are running in the
  context of run_all.py, which depends on that Exception to signal failures).

  TODO(epoger): Make all of our unit tests use the Python unittest framework,
  so we can leverage its ability to run *all* the tests and report failures at
  the end.
  """
  suite = unittest.TestLoader().loadTestsFromTestCase(test_case_class)
  results = unittest.TextTestRunner(verbosity=2).run(suite)
  if not results.wasSuccessful():
    raise Exception('failed unittest %s' % test_case_class)
