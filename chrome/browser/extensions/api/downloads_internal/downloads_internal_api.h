// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DOWNLOADS_INTERNAL_DOWNLOADS_INTERNAL_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_DOWNLOADS_INTERNAL_DOWNLOADS_INTERNAL_API_H_

#include "chrome/browser/extensions/chrome_extension_function.h"

namespace extensions {

class DownloadsInternalDetermineFilenameFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("downloadsInternal.determineFilename",
                             DOWNLOADSINTERNAL_DETERMINEFILENAME);
  DownloadsInternalDetermineFilenameFunction();
  bool RunAsync() override;

 protected:
  ~DownloadsInternalDetermineFilenameFunction() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadsInternalDetermineFilenameFunction);
};

}  // namespace extensions
#endif  // CHROME_BROWSER_EXTENSIONS_API_DOWNLOADS_INTERNAL_DOWNLOADS_INTERNAL_API_H_
