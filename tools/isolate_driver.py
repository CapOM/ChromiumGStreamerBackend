#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Adaptor script called through build/isolate.gypi.

Creates a wrapping .isolate which 'includes' the original one, that can be
consumed by tools/swarming_client/isolate.py. Path variables are determined
based on the current working directory. The relative_cwd in the .isolated file
is determined based on the .isolate file that declare the 'command' variable to
be used so the wrapping .isolate doesn't affect this value.

This script loads build.ninja and processes it to determine all the executables
referenced by the isolated target. It adds them in the wrapping .isolate file.

WARNING: The target to use for build.ninja analysis is the base name of the
.isolate file plus '_run'. For example, 'foo_test.isolate' would have the target
'foo_test_run' analysed.
"""

import glob
import json
import logging
import os
import posixpath
import re
import StringIO
import subprocess
import sys
import time

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
SWARMING_CLIENT_DIR = os.path.join(TOOLS_DIR, 'swarming_client')
SRC_DIR = os.path.dirname(TOOLS_DIR)

sys.path.insert(0, SWARMING_CLIENT_DIR)

import isolate_format


def load_ninja_recursively(build_dir, ninja_path, build_steps):
  """Crudely extracts all the subninja and build referenced in ninja_path.

  In particular, it ignores rule and variable declarations. The goal is to be
  performant (well, as much as python can be performant) which is currently in
  the <200ms range for a complete chromium tree. As such the code is laid out
  for performance instead of readability.
  """
  logging.debug('Loading %s', ninja_path)
  try:
    with open(os.path.join(build_dir, ninja_path), 'rb') as f:
      line = None
      merge_line = ''
      subninja = []
      for line in f:
        line = line.rstrip()
        if not line:
          continue

        if line[-1] == '$':
          # The next line needs to be merged in.
          merge_line += line[:-1]
          continue

        if merge_line:
          line = merge_line + line
          merge_line = ''

        statement = line[:line.find(' ')]
        if statement == 'build':
          # Save the dependency list as a raw string. Only the lines needed will
          # be processed with raw_build_to_deps(). This saves a good 70ms of
          # processing time.
          build_target, dependencies = line[6:].split(': ', 1)
          # Interestingly, trying to be smart and only saving the build steps
          # with the intended extensions ('', '.stamp', '.so') slows down
          # parsing even if 90% of the build rules can be skipped.
          # On Windows, a single step may generate two target, so split items
          # accordingly. It has only been seen for .exe/.exe.pdb combos.
          for i in build_target.strip().split():
            build_steps[i] = dependencies
        elif statement == 'subninja':
          subninja.append(line[9:])
  except IOError:
    print >> sys.stderr, 'Failed to open %s' % ninja_path
    raise

  total = 1
  for rel_path in subninja:
    try:
      # Load each of the files referenced.
      # TODO(maruel): Skip the files known to not be needed. It saves an aweful
      # lot of processing time.
      total += load_ninja_recursively(build_dir, rel_path, build_steps)
    except IOError:
      print >> sys.stderr, '... as referenced by %s' % ninja_path
      raise
  return total


def load_ninja(build_dir):
  """Loads the tree of .ninja files in build_dir.

  Returns:
    dict(target: list of dependencies).
  """
  build_steps = {}
  total = load_ninja_recursively(build_dir, 'build.ninja', build_steps)
  logging.info('Loaded %d ninja files, %d build steps', total, len(build_steps))
  return build_steps


def using_blacklist(item):
  """Returns True if an item should be analyzed.

  Ignores many rules that are assumed to not depend on a dynamic library. If
  the assumption doesn't hold true anymore for a file format, remove it from
  this list. This is simply an optimization.
  """
  # *.json is ignored below, *.isolated.gen.json is an exception, it is produced
  # by isolate_driver.py in 'test_isolation_mode==prepare'.
  if item.endswith('.isolated.gen.json'):
    return True
  IGNORED = (
    '.a', '.cc', '.css', '.dat', '.def', '.frag', '.h', '.html', '.isolate',
    '.js', '.json', '.manifest', '.o', '.obj', '.pak', '.png', '.pdb', '.py',
    '.strings', '.test', '.txt', '.vert',
  )
  # ninja files use native path format.
  ext = os.path.splitext(item)[1]
  if ext in IGNORED:
    return False
  # Special case Windows, keep .dll.lib but discard .lib.
  if sys.platform == 'win32':
    if item.endswith('.dll.lib'):
      return True
    if ext == '.lib':
      return False
  return item not in ('', '|', '||')

# This is a whitelist of known ninja native rules.
KNOWN_TOOLS = frozenset(
    (
      'copy',
      'copy_infoplist',
      'cxx',
      'idl',
      'link',
      'link_embed',
      'mac_tool',
      'package_framework',
      'phony',
      'rc',
      'solink',
      'solink_embed',
      'solink_module',
      'solink_module_embed',
      'solink_module_notoc',
      'solink_notoc',
      'stamp',
    ))


def raw_build_to_deps(item):
  """Converts a raw ninja build statement into the list of interesting
  dependencies.
  """
  items = filter(None, item.split(' '))
  for i in xrange(len(items) - 2, 0, -1):
    # Merge back '$ ' escaping.
    # OMG please delete this code as soon as possible.
    if items[i].endswith('$'):
      items[i] = items[i][:-1] + ' ' + items[i+1]
      items.pop(i+1)

  # Always skip the first item; it is the build rule type, e.g. , etc.
  if items[0] not in KNOWN_TOOLS:
    # Check for phony ninja rules.
    assert re.match(r'^[^.]+_[0-9a-f]{32}$', items[0]), items

  return filter(using_blacklist, items[1:])


def collect_deps(target, build_steps, dependencies_added, rules_seen):
  """Recursively adds all the interesting dependencies for |target|
  into |dependencies_added|.
  """
  if rules_seen is None:
    rules_seen = set()
  if target in rules_seen:
    # TODO(maruel): Figure out how it happens.
    logging.warning('Circular dependency for %s!', target)
    return
  rules_seen.add(target)
  try:
    dependencies = raw_build_to_deps(build_steps[target])
  except KeyError:
    logging.info('Failed to find a build step to generate: %s', target)
    return
  logging.debug('collect_deps(%s) -> %s', target, dependencies)
  for dependency in dependencies:
    dependencies_added.add(dependency)
    collect_deps(dependency, build_steps, dependencies_added, rules_seen)


def post_process_deps(build_dir, dependencies):
  """Processes the dependency list with OS specific rules.

  Returns:
    list of dependencies to add.
  """
  out = []
  for i in dependencies:
    if os.path.isabs(i):
      # In some rare case, there's dependency set explicitly on files outside
      # the checkout. In practice, it was observed on /usr/bin/eu-strip only on
      # official Chrome build.
      continue
    if os.path.isdir(os.path.join(build_dir, i)):
      if sys.platform == 'darwin':
        # This is an application.
        out.append(i + '/')
    elif i.endswith('.so.TOC'):
      out.append(i[:-4])
    elif i.endswith('.dylib.TOC'):
      i = i[:-4]
      out.append(i)
      # Debug symbols may not be present.
      i += '.dSym'
      if os.path.isdir(os.path.join(build_dir, i)):
        out.append(i + '/')
    elif i.endswith('.dll.lib'):
      i = i[:-4]
      out.append(i)
      # Naming is inconsistent.
      if os.path.isfile(os.path.join(build_dir, i + '.pdb')):
        out.append(i + '.pdb')
      if os.path.isfile(os.path.join(build_dir, i[:-4] + '.pdb')):
        out.append(i[:-4] + '.pdb')
    elif i.endswith('.exe'):
      out.append(i)
      # Naming is inconsistent.
      if os.path.isfile(os.path.join(build_dir, i + '.pdb')):
        out.append(i + '.pdb')
      if os.path.isfile(os.path.join(build_dir, i[:-4] + '.pdb')):
        out.append(i[:-4] + '.pdb')
    elif i.endswith('.nexe'):
      out.append(i)
      i += '.debug'
      if os.path.isfile(os.path.join(build_dir, i)):
        out.append(i)
    elif sys.platform != 'win32':
      # On POSIX, executables have no extension.
      if not os.path.splitext(i)[1]:
        out.append(i)
  return out


def create_wrapper(args, isolate_index, isolated_index):
  """Creates a wrapper .isolate that add dynamic libs.

  The original .isolate is not modified.
  """
  cwd = os.getcwd()
  isolate = args[isolate_index]
  # The code assumes the .isolate file is always specified path-less in cwd. Fix
  # if this assumption doesn't hold true.
  assert os.path.basename(isolate) == isolate, isolate

  # This will look like ../out/Debug. This is based against cwd. Note that this
  # must equal the value provided as PRODUCT_DIR.
  build_dir = os.path.dirname(args[isolated_index])

  # This will look like chrome/unit_tests.isolate. It is based against SRC_DIR.
  # It's used to calculate temp_isolate.
  src_isolate = os.path.relpath(os.path.join(cwd, isolate), SRC_DIR)

  # The wrapping .isolate. This will look like
  # ../out/Debug/gen/chrome/unit_tests.isolate.
  temp_isolate = os.path.join(build_dir, 'gen', src_isolate)
  temp_isolate_dir = os.path.dirname(temp_isolate)

  # Relative path between the new and old .isolate file.
  isolate_relpath = os.path.relpath(
      '.', temp_isolate_dir).replace(os.path.sep, '/')

  # It's a big assumption here that the name of the isolate file matches the
  # primary target '_run'. Fix accordingly if this doesn't hold true, e.g.
  # complain to maruel@.
  target = isolate[:-len('.isolate')] + '_run'
  build_steps = load_ninja(build_dir)
  binary_deps = set()
  collect_deps(target, build_steps, binary_deps, None)
  binary_deps = post_process_deps(build_dir, binary_deps)
  logging.debug(
      'Binary dependencies:%s', ''.join('\n  ' + i for i in binary_deps))

  # Now do actual wrapping .isolate.
  isolate_dict = {
    'includes': [
      posixpath.join(isolate_relpath, isolate),
    ],
    'variables': {
      # Will look like ['<(PRODUCT_DIR)/lib/flibuser_prefs.so'].
      'files': sorted(
          '<(PRODUCT_DIR)/%s' % i.replace(os.path.sep, '/')
          for i in binary_deps),
    },
  }
  if not os.path.isdir(temp_isolate_dir):
    os.makedirs(temp_isolate_dir)
  comment = (
      '# Warning: this file was AUTOGENERATED.\n'
      '# DO NO EDIT.\n')
  out = StringIO.StringIO()
  isolate_format.print_all(comment, isolate_dict, out)
  isolate_content = out.getvalue()
  with open(temp_isolate, 'wb') as f:
    f.write(isolate_content)
  logging.info('Added %d dynamic libs', len(binary_deps))
  logging.debug('%s', isolate_content)
  args[isolate_index] = temp_isolate


def prepare_isolate_call(args, output):
  """Gathers all information required to run isolate.py later.

  Dumps it as JSON to |output| file.
  """
  with open(output, 'wb') as f:
    json.dump({
      'args': args,
      'dir': os.getcwd(),
      'version': 1,
    }, f, indent=2, sort_keys=True)


def main():
  logging.basicConfig(level=logging.ERROR, format='%(levelname)7s %(message)s')
  args = sys.argv[1:]
  mode = args[0] if args else None
  isolate = None
  isolated = None
  for i, arg in enumerate(args):
    if arg == '--isolate':
      isolate = i + 1
    if arg == '--isolated':
      isolated = i + 1
  if isolate is None or isolated is None or not mode:
    print >> sys.stderr, 'Internal failure'
    return 1

  create_wrapper(args, isolate, isolated)

  # In 'prepare' mode just collect all required information for postponed
  # isolated.py invocation later, store it in *.isolated.gen.json file.
  if mode == 'prepare':
    prepare_isolate_call(args[1:], args[isolated] + '.gen.json')
    return 0

  swarming_client = os.path.join(SRC_DIR, 'tools', 'swarming_client')
  sys.stdout.flush()
  result = subprocess.call(
      [sys.executable, os.path.join(swarming_client, 'isolate.py')] + args)
  return result


if __name__ == '__main__':
  sys.exit(main())
