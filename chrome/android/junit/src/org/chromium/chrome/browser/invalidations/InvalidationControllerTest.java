// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.invalidation;

import android.accounts.Account;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.CollectionUtil;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.components.invalidation.InvalidationClientService;
import org.chromium.sync.AndroidSyncSettings;
import org.chromium.sync.ModelType;
import org.chromium.sync.ModelTypeHelper;
import org.chromium.sync.notifier.InvalidationIntentProtocol;
import org.chromium.sync.signin.ChromeSigninController;
import org.chromium.sync.test.util.MockSyncContentResolverDelegate;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowActivity;

import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Tests for the {@link InvalidationController}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class InvalidationControllerTest {
    /**
     * Stubbed out ProfileSyncService with a setter to control return value of
     * {@link ProfileSyncService#getPreferredDataTypes()}
     */
    private static class ProfileSyncServiceStub extends ProfileSyncService {
        private Set<Integer> mPreferredDataTypes;

        public ProfileSyncServiceStub() {
            super();
        }

        public void setPreferredDataTypes(Set<Integer> types) {
            mPreferredDataTypes = types;
        }

        @Override
        protected void init() {
            // Skip native initialization.
        }

        @Override
        public Set<Integer> getPreferredDataTypes() {
            return mPreferredDataTypes;
        }
    }

    private enum IntentType { START, START_AND_REGISTER, STOP };

    private ShadowActivity mShadowActivity;
    private Context mContext;
    private InvalidationController mController;

    /**
     * The names of the preferred ModelTypes.
     */
    private Set<String> mAllTypes;

    /**
     * The names of the non-session preferred ModelTypes.
     */
    private Set<String> mNonSessionTypes;

    @Before
    public void setUp() throws Exception {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        mShadowActivity = Robolectric.shadowOf(activity);
        mContext = activity;

        ModelTypeHelper.setTestDelegate(new ModelTypeHelper.TestDelegate() {
            public String toNotificationType(int modelType) {
                return Integer.toString(modelType);
            }
        });

        ProfileSyncServiceStub profileSyncServiceStub = new ProfileSyncServiceStub();
        ProfileSyncService.overrideForTests(profileSyncServiceStub);
        profileSyncServiceStub.setPreferredDataTypes(
                CollectionUtil.newHashSet(ModelType.BOOKMARKS, ModelType.SESSIONS));
        mAllTypes = CollectionUtil.newHashSet(
                ModelTypeHelper.toNotificationType(ModelType.BOOKMARKS),
                ModelTypeHelper.toNotificationType(ModelType.SESSIONS));
        mNonSessionTypes = CollectionUtil.newHashSet(ModelTypeHelper.toNotificationType(
                ModelType.BOOKMARKS));

        // We don't want to use the system content resolver, so we override it.
        MockSyncContentResolverDelegate delegate = new MockSyncContentResolverDelegate();
        // Android master sync can safely always be on.
        delegate.setMasterSyncAutomatically(true);
        AndroidSyncSettings.overrideForTests(mContext, delegate);

        ChromeSigninController.get(mContext).setSignedInAccountName("test@example.com");
        AndroidSyncSettings.enableChromeSync(mContext);
    }

    /**
     * Verify the intent sent by InvalidationController#stop().
     */
    @Test
    @Feature({"Sync"})
    public void testStop() throws Exception {
        InvalidationController controller = new InvalidationController(mContext, true, false);
        controller.stop();
        Intent intent = getOnlyIntent();
        validateIntentComponent(intent);
        Assert.assertEquals(1, intent.getExtras().size());
        Assert.assertTrue(intent.hasExtra(InvalidationIntentProtocol.EXTRA_STOP));
        Assert.assertTrue(intent.getBooleanExtra(InvalidationIntentProtocol.EXTRA_STOP, false));
    }

    /**
     * Verify the intent sent by InvalidationController#ensureStartedAndUpdateRegisteredTypes().
     */
    @Test
    @Feature({"Sync"})
    public void testEnsureStartedAndUpdateRegisteredTypes() {
        InvalidationController controller = new InvalidationController(mContext, false, false);
        controller.ensureStartedAndUpdateRegisteredTypes();
        Intent intent = getOnlyIntent();

        // Ensure GCM is initialized. This is a regression test for http://crbug.com/475299.
        Assert.assertTrue(controller.isGcmInitialized());

        // Validate destination.
        validateIntentComponent(intent);
        Assert.assertEquals(InvalidationIntentProtocol.ACTION_REGISTER, intent.getAction());

        // Validate account.
        Account intentAccount =
                intent.getParcelableExtra(InvalidationIntentProtocol.EXTRA_ACCOUNT);
        Assert.assertEquals("test@example.com", intentAccount.name);

        // Validate registered types.
        Assert.assertEquals(mAllTypes, getRegisterIntentRegisterTypes(intent));
        Assert.assertNull(InvalidationIntentProtocol.getRegisteredObjectIds(intent));
    }

    /**
     * Test that pausing and resuming Chrome's activities does not send any intents if sync is
     * disabled.
     */
    @Test
    @Feature({"Sync"})
    public void testPauseAndResumeMainActivityWithSyncDisabled() throws Exception {
        AndroidSyncSettings.disableChromeSync(mContext);

        InvalidationController controller = new InvalidationController(mContext, false, false);
        controller.onApplicationStateChange(ApplicationState.HAS_PAUSED_ACTIVITIES);
        controller.onApplicationStateChange(ApplicationState.HAS_RUNNING_ACTIVITIES);
        assertNoNewIntents();
    }

    /**
     * Test that creating an InvalidationController object registers an ApplicationStateListener.
     */
    @Test
    @Feature({"Sync"})
    public void testEnsureConstructorRegistersListener() throws Exception {
        final AtomicBoolean listenerCallbackCalled = new AtomicBoolean();

        // Create instance.
        new InvalidationController(mContext, true, false) {
            @Override
            public void onApplicationStateChange(int newState) {
                listenerCallbackCalled.set(true);
            }
        };

        // Ensure initial state is correct.
        Assert.assertFalse(listenerCallbackCalled.get());

        // Ensure we get a callback, which means we have registered for them.
        ApplicationStatus.onStateChangeForTesting(new Activity(), ActivityState.CREATED);
        Assert.assertTrue(listenerCallbackCalled.get());
    }

    /**
     * Test that the controller registers for session invalidations and stays registered when
     * disabling session invalidations is prohibited.
     */
    @Test
    @Feature({"Sync"})
    public void testCannotToggleSessionInvalidations() {
        InvalidationController controller = new InvalidationController(mContext, false, false);
        controller.ensureStartedAndUpdateRegisteredTypes();
        Assert.assertEquals(mAllTypes, getRegisterIntentRegisterTypes(getOnlyIntent()));

        controller.onRecentTabsPageOpened();
        Robolectric.runUiThreadTasksIncludingDelayedTasks();
        assertNoNewIntents();

        controller.onRecentTabsPageClosed();
        Robolectric.runUiThreadTasksIncludingDelayedTasks();
        assertNoNewIntents();
    }

    /**********************************************************************************************
     * Tests for when session invalidations can be disabled.
     */

    /**
     * Test that an intent is sent to register for session invalidations after the RecentTabsPage is
     * opened.
     */
    @Test
    @Feature({"Sync"})
    public void testRecentTabsPageShown() {
        InvalidationController controller = new InvalidationController(mContext, true, false);
        controller.ensureStartedAndUpdateRegisteredTypes();
        Assert.assertEquals(mNonSessionTypes, getRegisterIntentRegisterTypes(getOnlyIntent()));

        controller.onRecentTabsPageOpened();
        Robolectric.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertEquals(mAllTypes, getRegisterIntentRegisterTypes(getOnlyIntent()));

        controller.onRecentTabsPageClosed();
        Robolectric.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertEquals(mNonSessionTypes, getRegisterIntentRegisterTypes(getOnlyIntent()));
    }

    /**
     * Test that if the InvalidationController is started while the RecentTabsPage is shown that
     * an intent is sent to register for session invalidations only once the InvalidationController
     * is started.
     */
    @Test
    @Feature({"Sync"})
    public void testStartWhileRecentTabsPageShown() {
        InvalidationController controller = new InvalidationController(mContext, true, false);
        controller.onRecentTabsPageOpened();
        Robolectric.runUiThreadTasksIncludingDelayedTasks();
        assertNoNewIntents();

        controller.ensureStartedAndUpdateRegisteredTypes();
        Assert.assertEquals(IntentType.START_AND_REGISTER, getIntentType(getOnlyIntent()));
        Robolectric.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertEquals(mAllTypes, getRegisterIntentRegisterTypes(getOnlyIntent()));

        controller.onRecentTabsPageClosed();
        Robolectric.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertEquals(mNonSessionTypes, getRegisterIntentRegisterTypes(getOnlyIntent()));
    }

    /**
     * Test the handling for multiple open RecentTabsPages.
     */
    @Test
    @Feature({"Sync"})
    public void testMultipleRecentTabsPages() {
        InvalidationController controller = new InvalidationController(mContext, true, false);
        controller.ensureStartedAndUpdateRegisteredTypes();
        Assert.assertEquals(mNonSessionTypes, getRegisterIntentRegisterTypes(getOnlyIntent()));

        controller.onRecentTabsPageOpened();
        Robolectric.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertEquals(mAllTypes, getRegisterIntentRegisterTypes(getOnlyIntent()));

        controller.onRecentTabsPageOpened();
        Robolectric.runUiThreadTasksIncludingDelayedTasks();
        assertNoNewIntents();

        controller.onRecentTabsPageClosed();
        Robolectric.runUiThreadTasksIncludingDelayedTasks();
        assertNoNewIntents();

        controller.onRecentTabsPageClosed();
        Robolectric.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertEquals(mNonSessionTypes, getRegisterIntentRegisterTypes(getOnlyIntent()));
    }

    /**
     * Test the handling for when the RecentTabsPage is opened/closed quickly.
     */
    @Test
    @Feature({"Sync"})
    public void testOpenCloseRecentTabsPageQuickly() {
        InvalidationController controller = new InvalidationController(mContext, true, false);
        controller.ensureStartedAndUpdateRegisteredTypes();
        Assert.assertEquals(mNonSessionTypes, getRegisterIntentRegisterTypes(getOnlyIntent()));

        controller.onRecentTabsPageOpened();
        controller.onRecentTabsPageClosed();
        controller.onRecentTabsPageOpened();
        Robolectric.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertEquals(mAllTypes, getRegisterIntentRegisterTypes(getOnlyIntent()));

        controller.onRecentTabsPageClosed();
        Robolectric.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertEquals(mNonSessionTypes, getRegisterIntentRegisterTypes(getOnlyIntent()));

        controller.onRecentTabsPageOpened();
        controller.onRecentTabsPageClosed();
        controller.onRecentTabsPageOpened();
        controller.onRecentTabsPageClosed();
        Robolectric.runUiThreadTasksIncludingDelayedTasks();
        assertNoNewIntents();
    }

    /**
     * Test that if the InvalidationController is stopped prior to session sync invalidations being
     * disabled as a result of the RecentTabsPage being closed, that session sync invalidations are
     * disabled when the InvalidationController is restarted.
     */
    @Test
    @Feature({"Sync"})
    public void testDisableSessionInvalidationsOnStart() {
        InvalidationController controller = new InvalidationController(mContext, true, false);
        controller.ensureStartedAndUpdateRegisteredTypes();
        Assert.assertEquals(mNonSessionTypes, getRegisterIntentRegisterTypes(getOnlyIntent()));
        controller.onRecentTabsPageOpened();
        Robolectric.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertEquals(mAllTypes, getRegisterIntentRegisterTypes(getOnlyIntent()));

        controller.onRecentTabsPageClosed();
        controller.stop();
        Robolectric.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertEquals(IntentType.STOP, getIntentType(getOnlyIntent()));

        controller.ensureStartedAndUpdateRegisteredTypes();
        Assert.assertEquals(mAllTypes, getRegisterIntentRegisterTypes(getOnlyIntent()));
        Robolectric.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertEquals(mNonSessionTypes, getRegisterIntentRegisterTypes(getOnlyIntent()));
    }

    /**
     * Test that if the activity is paused prior to session sync invalidations being disabled as a
     * result of the RecentTabsPage being closed, that session sync invalidations are disabled when
     * the activity is resumed.
     */
    @Test
    @Feature({"Sync"})
    public void testDisableSessionInvalidationsOnResume() {
        InvalidationController controller = new InvalidationController(mContext, true, false);
        controller.ensureStartedAndUpdateRegisteredTypes();
        Assert.assertEquals(mNonSessionTypes, getRegisterIntentRegisterTypes(getOnlyIntent()));
        controller.onRecentTabsPageOpened();
        Robolectric.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertEquals(mAllTypes, getRegisterIntentRegisterTypes(getOnlyIntent()));

        controller.onRecentTabsPageClosed();
        controller.onApplicationStateChange(ApplicationState.HAS_PAUSED_ACTIVITIES);
        Robolectric.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertEquals(IntentType.STOP, getIntentType(getOnlyIntent()));

        controller.onApplicationStateChange(ApplicationState.HAS_RUNNING_ACTIVITIES);
        Assert.assertEquals(IntentType.START, getIntentType(getOnlyIntent()));
        Robolectric.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertEquals(mNonSessionTypes, getRegisterIntentRegisterTypes(getOnlyIntent()));
    }

    /**
     * Test that pausing all of Chrome's activities and resuming one of them keeps session
     * invalidations enabled if they were enabled prior to Chrome's activities getting paused.
     */
    @Test
    @Feature({"Sync"})
    public void testPauseAndResumeMainActivity() throws Exception {
        InvalidationController controller = new InvalidationController(mContext, true, false);
        controller.ensureStartedAndUpdateRegisteredTypes();
        Assert.assertEquals(mNonSessionTypes, getRegisterIntentRegisterTypes(getOnlyIntent()));
        controller.onRecentTabsPageOpened();
        Robolectric.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertEquals(mAllTypes, getRegisterIntentRegisterTypes(getOnlyIntent()));

        controller.onApplicationStateChange(ApplicationState.HAS_PAUSED_ACTIVITIES);
        Robolectric.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertEquals(IntentType.STOP, getIntentType(getOnlyIntent()));

        // Resuming the activity should not send a START_AND_REGISTER intent.
        controller.onApplicationStateChange(ApplicationState.HAS_RUNNING_ACTIVITIES);
        Robolectric.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertEquals(IntentType.START, getIntentType(getOnlyIntent()));
    }

    /**
     * Test that opening the RecentTabsPage has no effect after the InvalidationController is
     * stopped.
     */
    @Test
    @Feature({"Sync"})
    public void testPauseAndResumeMainActivityAfterStop() throws Exception {
        InvalidationController controller = new InvalidationController(mContext, true, false);
        controller.ensureStartedAndUpdateRegisteredTypes();
        Assert.assertEquals(mNonSessionTypes, getRegisterIntentRegisterTypes(getOnlyIntent()));

        controller.stop();
        Robolectric.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertEquals(IntentType.STOP, getIntentType(getOnlyIntent()));

        controller.onRecentTabsPageOpened();
        Robolectric.runUiThreadTasksIncludingDelayedTasks();
        assertNoNewIntents();
    }

    /**
     * Asserts that {@code intent} is destined for the correct component.
     */
    private static void validateIntentComponent(Intent intent) {
        Assert.assertNotNull(intent.getComponent());
        Assert.assertEquals(
                InvalidationClientService.class.getName(), intent.getComponent().getClassName());
    }

    /**
     * Assert that there are no new intents.
     */
    private void assertNoNewIntents() {
        Assert.assertNull(mShadowActivity.getNextStartedService());
    }

    /**
     * Assert that there is only one new intent. Returns the intent.
     */
    private Intent getOnlyIntent() {
        Intent intent = mShadowActivity.getNextStartedService();
        Assert.assertNotNull(intent);
        Assert.assertNull(mShadowActivity.getNextStartedService());
        return intent;
    }

    /**
     * Returns the type of the intent.
     */
    private static IntentType getIntentType(Intent intent) {
        if (intent.hasExtra(InvalidationIntentProtocol.EXTRA_STOP)) {
            return IntentType.STOP;
        } else if (intent.hasExtra(InvalidationIntentProtocol.EXTRA_REGISTERED_TYPES)) {
            return IntentType.START_AND_REGISTER;
        } else {
            return IntentType.START;
        }
    }

    /**
     * Returns the names of the ModelTypes to be registered by the passed in START_AND_REGISTER
     * intent.
     */
    private static Set<String> getRegisterIntentRegisterTypes(Intent intent) {
        Set<String> registeredTypes = new HashSet<String>();
        registeredTypes.addAll(
                intent.getStringArrayListExtra(InvalidationIntentProtocol.EXTRA_REGISTERED_TYPES));
        return registeredTypes;
    }

}
