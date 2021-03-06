// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_SESSION_SYNC_TEST_HELPER_H_
#define CHROME_BROWSER_SYNC_GLUE_SESSION_SYNC_TEST_HELPER_H_

#include <string>
#include <vector>

#include "components/sessions/session_id.h"

namespace sync_driver {
struct SyncedSession;
}

namespace sync_pb {
class SessionSpecifics;
}

namespace browser_sync {

class SessionSyncTestHelper {
 public:
  SessionSyncTestHelper() : max_tab_node_id_(0) {}

  static void BuildSessionSpecifics(const std::string& tag,
                                    sync_pb::SessionSpecifics* meta);

  static void AddWindowSpecifics(int window_id,
                                 const std::vector<int>& tab_list,
                                 sync_pb::SessionSpecifics* meta);

  static void VerifySyncedSession(
      const std::string& tag,
      const std::vector<std::vector<SessionID::id_type>>& windows,
      const sync_driver::SyncedSession& session);

  void BuildTabSpecifics(const std::string& tag,
                         int window_id,
                         int tab_id,
                         sync_pb::SessionSpecifics* tab_base);

  sync_pb::SessionSpecifics BuildForeignSession(
      const std::string& tag,
      const std::vector<SessionID::id_type>& tab_list,
      std::vector<sync_pb::SessionSpecifics>* tabs);

  void Reset();

 private:
  int max_tab_node_id_;
  DISALLOW_COPY_AND_ASSIGN(SessionSyncTestHelper);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_SESSION_SYNC_TEST_HELPER_H_
