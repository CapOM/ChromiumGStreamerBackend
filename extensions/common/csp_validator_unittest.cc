// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/csp_validator.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/install_warning.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::csp_validator::ContentSecurityPolicyIsLegal;
using extensions::csp_validator::SanitizeContentSecurityPolicy;
using extensions::csp_validator::ContentSecurityPolicyIsSandboxed;
using extensions::csp_validator::OPTIONS_NONE;
using extensions::csp_validator::OPTIONS_ALLOW_UNSAFE_EVAL;
using extensions::csp_validator::OPTIONS_ALLOW_INSECURE_OBJECT_SRC;
using extensions::ErrorUtils;
using extensions::InstallWarning;
using extensions::Manifest;

namespace {

std::string InsecureValueWarning(const std::string& directive,
                                 const std::string& value) {
  return ErrorUtils::FormatErrorMessage(
      extensions::manifest_errors::kInvalidCSPInsecureValue, value, directive);
}

std::string MissingSecureSrcWarning(const std::string& directive) {
  return ErrorUtils::FormatErrorMessage(
      extensions::manifest_errors::kInvalidCSPMissingSecureSrc, directive);
}

testing::AssertionResult CheckSanitizeCSP(
    const std::string& policy,
    int options,
    const std::string& expected_csp,
    const std::vector<std::string>& expected_warnings) {
  std::vector<InstallWarning> actual_warnings;
  std::string actual_csp = SanitizeContentSecurityPolicy(policy,
                                                         options,
                                                         &actual_warnings);
  if (actual_csp != expected_csp)
    return testing::AssertionFailure()
        << "SanitizeContentSecurityPolicy returned an unexpected CSP.\n"
        << "Expected CSP: " << expected_csp << "\n"
        << "  Actual CSP: " << actual_csp;

  if (expected_warnings.size() != actual_warnings.size()) {
    testing::Message msg;
    msg << "Expected " << expected_warnings.size()
        << " warnings, but got " << actual_warnings.size();
    for (size_t i = 0; i < actual_warnings.size(); ++i)
      msg << "\nWarning " << i << " " << actual_warnings[i].message;
    return testing::AssertionFailure() << msg;
  }

  for (size_t i = 0; i < expected_warnings.size(); ++i) {
    if (expected_warnings[i] != actual_warnings[i].message)
      return testing::AssertionFailure()
          << "Unexpected warning from SanitizeContentSecurityPolicy.\n"
          << "Expected warning[" << i << "]: " << expected_warnings[i]
          << "  Actual warning[" << i << "]: " << actual_warnings[i].message;
  }
  return testing::AssertionSuccess();
}

testing::AssertionResult CheckSanitizeCSP(const std::string& policy,
                                          int options) {
  return CheckSanitizeCSP(policy, options, policy, std::vector<std::string>());
}

testing::AssertionResult CheckSanitizeCSP(const std::string& policy,
                                          int options,
                                          const std::string& expected_csp) {
  std::vector<std::string> expected_warnings;
  return CheckSanitizeCSP(policy, options, expected_csp, expected_warnings);
}

testing::AssertionResult CheckSanitizeCSP(const std::string& policy,
                                          int options,
                                          const std::string& expected_csp,
                                          const std::string& warning1) {
  std::vector<std::string> expected_warnings(1, warning1);
  return CheckSanitizeCSP(policy, options, expected_csp, expected_warnings);
}

testing::AssertionResult CheckSanitizeCSP(const std::string& policy,
                                          int options,
                                          const std::string& expected_csp,
                                          const std::string& warning1,
                                          const std::string& warning2) {
  std::vector<std::string> expected_warnings(1, warning1);
  expected_warnings.push_back(warning2);
  return CheckSanitizeCSP(policy, options, expected_csp, expected_warnings);
}

testing::AssertionResult CheckSanitizeCSP(const std::string& policy,
                                          int options,
                                          const std::string& expected_csp,
                                          const std::string& warning1,
                                          const std::string& warning2,
                                          const std::string& warning3) {
  std::vector<std::string> expected_warnings(1, warning1);
  expected_warnings.push_back(warning2);
  expected_warnings.push_back(warning3);
  return CheckSanitizeCSP(policy, options, expected_csp, expected_warnings);
}

};  // namespace

TEST(ExtensionCSPValidator, IsLegal) {
  EXPECT_TRUE(ContentSecurityPolicyIsLegal("foo"));
  EXPECT_TRUE(ContentSecurityPolicyIsLegal(
      "default-src 'self'; script-src http://www.google.com"));
  EXPECT_FALSE(ContentSecurityPolicyIsLegal(
      "default-src 'self';\nscript-src http://www.google.com"));
  EXPECT_FALSE(ContentSecurityPolicyIsLegal(
      "default-src 'self';\rscript-src http://www.google.com"));
  EXPECT_FALSE(ContentSecurityPolicyIsLegal(
      "default-src 'self';,script-src http://www.google.com"));
}

TEST(ExtensionCSPValidator, IsSecure) {
  EXPECT_TRUE(CheckSanitizeCSP(
      std::string(), OPTIONS_ALLOW_UNSAFE_EVAL,
      "script-src 'self' chrome-extension-resource:; object-src 'self';",
      MissingSecureSrcWarning("script-src"),
      MissingSecureSrcWarning("object-src")));
  EXPECT_TRUE(CheckSanitizeCSP(
      "img-src https://google.com", OPTIONS_ALLOW_UNSAFE_EVAL,
      "img-src https://google.com; script-src 'self'"
      " chrome-extension-resource:; object-src 'self';",
      MissingSecureSrcWarning("script-src"),
      MissingSecureSrcWarning("object-src")));
  EXPECT_TRUE(CheckSanitizeCSP(
      "script-src a b", OPTIONS_ALLOW_UNSAFE_EVAL,
      "script-src; object-src 'self';",
      InsecureValueWarning("script-src", "a"),
      InsecureValueWarning("script-src", "b"),
      MissingSecureSrcWarning("object-src")));

  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src *", OPTIONS_ALLOW_UNSAFE_EVAL,
      "default-src;",
      InsecureValueWarning("default-src", "*")));
  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src 'self';", OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src 'none';", OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src 'self' ftp://google.com", OPTIONS_ALLOW_UNSAFE_EVAL,
      "default-src 'self';",
      InsecureValueWarning("default-src", "ftp://google.com")));
  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src 'self' https://google.com;", OPTIONS_ALLOW_UNSAFE_EVAL));

  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src *; default-src 'self'", OPTIONS_ALLOW_UNSAFE_EVAL,
      "default-src; default-src 'self';",
      InsecureValueWarning("default-src", "*")));
  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src 'self'; default-src *;", OPTIONS_ALLOW_UNSAFE_EVAL,
      "default-src 'self'; default-src;"));
  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src 'self'; default-src *; script-src *; script-src 'self'",
      OPTIONS_ALLOW_UNSAFE_EVAL,
      "default-src 'self'; default-src; script-src; script-src 'self';",
      InsecureValueWarning("script-src", "*")));
  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src 'self'; default-src *; script-src 'self'; script-src *;",
      OPTIONS_ALLOW_UNSAFE_EVAL,
      "default-src 'self'; default-src; script-src 'self'; script-src;"));
  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src *; script-src 'self'", OPTIONS_ALLOW_UNSAFE_EVAL,
      "default-src; script-src 'self';",
      InsecureValueWarning("default-src", "*")));
  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src *; script-src 'self'; img-src 'self'",
      OPTIONS_ALLOW_UNSAFE_EVAL,
      "default-src; script-src 'self'; img-src 'self';",
      InsecureValueWarning("default-src", "*")));
  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src *; script-src 'self'; object-src 'self';",
      OPTIONS_ALLOW_UNSAFE_EVAL,
      "default-src; script-src 'self'; object-src 'self';"));
  EXPECT_TRUE(CheckSanitizeCSP(
      "script-src 'self'; object-src 'self';", OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src 'unsafe-eval';", OPTIONS_ALLOW_UNSAFE_EVAL));

  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src 'unsafe-eval'", OPTIONS_NONE,
      "default-src;",
      InsecureValueWarning("default-src", "'unsafe-eval'")));
  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src 'unsafe-inline'", OPTIONS_ALLOW_UNSAFE_EVAL,
      "default-src;",
      InsecureValueWarning("default-src", "'unsafe-inline'")));
  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src 'unsafe-inline' 'none'", OPTIONS_ALLOW_UNSAFE_EVAL,
      "default-src 'none';",
      InsecureValueWarning("default-src", "'unsafe-inline'")));
  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src 'self' http://google.com", OPTIONS_ALLOW_UNSAFE_EVAL,
      "default-src 'self';",
      InsecureValueWarning("default-src", "http://google.com")));
  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src 'self' https://google.com;", OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src 'self' chrome://resources;", OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src 'self' chrome-extension://aabbcc;",
      OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src 'self' chrome-extension-resource://aabbcc;",
      OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src 'self' https:", OPTIONS_ALLOW_UNSAFE_EVAL,
      "default-src 'self';",
      InsecureValueWarning("default-src", "https:")));
  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src 'self' http:", OPTIONS_ALLOW_UNSAFE_EVAL,
      "default-src 'self';",
      InsecureValueWarning("default-src", "http:")));
  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src 'self' google.com", OPTIONS_ALLOW_UNSAFE_EVAL,
      "default-src 'self';",
      InsecureValueWarning("default-src", "google.com")));

  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src 'self' *", OPTIONS_ALLOW_UNSAFE_EVAL,
      "default-src 'self';",
      InsecureValueWarning("default-src", "*")));
  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src 'self' *:*", OPTIONS_ALLOW_UNSAFE_EVAL,
      "default-src 'self';",
      InsecureValueWarning("default-src", "*:*")));
  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src 'self' *:*/", OPTIONS_ALLOW_UNSAFE_EVAL,
      "default-src 'self';",
      InsecureValueWarning("default-src", "*:*/")));
  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src 'self' *:*/path", OPTIONS_ALLOW_UNSAFE_EVAL,
      "default-src 'self';",
      InsecureValueWarning("default-src", "*:*/path")));
  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src 'self' https://", OPTIONS_ALLOW_UNSAFE_EVAL,
      "default-src 'self';",
      InsecureValueWarning("default-src", "https://")));
  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src 'self' https://*:*", OPTIONS_ALLOW_UNSAFE_EVAL,
      "default-src 'self';",
      InsecureValueWarning("default-src", "https://*:*")));
  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src 'self' https://*:*/", OPTIONS_ALLOW_UNSAFE_EVAL,
      "default-src 'self';",
      InsecureValueWarning("default-src", "https://*:*/")));
  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src 'self' https://*:*/path", OPTIONS_ALLOW_UNSAFE_EVAL,
      "default-src 'self';",
      InsecureValueWarning("default-src", "https://*:*/path")));
  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src 'self' https://*.com", OPTIONS_ALLOW_UNSAFE_EVAL,
      "default-src 'self';",
      InsecureValueWarning("default-src", "https://*.com")));
  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src 'self' https://*.*.google.com/", OPTIONS_ALLOW_UNSAFE_EVAL,
      "default-src 'self';",
      InsecureValueWarning("default-src", "https://*.*.google.com/")));
  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src 'self' https://*.*.google.com:*/", OPTIONS_ALLOW_UNSAFE_EVAL,
      "default-src 'self';",
      InsecureValueWarning("default-src", "https://*.*.google.com:*/")));
  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src 'self' https://www.*.google.com/", OPTIONS_ALLOW_UNSAFE_EVAL,
      "default-src 'self';",
      InsecureValueWarning("default-src", "https://www.*.google.com/")));
  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src 'self' https://www.*.google.com:*/",
      OPTIONS_ALLOW_UNSAFE_EVAL,
      "default-src 'self';",
      InsecureValueWarning("default-src", "https://www.*.google.com:*/")));
  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src 'self' chrome://*", OPTIONS_ALLOW_UNSAFE_EVAL,
      "default-src 'self';",
      InsecureValueWarning("default-src", "chrome://*")));
  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src 'self' chrome-extension://*", OPTIONS_ALLOW_UNSAFE_EVAL,
      "default-src 'self';",
      InsecureValueWarning("default-src", "chrome-extension://*")));
  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src 'self' chrome-extension://", OPTIONS_ALLOW_UNSAFE_EVAL,
      "default-src 'self';",
      InsecureValueWarning("default-src", "chrome-extension://")));

  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src 'self' https://*.google.com;", OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src 'self' https://*.google.com:1;", OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src 'self' https://*.google.com:*;", OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src 'self' https://*.google.com:1/;",
      OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src 'self' https://*.google.com:*/;",
      OPTIONS_ALLOW_UNSAFE_EVAL));

  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src 'self' http://127.0.0.1;", OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src 'self' http://localhost;", OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_TRUE(CheckSanitizeCSP("default-src 'self' http://lOcAlHoSt;",
                               OPTIONS_ALLOW_UNSAFE_EVAL,
                               "default-src 'self' http://lOcAlHoSt;"));
  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src 'self' http://127.0.0.1:9999;", OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src 'self' http://localhost:8888;", OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src 'self' http://127.0.0.1.example.com",
      OPTIONS_ALLOW_UNSAFE_EVAL,
      "default-src 'self';",
      InsecureValueWarning("default-src", "http://127.0.0.1.example.com")));
  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src 'self' http://localhost.example.com",
      OPTIONS_ALLOW_UNSAFE_EVAL,
      "default-src 'self';",
      InsecureValueWarning("default-src", "http://localhost.example.com")));

  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src 'self' blob:;", OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src 'self' blob:http://example.com/XXX",
      OPTIONS_ALLOW_UNSAFE_EVAL, "default-src 'self';",
      InsecureValueWarning("default-src", "blob:http://example.com/XXX")));
  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src 'self' filesystem:;", OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src 'self' filesystem:http://example.com/XX",
      OPTIONS_ALLOW_UNSAFE_EVAL, "default-src 'self';",
      InsecureValueWarning("default-src", "filesystem:http://example.com/XX")));

  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src 'self' https://*.googleapis.com;",
      OPTIONS_ALLOW_UNSAFE_EVAL));
  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src 'self' https://x.googleapis.com;",
      OPTIONS_ALLOW_UNSAFE_EVAL));

  EXPECT_TRUE(CheckSanitizeCSP(
      "script-src 'self'; object-src *", OPTIONS_NONE,
      "script-src 'self'; object-src;",
      InsecureValueWarning("object-src", "*")));
  EXPECT_TRUE(CheckSanitizeCSP(
      "script-src 'self'; object-src *", OPTIONS_ALLOW_INSECURE_OBJECT_SRC,
      "script-src 'self'; object-src;",
      InsecureValueWarning("object-src", "*")));
  EXPECT_TRUE(CheckSanitizeCSP(
      "script-src 'self'; object-src *; plugin-types application/pdf;",
      OPTIONS_ALLOW_INSECURE_OBJECT_SRC));
  EXPECT_TRUE(CheckSanitizeCSP(
      "script-src 'self'; object-src *; "
      "plugin-types application/x-shockwave-flash",
      OPTIONS_ALLOW_INSECURE_OBJECT_SRC,
      "script-src 'self'; object-src; "
      "plugin-types application/x-shockwave-flash;",
      InsecureValueWarning("object-src", "*")));
  EXPECT_TRUE(CheckSanitizeCSP(
      "script-src 'self'; object-src *; "
      "plugin-types application/x-shockwave-flash application/pdf;",
      OPTIONS_ALLOW_INSECURE_OBJECT_SRC,
      "script-src 'self'; object-src; "
      "plugin-types application/x-shockwave-flash application/pdf;",
      InsecureValueWarning("object-src", "*")));
  EXPECT_TRUE(CheckSanitizeCSP(
      "script-src 'self'; object-src http://www.example.com; "
      "plugin-types application/pdf;",
      OPTIONS_ALLOW_INSECURE_OBJECT_SRC));
  EXPECT_TRUE(CheckSanitizeCSP(
      "object-src http://www.example.com blob:; script-src 'self'; "
      "plugin-types application/pdf;",
      OPTIONS_ALLOW_INSECURE_OBJECT_SRC));
  EXPECT_TRUE(CheckSanitizeCSP(
      "script-src 'self'; object-src http://*.example.com; "
      "plugin-types application/pdf;",
      OPTIONS_ALLOW_INSECURE_OBJECT_SRC));
  EXPECT_TRUE(CheckSanitizeCSP(
      "script-src *; object-src *; plugin-types application/pdf;",
      OPTIONS_ALLOW_INSECURE_OBJECT_SRC,
      "script-src; object-src *; plugin-types application/pdf;",
      InsecureValueWarning("script-src", "*")));

  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src; script-src"
      " 'sha256-hndjYvzUzy2Ykuad81Cwsl1FOXX/qYs/aDVyUyNZwBw='"
      " 'sha384-bSVm1i3sjPBRM4TwZtYTDjk9JxZMExYHWbFmP1SxDhJH4ue0Wu9OPOkY5hcqRcS"
      "t'"
      " 'sha512-440MmBLtj9Kp5Bqloogn9BqGDylY8vFsv5/zXL1zH2fJVssCoskRig4gyM+9Kqw"
      "vCSapSz5CVoUGHQcxv43UQg==';",
      OPTIONS_NONE));

  // Reject non-standard algorithms, even if they are still supported by Blink.
  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src; script-src 'sha1-eYyYGmKWdhpUewohaXk9o8IaLSw=';",
      OPTIONS_NONE, "default-src; script-src;",
      InsecureValueWarning("script-src",
                           "'sha1-eYyYGmKWdhpUewohaXk9o8IaLSw='")));

  EXPECT_TRUE(CheckSanitizeCSP(
      "default-src; script-src 'sha256-hndjYvzUzy2Ykuad81Cwsl1FOXX/qYs/aDVyUyNZ"
      "wBw= sha256-qznLcsROx4GACP2dm0UCKCzCG+HiZ1guq6ZZDob/Tng=';",
      OPTIONS_NONE, "default-src; script-src;",
      InsecureValueWarning(
          "script-src", "'sha256-hndjYvzUzy2Ykuad81Cwsl1FOXX/qYs/aDVyUyNZwBw="),
      InsecureValueWarning(
          "script-src",
          "sha256-qznLcsROx4GACP2dm0UCKCzCG+HiZ1guq6ZZDob/Tng='")));
}

TEST(ExtensionCSPValidator, IsSandboxed) {
  EXPECT_FALSE(ContentSecurityPolicyIsSandboxed(std::string(),
                                                Manifest::TYPE_EXTENSION));
  EXPECT_FALSE(ContentSecurityPolicyIsSandboxed("img-src https://google.com",
                                                Manifest::TYPE_EXTENSION));

  // Sandbox directive is required.
  EXPECT_TRUE(ContentSecurityPolicyIsSandboxed(
      "sandbox", Manifest::TYPE_EXTENSION));

  // Additional sandbox tokens are OK.
  EXPECT_TRUE(ContentSecurityPolicyIsSandboxed(
      "sandbox allow-scripts", Manifest::TYPE_EXTENSION));
  // Except for allow-same-origin.
  EXPECT_FALSE(ContentSecurityPolicyIsSandboxed(
      "sandbox allow-same-origin", Manifest::TYPE_EXTENSION));

  // Additional directives are OK.
  EXPECT_TRUE(ContentSecurityPolicyIsSandboxed(
      "sandbox; img-src https://google.com", Manifest::TYPE_EXTENSION));

  // Extensions allow navigation, platform apps don't.
  EXPECT_TRUE(ContentSecurityPolicyIsSandboxed(
      "sandbox allow-top-navigation", Manifest::TYPE_EXTENSION));
  EXPECT_FALSE(ContentSecurityPolicyIsSandboxed(
      "sandbox allow-top-navigation", Manifest::TYPE_PLATFORM_APP));

  // Popups are OK.
  EXPECT_TRUE(ContentSecurityPolicyIsSandboxed(
      "sandbox allow-popups", Manifest::TYPE_EXTENSION));
  EXPECT_TRUE(ContentSecurityPolicyIsSandboxed(
      "sandbox allow-popups", Manifest::TYPE_PLATFORM_APP));
}
