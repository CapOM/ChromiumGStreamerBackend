// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_RESOURCE_WEB_RESOURCE_SERVICE_H_
#define COMPONENTS_WEB_RESOURCE_WEB_RESOURCE_SERVICE_H_

#include <string>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/web_resource/resource_request_allowed_notifier.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "url/gurl.h"

class PrefService;

namespace base {
class DictionaryValue;
class Value;
}

namespace net {
class URLFetcher;
class URLRequestContextGetter;
}

namespace web_resource {

// A WebResourceService fetches JSON data from a web server and periodically
// refreshes it.
class WebResourceService
    : public net::URLFetcherDelegate,
      public ResourceRequestAllowedNotifier::Observer {
 public:
  // Callbacks for JSON parsing.
  using SuccessCallback = base::Callback<void(scoped_ptr<base::Value>)>;
  using ErrorCallback = base::Callback<void(const std::string&)>;
  using ParseJSONCallback = base::Callback<
      void(const std::string&, const SuccessCallback&, const ErrorCallback&)>;

  // Creates a new WebResourceService.
  // If |application_locale| is not empty, it will be appended as a locale
  // parameter to the resource URL.
  WebResourceService(PrefService* prefs,
                     const GURL& web_resource_server,
                     const std::string& application_locale,  // May be empty
                     const char* last_update_time_pref_name,
                     int start_fetch_delay_ms,
                     int cache_update_delay_ms,
                     net::URLRequestContextGetter* request_context,
                     const char* disable_network_switch,
                     const ParseJSONCallback& parse_json_callback);

  ~WebResourceService() override;

  // Sleep until cache needs to be updated, but always for at least
  // |start_fetch_delay_ms| so we don't interfere with startup.
  // Then begin updating resources.
  void StartAfterDelay();

 protected:
  PrefService* prefs_;

 private:
  // For the subclasses to process the result of a fetch.
  virtual void Unpack(const base::DictionaryValue& parsed_json) = 0;

  // net::URLFetcherDelegate implementation:
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  // Schedules a fetch after |delay_ms| milliseconds.
  void ScheduleFetch(int64 delay_ms);

  // Starts fetching data from the server.
  void StartFetch();

  // Set |in_fetch_| to false, clean up temp directories (in the future).
  void EndFetch();

  // Callbacks from the JSON parser.
  void OnUnpackFinished(scoped_ptr<base::Value> value);
  void OnUnpackError(const std::string& error_message);

  // Implements ResourceRequestAllowedNotifier::Observer.
  void OnResourceRequestsAllowed() override;

  // Helper class used to tell this service if it's allowed to make network
  // resource requests.
  ResourceRequestAllowedNotifier resource_request_allowed_notifier_;

  // The tool that fetches the url data from the server.
  scoped_ptr<net::URLFetcher> url_fetcher_;

  // True if we are currently fetching or unpacking data. If we are asked to
  // start a fetch when we are still fetching resource data, schedule another
  // one in |cache_update_delay_ms_| time, and silently exit.
  bool in_fetch_;

  // URL that hosts the web resource.
  GURL web_resource_server_;

  // Application locale, appended to the URL if not empty.
  std::string application_locale_;

  // Pref name to store the last update's time.
  const char* last_update_time_pref_name_;

  // Delay on first fetch so we don't interfere with startup.
  int start_fetch_delay_ms_;

  // Delay between calls to update the web resource cache. This delay may be
  // different for different builds of Chrome.
  int cache_update_delay_ms_;

  // The request context for the resource fetch.
  scoped_refptr<net::URLRequestContextGetter> request_context_;

  // Callback used to parse JSON.
  ParseJSONCallback parse_json_callback_;

  // So that we can delay our start so as not to affect start-up time; also,
  // so that we can schedule future cache updates.
  base::WeakPtrFactory<WebResourceService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebResourceService);
};

}  // namespace web_resource

#endif  // COMPONENTS_WEB_RESOURCE_WEB_RESOURCE_SERVICE_H_
