// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_PROVIDED_FILE_SYSTEM_H_
#define CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_PROVIDED_FILE_SYSTEM_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/file_system_provider/abort_callback.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_observer.h"
#include "chrome/browser/chromeos/file_system_provider/queue.h"
#include "chrome/browser/chromeos/file_system_provider/request_manager.h"
#include "storage/browser/fileapi/async_file_util.h"
#include "storage/browser/fileapi/watcher_manager.h"
#include "url/gurl.h"

class Profile;

namespace net {
class IOBuffer;
}  // namespace net

namespace base {
class FilePath;
}  // namespace base

namespace extensions {
class EventRouter;
}  // namespace extensions

namespace chromeos {
namespace file_system_provider {

class NotificationManagerInterface;

// Automatically calls the |update_callback| after all of the callbacks created
// with |CreateCallback| are called.
//
// It's used to update tags of watchers once a notification about a change is
// handled. It is to make sure that the change notification is fully handled
// before remembering the new tag.
//
// It is necessary to update the tag after all observers handle it fully, so
// in case of shutdown or a crash we get the notifications again.
class AutoUpdater : public base::RefCounted<AutoUpdater> {
 public:
  explicit AutoUpdater(const base::Closure& update_callback);

  // Creates a new callback which needs to be called before the update callback
  // is called.
  base::Closure CreateCallback();

 private:
  friend class base::RefCounted<AutoUpdater>;

  // Called once the callback created with |CreateCallback| is executed. Once
  // all of such callbacks are called, then the update callback is invoked.
  void OnPendingCallback();

  virtual ~AutoUpdater();

  base::Closure update_callback_;
  int created_callbacks_;
  int pending_callbacks_;
};

// Provided file system implementation. Forwards requests between providers and
// clients.
class ProvidedFileSystem : public ProvidedFileSystemInterface {
 public:
  ProvidedFileSystem(Profile* profile,
                     const ProvidedFileSystemInfo& file_system_info);
  ~ProvidedFileSystem() override;

  // Sets a custom event router. Used in unit tests to mock out the real
  // extension.
  void SetEventRouterForTesting(extensions::EventRouter* event_router);

  // Sets a custom notification manager. It will recreate the request manager,
  // so is must be called just after creating ProvideFileSystem instance.
  // Used by unit tests.
  void SetNotificationManagerForTesting(
      scoped_ptr<NotificationManagerInterface> notification_manager);

  // ProvidedFileSystemInterface overrides.
  AbortCallback RequestUnmount(
      const storage::AsyncFileUtil::StatusCallback& callback) override;
  AbortCallback GetMetadata(const base::FilePath& entry_path,
                            MetadataFieldMask fields,
                            const GetMetadataCallback& callback) override;
  AbortCallback GetActions(const base::FilePath& entry_path,
                           const GetActionsCallback& callback) override;
  AbortCallback ExecuteAction(
      const base::FilePath& entry_path,
      const std::string& action_id,
      const storage::AsyncFileUtil::StatusCallback& callback) override;
  AbortCallback ReadDirectory(
      const base::FilePath& directory_path,
      const storage::AsyncFileUtil::ReadDirectoryCallback& callback) override;
  AbortCallback OpenFile(const base::FilePath& file_path,
                         OpenFileMode mode,
                         const OpenFileCallback& callback) override;
  AbortCallback CloseFile(
      int file_handle,
      const storage::AsyncFileUtil::StatusCallback& callback) override;
  AbortCallback ReadFile(int file_handle,
                         net::IOBuffer* buffer,
                         int64 offset,
                         int length,
                         const ReadChunkReceivedCallback& callback) override;
  AbortCallback CreateDirectory(
      const base::FilePath& directory_path,
      bool recursive,
      const storage::AsyncFileUtil::StatusCallback& callback) override;
  AbortCallback DeleteEntry(
      const base::FilePath& entry_path,
      bool recursive,
      const storage::AsyncFileUtil::StatusCallback& callback) override;
  AbortCallback CreateFile(
      const base::FilePath& file_path,
      const storage::AsyncFileUtil::StatusCallback& callback) override;
  AbortCallback CopyEntry(
      const base::FilePath& source_path,
      const base::FilePath& target_path,
      const storage::AsyncFileUtil::StatusCallback& callback) override;
  AbortCallback MoveEntry(
      const base::FilePath& source_path,
      const base::FilePath& target_path,
      const storage::AsyncFileUtil::StatusCallback& callback) override;
  AbortCallback Truncate(
      const base::FilePath& file_path,
      int64 length,
      const storage::AsyncFileUtil::StatusCallback& callback) override;
  AbortCallback WriteFile(
      int file_handle,
      net::IOBuffer* buffer,
      int64 offset,
      int length,
      const storage::AsyncFileUtil::StatusCallback& callback) override;
  AbortCallback AddWatcher(
      const GURL& origin,
      const base::FilePath& entry_path,
      bool recursive,
      bool persistent,
      const storage::AsyncFileUtil::StatusCallback& callback,
      const storage::WatcherManager::NotificationCallback&
          notification_callback) override;
  void RemoveWatcher(
      const GURL& origin,
      const base::FilePath& entry_path,
      bool recursive,
      const storage::AsyncFileUtil::StatusCallback& callback) override;
  const ProvidedFileSystemInfo& GetFileSystemInfo() const override;
  RequestManager* GetRequestManager() override;
  Watchers* GetWatchers() override;
  const OpenedFiles& GetOpenedFiles() const override;
  void AddObserver(ProvidedFileSystemObserver* observer) override;
  void RemoveObserver(ProvidedFileSystemObserver* observer) override;
  void Notify(const base::FilePath& entry_path,
              bool recursive,
              storage::WatcherManager::ChangeType change_type,
              scoped_ptr<ProvidedFileSystemObserver::Changes> changes,
              const std::string& tag,
              const storage::AsyncFileUtil::StatusCallback& callback) override;
  void Configure(
      const storage::AsyncFileUtil::StatusCallback& callback) override;
  base::WeakPtr<ProvidedFileSystemInterface> GetWeakPtr() override;

 private:
  // Wrapper for arguments for AddWatcherInQueue, as it's too many of them to
  // be used by base::Bind.
  struct AddWatcherInQueueArgs;

  // Wrapper for arguments for NotifyInQueue, as it's too many of them to be
  // used by base::Bind.
  struct NotifyInQueueArgs;

  // Aborts an operation executed with a request id equal to
  // |operation_request_id|. The request is removed immediately on the C++ side
  // despite being handled by the providing extension or not.
  void Abort(int operation_request_id);

  // Called when aborting is completed with either a success or an error.
  void OnAbortCompleted(int operation_request_id, base::File::Error result);

  // Adds a watcher within |watcher_queue_|.
  AbortCallback AddWatcherInQueue(const AddWatcherInQueueArgs& args);

  // Removes a watcher within |watcher_queue_|.
  AbortCallback RemoveWatcherInQueue(
      size_t token,
      const GURL& origin,
      const base::FilePath& entry_path,
      bool recursive,
      const storage::AsyncFileUtil::StatusCallback& callback);

  // Notifies about a notifier even within |watcher_queue_|.
  AbortCallback NotifyInQueue(scoped_ptr<NotifyInQueueArgs> args);

  // Called when adding a watcher is completed with either success or en error.
  void OnAddWatcherInQueueCompleted(
      size_t token,
      const base::FilePath& entry_path,
      bool recursive,
      const Subscriber& subscriber,
      const storage::AsyncFileUtil::StatusCallback& callback,
      base::File::Error result);

  // Called when adding a watcher is completed with either a success or an
  // error.
  void OnRemoveWatcherInQueueCompleted(
      size_t token,
      const GURL& origin,
      const WatcherKey& key,
      const storage::AsyncFileUtil::StatusCallback& callback,
      bool extension_response,
      base::File::Error result);

  // Called when all observers finished handling the change notification. It
  // updates the tag to |tag| for the entry at |entry_path|.
  void OnNotifyInQueueCompleted(scoped_ptr<NotifyInQueueArgs> args,
                                const base::File::Error result);

  // Called when opening a file is completed with either a success or an error.
  void OnOpenFileCompleted(const base::FilePath& file_path,
                           OpenFileMode mode,
                           const OpenFileCallback& callback,
                           int file_handle,
                           base::File::Error result);

  // Called when closing a file is completed with either a success or an error.
  void OnCloseFileCompleted(
      int file_handle,
      const storage::AsyncFileUtil::StatusCallback& callback,
      base::File::Error result);

  Profile* profile_;                       // Not owned.
  extensions::EventRouter* event_router_;  // Not owned. May be NULL.
  ProvidedFileSystemInfo file_system_info_;
  scoped_ptr<NotificationManagerInterface> notification_manager_;
  scoped_ptr<RequestManager> request_manager_;
  Watchers watchers_;
  Queue watcher_queue_;
  OpenedFiles opened_files_;
  base::ObserverList<ProvidedFileSystemObserver> observers_;

  base::WeakPtrFactory<ProvidedFileSystem> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(ProvidedFileSystem);
};

}  // namespace file_system_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_PROVIDED_FILE_SYSTEM_H_
