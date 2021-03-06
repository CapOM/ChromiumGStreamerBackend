# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import unittest

from telemetry import benchmark
from telemetry.core import discover
from telemetry.core import exceptions
from telemetry.core import util
from telemetry.story import story_set as story_set_module
from telemetry.testing import fakes

import mock # pylint: disable=import-error

import gpu_test_base

def _GetGpuDir(*subdirs):
  gpu_dir = os.path.dirname(os.path.dirname(__file__))
  return os.path.join(gpu_dir, *subdirs)

# Unit tests verifying invariants of classes in GpuTestBase.

class NoOverridesTest(unittest.TestCase):
  def testValidatorBase(self):
    all_validators = discover.DiscoverClasses(
      _GetGpuDir('gpu_tests'), _GetGpuDir(),
      gpu_test_base.ValidatorBase,
      index_by_class_name=True).values()
    self.assertGreater(len(all_validators), 0)
    for validator in all_validators:
      self.assertEquals(gpu_test_base.ValidatorBase.WillNavigateToPage,
                        validator.WillNavigateToPage,
                        'Class %s should not override WillNavigateToPage'
                        % validator.__name__)
      self.assertEquals(gpu_test_base.ValidatorBase.DidNavigateToPage,
                        validator.DidNavigateToPage,
                        'Class %s should not override DidNavigateToPage'
                        % validator.__name__)
      self.assertEquals(gpu_test_base.ValidatorBase.ValidateAndMeasurePage,
                        validator.ValidateAndMeasurePage,
                        'Class %s should not override ValidateAndMeasurePage'
                        % validator.__name__)

  def testPageBase(self):
    all_pages = discover.DiscoverClasses(
      _GetGpuDir(), _GetGpuDir(),
      gpu_test_base.PageBase,
      index_by_class_name=True).values()
    self.assertGreater(len(all_pages), 0)
    for page in all_pages:
      self.assertEquals(gpu_test_base.PageBase.RunNavigateSteps,
                        page.RunNavigateSteps,
                        'Class %s should not override RunNavigateSteps'
                        % page.__name__)
      self.assertEquals(gpu_test_base.PageBase.RunPageInteractions,
                        page.RunPageInteractions,
                        'Class %s should not override RunPageInteractions'
                        % page.__name__)

#
# Tests verifying interactions between Telemetry and GpuTestBase.
#

class FakeValidator(gpu_test_base.ValidatorBase):
  def __init__(self, manager_mock=None):
    super(FakeValidator, self).__init__()
    if manager_mock == None:
      self.WillNavigateToPageInner = mock.Mock()
      self.DidNavigateToPageInner = mock.Mock()
      self.ValidateAndMeasurePageInner = mock.Mock()
    else:
      self.WillNavigateToPageInner = manager_mock.WillNavigateToPageInner
      self.DidNavigateToPageInner = manager_mock.DidNavigateToPageInner
      self.ValidateAndMeasurePageInner = \
        manager_mock.ValidateAndMeasurePageInner


class FakeGpuSharedPageState(fakes.FakeSharedPageState):
  # NOTE: if you change this logic you must change the logic in
  # GpuSharedPageState (gpu_test_base.py) as well.
  def CanRunOnBrowser(self, browser_info, page):
    expectations = page.GetExpectations()
    return expectations.GetExpectationForPage(
      browser_info.browser, page) != 'skip'

  # NOTE: if you change this logic you must change the logic in
  # GpuSharedPageState and DesktopGpuSharedPageState
  # (gpu_test_base.py) as well.
  def RunStory(self, results):
    gpu_test_base.RunStoryWithRetries(FakeGpuSharedPageState, self, results)


class FakePage(gpu_test_base.PageBase):
  def __init__(self, benchmark, name, manager_mock=None):
    super(FakePage, self).__init__(
      name=name,
      url='http://nonexistentserver.com/' + name,
      page_set=benchmark.GetFakeStorySet(),
      shared_page_state_class=FakeGpuSharedPageState,
      expectations=benchmark.GetExpectations())
    if manager_mock == None:
      self.RunNavigateStepsInner = mock.Mock()
      self.RunPageInteractionsInner = mock.Mock()
    else:
      self.RunNavigateStepsInner = manager_mock.RunNavigateStepsInner
      self.RunPageInteractionsInner = manager_mock.RunPageInteractionsInner


class FakeTest(gpu_test_base.TestBase):
  def __init__(self, manager_mock=None, max_failures=None):
    super(FakeTest, self).__init__(max_failures)
    self._fake_pages = []
    self._fake_story_set = story_set_module.StorySet()
    self._created_story_set = False
    validator_mock = manager_mock.validator if manager_mock else None
    self.validator = FakeValidator(manager_mock=validator_mock)

  def _CreateExpectations(self):
    return super(FakeTest, self)._CreateExpectations()

  def CreatePageTest(self, options):
    return self.validator

  def GetFakeStorySet(self):
    return self._fake_story_set

  def AddFakePage(self, page):
    if self._created_story_set:
      raise Exception('Can not add any more fake pages')
    self._fake_pages.append(page)

  def CreateStorySet(self, options):
    if self._created_story_set:
      raise Exception('Can only create the story set once per FakeTest')
    for page in self._fake_pages:
      self._fake_story_set.AddStory(page)
    self._created_story_set = True
    return self._fake_story_set


class FailingPage(FakePage):
  def __init__(self, benchmark, name, manager_mock=None):
    super(FailingPage, self).__init__(benchmark, name,
                                      manager_mock=manager_mock)
    self.RunNavigateStepsInner.side_effect = Exception('Deliberate exception')


class CrashingPage(FakePage):
  def __init__(self, benchmark, name, manager_mock=None):
    super(CrashingPage, self).__init__(benchmark, name,
                                       manager_mock=manager_mock)
    self.RunNavigateStepsInner.side_effect = (
      exceptions.DevtoolsTargetCrashException(None))


class PageWhichFailsNTimes(FakePage):
  def __init__(self, benchmark, name, times_to_fail, manager_mock=None):
    super(PageWhichFailsNTimes, self).__init__(benchmark, name,
                                               manager_mock=manager_mock)
    self._times_to_fail = times_to_fail
    self.RunNavigateStepsInner.side_effect = self.maybeFail

  def maybeFail(self, action_runner):
    if self._times_to_fail > 0:
      self._times_to_fail = self._times_to_fail - 1
      raise Exception('Deliberate exception')

class PageRunExecutionTest(unittest.TestCase):
  def testNoGarbageCollectionCalls(self):
    mock_shared_state = mock.Mock()
    p = gpu_test_base.PageBase('file://foo.html')
    p.Run(mock_shared_state)
    expected = [mock.call.page_test.WillNavigateToPage(
                  p, mock_shared_state.current_tab),
                mock.call.page_test.RunNavigateSteps(
                  p, mock_shared_state.current_tab),
                mock.call.page_test.DidNavigateToPage(
                  p, mock_shared_state.current_tab)]
    self.assertEquals(mock_shared_state.mock_calls, expected)

class PageExecutionTest(unittest.TestCase):
  def setupTest(self, manager_mock=None):
    finder_options = fakes.CreateBrowserFinderOptions()
    finder_options.browser_options.platform = fakes.FakeLinuxPlatform()
    finder_options.output_formats = ['none']
    finder_options.suppress_gtest_report = True
    finder_options.output_dir = None
    finder_options.upload_bucket = 'public'
    finder_options.upload_results = False
    testclass = FakeTest
    parser = finder_options.CreateParser()
    benchmark.AddCommandLineArgs(parser)
    testclass.AddCommandLineArgs(parser)
    options, dummy_args = parser.parse_args([])
    benchmark.ProcessCommandLineArgs(parser, options)
    testclass.ProcessCommandLineArgs(parser, options)
    test = testclass(manager_mock=manager_mock)
    return test, finder_options

  # Test page.Run() method is called by telemetry framework before
  # ValidateAndMeasurePageInner.
  def testPageRunMethodIsCalledBeforeValidateAndMeasurePage(self):
    manager = mock.Mock()
    test, finder_options = self.setupTest(manager)
    page = FakePage(test, 'page1')
    page.Run = manager.Run
    test.AddFakePage(page)
    self.assertEqual(test.Run(finder_options), 0,
                     'Test should run with no errors')
    expected = [mock.call.Run(mock.ANY),
                mock.call.validator.ValidateAndMeasurePageInner(
                  page, mock.ANY, mock.ANY)]
    self.assertEquals(manager.mock_calls, expected)

  def testPassingPage(self):
    manager = mock.Mock()
    test, finder_options = self.setupTest(manager_mock=manager)
    page = FakePage(test, 'page1', manager_mock=manager.page)
    test.AddFakePage(page)
    self.assertEqual(test.Run(finder_options), 0,
                     'Test should run with no errors')
    expected = [mock.call.validator.WillNavigateToPageInner(
                  page, mock.ANY),
                mock.call.page.RunNavigateStepsInner(mock.ANY),
                mock.call.validator.DidNavigateToPageInner(
                  page, mock.ANY),
                mock.call.page.RunPageInteractionsInner(mock.ANY),
                mock.call.validator.ValidateAndMeasurePageInner(
                  page, mock.ANY, mock.ANY)]
    self.assertTrue(manager.mock_calls == expected)

  def testFailingPage(self):
    test, finder_options = self.setupTest()
    page = FailingPage(test, 'page1')
    test.AddFakePage(page)
    self.assertNotEqual(test.Run(finder_options), 0, 'Test should fail')

  def testExpectedFailure(self):
    test, finder_options = self.setupTest()
    page = FailingPage(test, 'page1')
    test.AddFakePage(page)
    test.GetExpectations().Fail('page1')
    self.assertEqual(test.Run(finder_options), 0,
                     'Test should run with no errors')
    self.assertFalse(page.RunPageInteractionsInner.called)
    self.assertFalse(test.validator.ValidateAndMeasurePageInner.called)

  def testPageSetRepeatOfPageWhichFailsOnce(self):
    test, finder_options = self.setupTest()
    finder_options.pageset_repeat = 2
    page = PageWhichFailsNTimes(test, 'page1', 1)
    test.AddFakePage(page)
    test.GetExpectations().Fail('page1')
    self.assertEqual(test.Run(finder_options), 0,
                     'Test should run with no errors')
    # This will be called the second time through the page set, when
    # the page doesn't fail.
    self.assertTrue(page.RunPageInteractionsInner.called)

  def testSkipAtPageBaseLevel(self):
    test, finder_options = self.setupTest()
    page = FailingPage(test, 'page1')
    test.AddFakePage(page)
    test.GetExpectations().Skip('page1')
    self.assertEqual(test.Run(finder_options), 0,
                     'Test should run with no errors')
    self.assertFalse(test.validator.WillNavigateToPageInner.called)
    self.assertFalse(page.RunNavigateStepsInner.called)
    self.assertFalse(test.validator.DidNavigateToPageInner.called)
    self.assertFalse(page.RunPageInteractionsInner.called)
    self.assertFalse(test.validator.ValidateAndMeasurePageInner.called)

  def testSkipAtPageLevel(self):
    test, finder_options = self.setupTest()
    page = FakePage(test, 'page1')
    page.RunNavigateSteps = mock.Mock()
    page.RunPageInteractions = mock.Mock()
    test.validator.WillNavigateToPage = mock.Mock()
    test.validator.DidNavigateToPage = mock.Mock()
    test.validator.ValidateAndMeasurePage = mock.Mock()
    test.AddFakePage(page)
    test.GetExpectations().Skip('page1')
    self.assertEqual(test.Run(finder_options), 0,
                     'Test should run with no errors')
    self.assertFalse(test.validator.WillNavigateToPage.called)
    self.assertFalse(page.RunNavigateSteps.called)
    self.assertFalse(test.validator.DidNavigateToPage.called)
    self.assertFalse(page.RunPageInteractions.called)
    self.assertFalse(test.validator.ValidateAndMeasurePage.called)

  def testPassAfterExpectedFailure(self):
    manager = mock.Mock()
    test, finder_options = self.setupTest(manager_mock=manager)
    page1 = FailingPage(test, 'page1', manager_mock=manager.page1)
    test.AddFakePage(page1)
    test.GetExpectations().Fail('page1')
    page2 = FakePage(test, 'page2', manager_mock=manager.page2)
    test.AddFakePage(page2)
    self.assertEqual(test.Run(finder_options), 0,
                     'Test should run with no errors')
    expected = [mock.call.validator.WillNavigateToPageInner(
                  page1, mock.ANY),
                mock.call.page1.RunNavigateStepsInner(mock.ANY),
                mock.call.validator.WillNavigateToPageInner(
                  page2, mock.ANY),
                mock.call.page2.RunNavigateStepsInner(mock.ANY),
                mock.call.validator.DidNavigateToPageInner(
                  page2, mock.ANY),
                mock.call.page2.RunPageInteractionsInner(mock.ANY),
                mock.call.validator.ValidateAndMeasurePageInner(
                  page2, mock.ANY, mock.ANY)]
    self.assertTrue(manager.mock_calls == expected)

  def testExpectedDevtoolsTargetCrash(self):
    manager = mock.Mock()
    test, finder_options = self.setupTest(manager_mock=manager)
    page = CrashingPage(test, 'page1', manager_mock=manager.page)
    test.AddFakePage(page)
    test.GetExpectations().Fail('page1')
    self.assertEqual(test.Run(finder_options), 0,
                     'Test should run with no errors')
    expected = [mock.call.validator.WillNavigateToPageInner(
                  page, mock.ANY),
                mock.call.page.RunNavigateStepsInner(mock.ANY)]
    self.assertTrue(manager.mock_calls == expected)

  def testFlakyPage(self):
    manager = mock.Mock()
    test, finder_options = self.setupTest(manager_mock=manager)
    page = PageWhichFailsNTimes(test, 'page1', 1, manager_mock=manager.page)
    test.AddFakePage(page)
    test.GetExpectations().Flaky('page1')
    self.assertEqual(test.Run(finder_options), 0,
                     'Test should run with no errors')
    expected = [mock.call.validator.WillNavigateToPageInner(
                  page, mock.ANY),
                mock.call.page.RunNavigateStepsInner(mock.ANY),
                mock.call.validator.WillNavigateToPageInner(
                  page, mock.ANY),
                mock.call.page.RunNavigateStepsInner(mock.ANY),
                mock.call.validator.DidNavigateToPageInner(
                  page, mock.ANY),
                mock.call.page.RunPageInteractionsInner(mock.ANY),
                mock.call.validator.ValidateAndMeasurePageInner(
                  page, mock.ANY, mock.ANY)]
    self.assertTrue(manager.mock_calls == expected)

  def testFlakyPageExceedingNumRetries(self):
    manager = mock.Mock()
    test, finder_options = self.setupTest(manager_mock=manager)
    page = PageWhichFailsNTimes(test, 'page1', 2, manager_mock=manager.page)
    test.AddFakePage(page)
    test.GetExpectations().Flaky('page1', max_num_retries=1)
    self.assertNotEqual(test.Run(finder_options), 0,
                     'Test should fail')
    expected = [mock.call.validator.WillNavigateToPageInner(
                  page, mock.ANY),
                mock.call.page.RunNavigateStepsInner(mock.ANY),
                mock.call.validator.WillNavigateToPageInner(
                  page, mock.ANY),
                mock.call.page.RunNavigateStepsInner(mock.ANY)]
    self.assertTrue(manager.mock_calls == expected)

  def testFlakyPageThenPassingPage(self):
    manager = mock.Mock()
    test, finder_options = self.setupTest(manager_mock=manager)
    page1 = PageWhichFailsNTimes(test, 'page1', 1, manager_mock=manager.page1)
    test.AddFakePage(page1)
    page2 = FakePage(test, 'page2', manager_mock=manager.page2)
    test.AddFakePage(page2)
    test.GetExpectations().Flaky('page1', max_num_retries=1)
    self.assertEqual(test.Run(finder_options), 0,
                     'Test should run with no errors')
    expected = [mock.call.validator.WillNavigateToPageInner(
                  page1, mock.ANY),
                mock.call.page1.RunNavigateStepsInner(mock.ANY),
                mock.call.validator.WillNavigateToPageInner(
                  page1, mock.ANY),
                mock.call.page1.RunNavigateStepsInner(mock.ANY),
                mock.call.validator.DidNavigateToPageInner(
                  page1, mock.ANY),
                mock.call.page1.RunPageInteractionsInner(mock.ANY),
                mock.call.validator.ValidateAndMeasurePageInner(
                  page1, mock.ANY, mock.ANY),
                mock.call.validator.WillNavigateToPageInner(
                  page2, mock.ANY),
                mock.call.page2.RunNavigateStepsInner(mock.ANY),
                mock.call.validator.DidNavigateToPageInner(
                  page2, mock.ANY),
                mock.call.page2.RunPageInteractionsInner(mock.ANY),
                mock.call.validator.ValidateAndMeasurePageInner(
                  page2, mock.ANY, mock.ANY)]
    self.assertTrue(manager.mock_calls == expected)

  def testPassingPageThenFlakyPage(self):
    manager = mock.Mock()
    test, finder_options = self.setupTest(manager_mock=manager)
    page1 = FakePage(test, 'page1', manager_mock=manager.page1)
    test.AddFakePage(page1)
    page2 = PageWhichFailsNTimes(test, 'page2', 1, manager_mock=manager.page2)
    test.AddFakePage(page2)
    test.GetExpectations().Flaky('page2', max_num_retries=1)
    self.assertEqual(test.Run(finder_options), 0,
                     'Test should run with no errors')
    expected = [mock.call.validator.WillNavigateToPageInner(
                  page1, mock.ANY),
                mock.call.page1.RunNavigateStepsInner(mock.ANY),
                mock.call.validator.DidNavigateToPageInner(
                  page1, mock.ANY),
                mock.call.page1.RunPageInteractionsInner(mock.ANY),
                mock.call.validator.ValidateAndMeasurePageInner(
                  page1, mock.ANY, mock.ANY),
                mock.call.validator.WillNavigateToPageInner(
                  page2, mock.ANY),
                mock.call.page2.RunNavigateStepsInner(mock.ANY),
                mock.call.validator.WillNavigateToPageInner(
                  page2, mock.ANY),
                mock.call.page2.RunNavigateStepsInner(mock.ANY),
                mock.call.validator.DidNavigateToPageInner(
                  page2, mock.ANY),
                mock.call.page2.RunPageInteractionsInner(mock.ANY),
                mock.call.validator.ValidateAndMeasurePageInner(
                  page2, mock.ANY, mock.ANY)]
    self.assertTrue(manager.mock_calls == expected)
