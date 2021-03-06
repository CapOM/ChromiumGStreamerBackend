// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/cast_channel/cast_auth_util.h"

#include <string>

#include "base/macros.h"
#include "extensions/browser/api/cast_channel/cast_auth_ica.h"
#include "extensions/common/api/cast_channel/cast_channel.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace api {
namespace cast_channel {
namespace {

const unsigned char kIntermediateCertificate[] = {
  0x30, 0x82, 0x03, 0x87, 0x30, 0x82, 0x02, 0x6f, 0xa0, 0x03,
  0x02, 0x01, 0x02, 0x02, 0x01, 0x01, 0x30, 0x0d, 0x06, 0x09,
  0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x05, 0x05,
  0x00, 0x30, 0x7c, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55,
  0x04, 0x06, 0x13, 0x02, 0x55, 0x53, 0x31, 0x13, 0x30, 0x11,
  0x06, 0x03, 0x55, 0x04, 0x08, 0x0c, 0x0a, 0x43, 0x61, 0x6c,
  0x69, 0x66, 0x6f, 0x72, 0x6e, 0x69, 0x61, 0x31, 0x16, 0x30,
  0x14, 0x06, 0x03, 0x55, 0x04, 0x07, 0x0c, 0x0d, 0x4d, 0x6f,
  0x75, 0x6e, 0x74, 0x61, 0x69, 0x6e, 0x20, 0x56, 0x69, 0x65,
  0x77, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03, 0x55, 0x04, 0x0a,
  0x0c, 0x0a, 0x47, 0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x20, 0x49,
  0x6e, 0x63, 0x31, 0x12, 0x30, 0x10, 0x06, 0x03, 0x55, 0x04,
  0x0b, 0x0c, 0x09, 0x47, 0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x20,
  0x54, 0x56, 0x31, 0x17, 0x30, 0x15, 0x06, 0x03, 0x55, 0x04,
  0x03, 0x0c, 0x0e, 0x45, 0x75, 0x72, 0x65, 0x6b, 0x61, 0x20,
  0x52, 0x6f, 0x6f, 0x74, 0x20, 0x43, 0x41, 0x30, 0x1e, 0x17,
  0x0d, 0x31, 0x32, 0x31, 0x32, 0x31, 0x39, 0x30, 0x30, 0x34,
  0x37, 0x31, 0x32, 0x5a, 0x17, 0x0d, 0x33, 0x32, 0x31, 0x32,
  0x31, 0x34, 0x30, 0x30, 0x34, 0x37, 0x31, 0x32, 0x5a, 0x30,
  0x7d, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06,
  0x13, 0x02, 0x55, 0x53, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03,
  0x55, 0x04, 0x08, 0x0c, 0x0a, 0x43, 0x61, 0x6c, 0x69, 0x66,
  0x6f, 0x72, 0x6e, 0x69, 0x61, 0x31, 0x16, 0x30, 0x14, 0x06,
  0x03, 0x55, 0x04, 0x07, 0x0c, 0x0d, 0x4d, 0x6f, 0x75, 0x6e,
  0x74, 0x61, 0x69, 0x6e, 0x20, 0x56, 0x69, 0x65, 0x77, 0x31,
  0x13, 0x30, 0x11, 0x06, 0x03, 0x55, 0x04, 0x0a, 0x0c, 0x0a,
  0x47, 0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x20, 0x49, 0x6e, 0x63,
  0x31, 0x12, 0x30, 0x10, 0x06, 0x03, 0x55, 0x04, 0x0b, 0x0c,
  0x09, 0x47, 0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x20, 0x54, 0x56,
  0x31, 0x18, 0x30, 0x16, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0c,
  0x0f, 0x45, 0x75, 0x72, 0x65, 0x6b, 0x61, 0x20, 0x47, 0x65,
  0x6e, 0x31, 0x20, 0x49, 0x43, 0x41, 0x30, 0x82, 0x01, 0x22,
  0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d,
  0x01, 0x01, 0x01, 0x05, 0x00, 0x03, 0x82, 0x01, 0x0f, 0x00,
  0x30, 0x82, 0x01, 0x0a, 0x02, 0x82, 0x01, 0x01, 0x00, 0xbc,
  0x22, 0x80, 0xbd, 0x80, 0xf6, 0x3a, 0x21, 0x00, 0x3b, 0xae,
  0x76, 0x5e, 0x35, 0x7f, 0x3d, 0xc3, 0x64, 0x5c, 0x55, 0x94,
  0x86, 0x34, 0x2f, 0x05, 0x87, 0x28, 0xcd, 0xf7, 0x69, 0x8c,
  0x17, 0xb3, 0x50, 0xa7, 0xb8, 0x82, 0xfa, 0xdf, 0xc7, 0x43,
  0x2d, 0xd6, 0x7e, 0xab, 0xa0, 0x6f, 0xb7, 0x13, 0x72, 0x80,
  0xa4, 0x47, 0x15, 0xc1, 0x20, 0x99, 0x50, 0xcd, 0xec, 0x14,
  0x62, 0x09, 0x5b, 0xa4, 0x98, 0xcd, 0xd2, 0x41, 0xb6, 0x36,
  0x4e, 0xff, 0xe8, 0x2e, 0x32, 0x30, 0x4a, 0x81, 0xa8, 0x42,
  0xa3, 0x6c, 0x9b, 0x33, 0x6e, 0xca, 0xb2, 0xf5, 0x53, 0x66,
  0xe0, 0x27, 0x53, 0x86, 0x1a, 0x85, 0x1e, 0xa7, 0x39, 0x3f,
  0x4a, 0x77, 0x8e, 0xfb, 0x54, 0x66, 0x66, 0xfb, 0x58, 0x54,
  0xc0, 0x5e, 0x39, 0xc7, 0xf5, 0x50, 0x06, 0x0b, 0xe0, 0x8a,
  0xd4, 0xce, 0xe1, 0x6a, 0x55, 0x1f, 0x8b, 0x17, 0x00, 0xe6,
  0x69, 0xa3, 0x27, 0xe6, 0x08, 0x25, 0x69, 0x3c, 0x12, 0x9d,
  0x8d, 0x05, 0x2c, 0xd6, 0x2e, 0xa2, 0x31, 0xde, 0xb4, 0x52,
  0x50, 0xd6, 0x20, 0x49, 0xde, 0x71, 0xa0, 0xf9, 0xad, 0x20,
  0x40, 0x12, 0xf1, 0xdd, 0x25, 0xeb, 0xd5, 0xe6, 0xb8, 0x36,
  0xf4, 0xd6, 0x8f, 0x7f, 0xca, 0x43, 0xdc, 0xd7, 0x10, 0x5b,
  0xe6, 0x3f, 0x51, 0x8a, 0x85, 0xb3, 0xf3, 0xff, 0xf6, 0x03,
  0x2d, 0xcb, 0x23, 0x4f, 0x9c, 0xad, 0x18, 0xe7, 0x93, 0x05,
  0x8c, 0xac, 0x52, 0x9a, 0xf7, 0x4c, 0xe9, 0x99, 0x7a, 0xbe,
  0x6e, 0x7e, 0x4d, 0x0a, 0xe3, 0xc6, 0x1c, 0xa9, 0x93, 0xfa,
  0x3a, 0xa5, 0x91, 0x5d, 0x1c, 0xbd, 0x66, 0xeb, 0xcc, 0x60,
  0xdc, 0x86, 0x74, 0xca, 0xcf, 0xf8, 0x92, 0x1c, 0x98, 0x7d,
  0x57, 0xfa, 0x61, 0x47, 0x9e, 0xab, 0x80, 0xb7, 0xe4, 0x48,
  0x80, 0x2a, 0x92, 0xc5, 0x1b, 0x02, 0x03, 0x01, 0x00, 0x01,
  0xa3, 0x13, 0x30, 0x11, 0x30, 0x0f, 0x06, 0x03, 0x55, 0x1d,
  0x13, 0x04, 0x08, 0x30, 0x06, 0x01, 0x01, 0xff, 0x02, 0x01,
  0x01, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7,
  0x0d, 0x01, 0x01, 0x05, 0x05, 0x00, 0x03, 0x82, 0x01, 0x01,
  0x00, 0x8b, 0xd4, 0xa1, 0xb1, 0xcf, 0x5d, 0xcd, 0x7b, 0x6c,
  0x48, 0x4a, 0x41, 0x1f, 0x53, 0x2f, 0x18, 0x2d, 0x32, 0x45,
  0xff, 0x9e, 0xab, 0xd3, 0x73, 0x3e, 0x1f, 0x22, 0xd7, 0xea,
  0xfa, 0x01, 0xe6, 0x73, 0x03, 0x0f, 0x2b, 0xc6, 0x25, 0xbb,
  0xa5, 0xee, 0xc5, 0xf5, 0x45, 0xcb, 0x24, 0x12, 0x2a, 0xad,
  0xc2, 0x5d, 0x05, 0xf4, 0x7a, 0xf5, 0xc2, 0x9b, 0x10, 0x16,
  0x5a, 0xd1, 0x0a, 0x73, 0xc5, 0x16, 0x39, 0xa0, 0x10, 0xca,
  0xd1, 0x68, 0x85, 0x9e, 0xfb, 0x9e, 0x26, 0x83, 0x8e, 0x58,
  0xf3, 0x77, 0xa0, 0x4e, 0xe5, 0xdb, 0x97, 0xbe, 0x2d, 0x00,
  0x5f, 0xf5, 0x94, 0xdb, 0xb1, 0x9d, 0x65, 0x6b, 0xfd, 0xf0,
  0xd1, 0x04, 0x51, 0xdf, 0xcc, 0x92, 0xa6, 0x99, 0x2d, 0x71,
  0xf5, 0x4d, 0xd5, 0x23, 0xfe, 0x33, 0x1c, 0xa9, 0xb4, 0xab,
  0xc5, 0xbf, 0x1a, 0xb8, 0xd1, 0x80, 0xef, 0x89, 0xc9, 0xe2,
  0x1f, 0x9c, 0x4c, 0x48, 0x3b, 0xa2, 0xfa, 0x02, 0x0a, 0xdc,
  0x84, 0x01, 0x8a, 0x87, 0x02, 0xfb, 0x59, 0xee, 0xa7, 0x4c,
  0x04, 0x7d, 0x74, 0x99, 0x87, 0x6a, 0x25, 0x44, 0xad, 0x16,
  0xaa, 0xec, 0x4e, 0x35, 0x1b, 0x7c, 0x7b, 0x84, 0xc9, 0xb1,
  0x3f, 0xe1, 0x82, 0x70, 0xe5, 0x0d, 0xe7, 0xd9, 0x6d, 0xfa,
  0x95, 0xb6, 0xc5, 0xe4, 0x1e, 0xe8, 0x11, 0x9b, 0xd8, 0xb2,
  0xf3, 0xa4, 0xfd, 0x13, 0xf3, 0x83, 0x4f, 0xf7, 0x07, 0x14,
  0x20, 0xbb, 0x22, 0xa5, 0xa6, 0x8f, 0xd6, 0xb5, 0xdb, 0xa9,
  0x74, 0x78, 0xe2, 0x93, 0x0d, 0xe5, 0x23, 0x2f, 0x05, 0x17,
  0xe0, 0xb2, 0x97, 0x67, 0x34, 0x4d, 0x0f, 0x9c, 0x76, 0x43,
  0x7b, 0xa6, 0x21, 0x4a, 0x56, 0x05, 0xf6, 0x2a, 0x7c, 0xf2,
  0x7f, 0x12, 0x94, 0x82, 0x26, 0x29, 0x07, 0xf0, 0x0b, 0x6c,
  0x6c, 0x79, 0x14, 0xb0, 0x74, 0xd5, 0x6c,
};

const unsigned char kClientAuthCertificate[] = {
  0x30, 0x82, 0x03, 0x7a, 0x30, 0x82, 0x02, 0x62, 0x02, 0x04,
  0x51, 0x1d, 0x0e, 0xe2, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86,
  0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x05, 0x05, 0x00, 0x30,
  0x7d, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06,
  0x13, 0x02, 0x55, 0x53, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03,
  0x55, 0x04, 0x08, 0x0c, 0x0a, 0x43, 0x61, 0x6c, 0x69, 0x66,
  0x6f, 0x72, 0x6e, 0x69, 0x61, 0x31, 0x16, 0x30, 0x14, 0x06,
  0x03, 0x55, 0x04, 0x07, 0x0c, 0x0d, 0x4d, 0x6f, 0x75, 0x6e,
  0x74, 0x61, 0x69, 0x6e, 0x20, 0x56, 0x69, 0x65, 0x77, 0x31,
  0x13, 0x30, 0x11, 0x06, 0x03, 0x55, 0x04, 0x0a, 0x0c, 0x0a,
  0x47, 0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x20, 0x49, 0x6e, 0x63,
  0x31, 0x12, 0x30, 0x10, 0x06, 0x03, 0x55, 0x04, 0x0b, 0x0c,
  0x09, 0x47, 0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x20, 0x54, 0x56,
  0x31, 0x18, 0x30, 0x16, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0c,
  0x0f, 0x45, 0x75, 0x72, 0x65, 0x6b, 0x61, 0x20, 0x47, 0x65,
  0x6e, 0x31, 0x20, 0x49, 0x43, 0x41, 0x30, 0x1e, 0x17, 0x0d,
  0x31, 0x33, 0x30, 0x32, 0x31, 0x34, 0x31, 0x36, 0x32, 0x30,
  0x35, 0x30, 0x5a, 0x17, 0x0d, 0x33, 0x33, 0x30, 0x32, 0x30,
  0x39, 0x31, 0x36, 0x32, 0x30, 0x35, 0x30, 0x5a, 0x30, 0x77,
  0x31, 0x12, 0x30, 0x10, 0x06, 0x03, 0x55, 0x04, 0x0b, 0x13,
  0x09, 0x47, 0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x20, 0x54, 0x56,
  0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13,
  0x02, 0x55, 0x53, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03, 0x55,
  0x04, 0x08, 0x13, 0x0a, 0x43, 0x61, 0x6c, 0x69, 0x66, 0x6f,
  0x72, 0x6e, 0x69, 0x61, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03,
  0x55, 0x04, 0x0a, 0x13, 0x0a, 0x47, 0x6f, 0x6f, 0x67, 0x6c,
  0x65, 0x20, 0x49, 0x6e, 0x63, 0x31, 0x16, 0x30, 0x14, 0x06,
  0x03, 0x55, 0x04, 0x07, 0x13, 0x0d, 0x4d, 0x6f, 0x75, 0x6e,
  0x74, 0x61, 0x69, 0x6e, 0x20, 0x56, 0x69, 0x65, 0x77, 0x31,
  0x12, 0x30, 0x10, 0x06, 0x03, 0x55, 0x04, 0x03, 0x14, 0x09,
  0x65, 0x76, 0x74, 0x5f, 0x65, 0x31, 0x32, 0x36, 0x36, 0x30,
  0x82, 0x01, 0x22, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48,
  0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00, 0x03, 0x82,
  0x01, 0x0f, 0x00, 0x30, 0x82, 0x01, 0x0a, 0x02, 0x82, 0x01,
  0x01, 0x00, 0xb8, 0x7d, 0x8a, 0x50, 0xfc, 0x58, 0xf2, 0x03,
  0x67, 0x7f, 0x9f, 0xe4, 0xab, 0xd0, 0xd6, 0xd2, 0xd5, 0x8a,
  0x7e, 0x30, 0x26, 0x4c, 0x54, 0xd1, 0xd0, 0xd4, 0x40, 0x92,
  0x6c, 0x7d, 0x18, 0x78, 0xe3, 0x96, 0xc1, 0x0d, 0xd6, 0x7d,
  0xca, 0x20, 0x32, 0x51, 0x30, 0xb5, 0x51, 0xcc, 0xe5, 0x21,
  0xf1, 0x61, 0x20, 0x5f, 0x18, 0x60, 0x42, 0x94, 0x8d, 0x8b,
  0x4c, 0x7a, 0x3f, 0xea, 0x11, 0xf6, 0x5a, 0xa0, 0x17, 0x60,
  0x7b, 0x55, 0xcf, 0x96, 0x6e, 0x59, 0xaa, 0x0a, 0xd7, 0x3c,
  0x69, 0xd9, 0x9d, 0x24, 0x4b, 0xc2, 0xde, 0xc1, 0x4c, 0xaa,
  0x14, 0xd3, 0x77, 0xa9, 0x7a, 0x94, 0x68, 0x79, 0x73, 0xb3,
  0xf5, 0x09, 0x28, 0xfc, 0x18, 0x20, 0xbd, 0xef, 0xf5, 0xf0,
  0xf8, 0xd3, 0x53, 0x9e, 0x5f, 0x55, 0xcb, 0x11, 0xab, 0xea,
  0x7b, 0xfe, 0x21, 0x44, 0x30, 0x2a, 0xd4, 0x46, 0x06, 0xd5,
  0x26, 0xd9, 0xb9, 0xa8, 0xc6, 0x8a, 0xc8, 0x3a, 0x93, 0xef,
  0xe8, 0xa0, 0x48, 0xb9, 0x41, 0x26, 0x69, 0x3e, 0x84, 0x36,
  0x20, 0xeb, 0xc7, 0x1d, 0x6e, 0xc7, 0xd6, 0x1e, 0x4d, 0x2d,
  0xe2, 0x56, 0x25, 0xcf, 0xf3, 0x03, 0xb1, 0xc8, 0x47, 0x53,
  0x63, 0xe9, 0x0f, 0x67, 0x34, 0x12, 0xd3, 0x50, 0xea, 0xb3,
  0x0a, 0x43, 0xc4, 0x3f, 0x5a, 0xbe, 0xd2, 0x88, 0x7e, 0xc8,
  0x02, 0xc7, 0xf7, 0x21, 0x31, 0xab, 0x62, 0xbd, 0xb1, 0x17,
  0xa9, 0x77, 0x80, 0x7d, 0x95, 0x6f, 0x66, 0x33, 0x82, 0xc1,
  0x2f, 0xaa, 0x72, 0xb7, 0xbf, 0x23, 0xef, 0x93, 0x04, 0x33,
  0x29, 0xc0, 0x68, 0x3f, 0x39, 0xaa, 0x35, 0x23, 0x8a, 0xbe,
  0x07, 0x2d, 0x96, 0x80, 0x13, 0x81, 0x33, 0x6c, 0xb4, 0xca,
  0x77, 0xa1, 0xca, 0x0d, 0xd4, 0x6a, 0xd6, 0xf3, 0xc7, 0x81,
  0xcd, 0x38, 0xc6, 0xf1, 0xf5, 0xd0, 0x1a, 0x85, 0x02, 0x03,
  0x01, 0x00, 0x01, 0xa3, 0x0d, 0x30, 0x0b, 0x30, 0x09, 0x06,
  0x03, 0x55, 0x1d, 0x13, 0x04, 0x02, 0x30, 0x00, 0x30, 0x0d,
  0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01,
  0x05, 0x05, 0x00, 0x03, 0x82, 0x01, 0x01, 0x00, 0x4a, 0xb0,
  0xb2, 0xb3, 0x3c, 0x3c, 0x64, 0x80, 0x4d, 0xc0, 0x4a, 0x4f,
  0xc2, 0xb7, 0x8c, 0xb2, 0x8b, 0x6a, 0x9a, 0x0a, 0x44, 0xbc,
  0x28, 0x1c, 0x61, 0xb0, 0x4f, 0xd5, 0x85, 0x36, 0x5f, 0xe9,
  0xa1, 0x66, 0x83, 0x6f, 0xd9, 0x88, 0x49, 0x30, 0x7a, 0x64,
  0x7c, 0x20, 0x20, 0xb7, 0x71, 0xa9, 0xcd, 0x13, 0x5a, 0x2e,
  0xfd, 0xdc, 0xa6, 0x04, 0x80, 0xc3, 0xa0, 0xa4, 0x71, 0xfd,
  0xd5, 0x96, 0x80, 0x24, 0x30, 0xea, 0xd6, 0x3a, 0x7a, 0x67,
  0xa9, 0xb2, 0xfc, 0x8d, 0xdf, 0x4e, 0xe1, 0x5d, 0x5d, 0x92,
  0xc6, 0xc1, 0xc9, 0x87, 0x9a, 0xde, 0x92, 0xa1, 0x78, 0x3e,
  0xb9, 0x30, 0xa4, 0x1d, 0xb7, 0x9b, 0xc3, 0x5f, 0x05, 0x48,
  0x37, 0x9c, 0x2a, 0xd0, 0x28, 0x5f, 0xf1, 0x54, 0xf6, 0xa7,
  0x71, 0x54, 0xbe, 0xf3, 0x1a, 0x0b, 0x4b, 0x07, 0x8c, 0xaf,
  0xa0, 0x3d, 0x44, 0x3a, 0xac, 0x7b, 0x69, 0x12, 0x2c, 0xeb,
  0x17, 0x9b, 0x96, 0x23, 0x99, 0xf0, 0xae, 0x07, 0xbc, 0x13,
  0x46, 0x7d, 0xb2, 0x0a, 0xeb, 0x92, 0xa8, 0x0f, 0xa7, 0x31,
  0xe6, 0xce, 0x66, 0xf3, 0xf6, 0xf4, 0xd7, 0xeb, 0xbd, 0x89,
  0xed, 0xa3, 0xcf, 0x08, 0xc7, 0xad, 0xfe, 0x09, 0x89, 0x0a,
  0xf3, 0x27, 0x70, 0x6d, 0x84, 0x34, 0xff, 0x0e, 0xe1, 0xf0,
  0x6f, 0x4a, 0x53, 0x46, 0x1d, 0x24, 0x12, 0x0c, 0x0a, 0x38,
  0xfa, 0xec, 0x73, 0x55, 0xe2, 0x3f, 0xd8, 0xfb, 0x8a, 0xb4,
  0x09, 0xb2, 0xe3, 0xdd, 0x76, 0x82, 0x6e, 0xce, 0x6e, 0xaa,
  0x15, 0xe2, 0x41, 0x6a, 0x67, 0x06, 0x16, 0x59, 0xf6, 0x07,
  0xef, 0xe2, 0x0b, 0x5d, 0x1c, 0x43, 0x0d, 0x65, 0x32, 0x7b,
  0x08, 0x03, 0x67, 0xe8, 0xb6, 0xec, 0xf7, 0xd2, 0x17, 0xef,
  0xe0, 0x79, 0x1c, 0xf0, 0x8b, 0xfd, 0xce, 0x09, 0x3e, 0xaa,
  0x3d, 0x25, 0xd7, 0xd1,
};

const unsigned char kSignature[] = {
  0x76, 0x93, 0x7f, 0x8e, 0x3e, 0x8f, 0x77, 0x64, 0x33, 0xb4,
  0x6d, 0x09, 0xf5, 0x38, 0xa2, 0xde, 0x1b, 0xaa, 0x3e, 0x1f,
  0xfc, 0x5c, 0xd7, 0x17, 0xaa, 0x28, 0x2a, 0xf3, 0x9f, 0x76,
  0xa1, 0x4e, 0x41, 0xab, 0xd2, 0x03, 0xf2, 0x63, 0xf6, 0x24,
  0x61, 0x97, 0xc0, 0x8a, 0xde, 0x11, 0x2e, 0x72, 0xff, 0x98,
  0xeb, 0xd8, 0x18, 0xd5, 0x75, 0x91, 0xcd, 0x08, 0x43, 0x84,
  0xe2, 0xc4, 0x1e, 0x7c, 0xe4, 0x9e, 0xeb, 0xe8, 0x09, 0x45,
  0xe5, 0x6d, 0x62, 0x37, 0x02, 0xeb, 0xfc, 0x0a, 0x12, 0xf5,
  0x91, 0x19, 0x4c, 0x17, 0xfa, 0x5e, 0x2d, 0x4b, 0x80, 0xb9,
  0x1a, 0xd2, 0x47, 0xf7, 0xf9, 0x2f, 0x9f, 0xed, 0xd8, 0x2c,
  0xbf, 0x2e, 0x16, 0x3b, 0x5b, 0x96, 0x5b, 0x9c, 0xe0, 0x73,
  0x4e, 0x15, 0x34, 0x99, 0xde, 0xce, 0x88, 0x5e, 0x6f, 0xcb,
  0x01, 0x36, 0x10, 0x07, 0x71, 0x6a, 0x86, 0xf6, 0x3a, 0x9d,
  0x7f, 0xef, 0xa3, 0x81, 0xd8, 0xff, 0xc0, 0x8c, 0x75, 0x9f,
  0xa0, 0xe4, 0xd0, 0x94, 0x9b, 0x11, 0xe4, 0x68, 0x44, 0x43,
  0xcf, 0x9b, 0x39, 0x3a, 0xb5, 0xa7, 0xa4, 0xd4, 0x74, 0x7f,
  0x86, 0x2a, 0x6a, 0x4b, 0x25, 0x00, 0x15, 0xee, 0x55, 0x30,
  0x87, 0xef, 0x3d, 0xcb, 0x53, 0xfe, 0x88, 0x5e, 0x71, 0x82,
  0x09, 0x0c, 0x5b, 0xef, 0x43, 0xda, 0xc8, 0x98, 0x23, 0x54,
  0x6a, 0xc4, 0x47, 0x8e, 0xbb, 0x4d, 0x5f, 0x2c, 0x1e, 0xaa,
  0x3f, 0xc2, 0xdb, 0xbe, 0x34, 0xf8, 0xac, 0xca, 0x0c, 0x0d,
  0xf8, 0x04, 0xec, 0xba, 0xc4, 0x44, 0xd6, 0x22, 0x9e, 0x12,
  0xc7, 0x14, 0x70, 0x77, 0x56, 0xc6, 0x56, 0xcc, 0x23, 0x12,
  0x7a, 0xef, 0xb0, 0xa0, 0x87, 0x21, 0x05, 0x15, 0xb4, 0xb7,
  0x9e, 0xb0, 0xe1, 0x56, 0xe6, 0x36, 0x73, 0x29, 0xb8, 0x81,
  0x94, 0x26, 0x3d, 0xa4, 0x4c, 0xd6,
};

const unsigned char kPeerCert[] = {
  0x30, 0x82, 0x02, 0xda, 0x30, 0x82, 0x01, 0xc2, 0xa0, 0x03,
  0x02, 0x01, 0x02, 0x02, 0x04, 0x01, 0xc1, 0x6f, 0x87, 0x30,
  0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01,
  0x01, 0x0b, 0x05, 0x00, 0x30, 0x2f, 0x31, 0x2d, 0x30, 0x2b,
  0x06, 0x03, 0x55, 0x04, 0x03, 0x13, 0x24, 0x31, 0x37, 0x63,
  0x63, 0x36, 0x62, 0x35, 0x35, 0x2d, 0x33, 0x38, 0x33, 0x34,
  0x2d, 0x31, 0x31, 0x30, 0x63, 0x2d, 0x64, 0x62, 0x64, 0x64,
  0x2d, 0x65, 0x32, 0x32, 0x61, 0x61, 0x37, 0x32, 0x37, 0x62,
  0x31, 0x39, 0x62, 0x30, 0x1e, 0x17, 0x0d, 0x31, 0x34, 0x31,
  0x30, 0x33, 0x30, 0x30, 0x38, 0x35, 0x30, 0x30, 0x37, 0x5a,
  0x17, 0x0d, 0x31, 0x34, 0x31, 0x31, 0x30, 0x31, 0x30, 0x38,
  0x35, 0x30, 0x30, 0x37, 0x5a, 0x30, 0x2f, 0x31, 0x2d, 0x30,
  0x2b, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13, 0x24, 0x31, 0x37,
  0x63, 0x63, 0x36, 0x62, 0x35, 0x35, 0x2d, 0x33, 0x38, 0x33,
  0x34, 0x2d, 0x31, 0x31, 0x30, 0x63, 0x2d, 0x64, 0x62, 0x64,
  0x64, 0x2d, 0x65, 0x32, 0x32, 0x61, 0x61, 0x37, 0x32, 0x37,
  0x62, 0x31, 0x39, 0x62, 0x30, 0x82, 0x01, 0x22, 0x30, 0x0d,
  0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01,
  0x01, 0x05, 0x00, 0x03, 0x82, 0x01, 0x0f, 0x00, 0x30, 0x82,
  0x01, 0x0a, 0x02, 0x82, 0x01, 0x01, 0x00, 0xb9, 0x3b, 0x11,
  0x16, 0xe0, 0x76, 0xc3, 0x19, 0xe9, 0x25, 0xb2, 0x59, 0x46,
  0x7d, 0xcf, 0x4f, 0xf5, 0x86, 0x3d, 0x73, 0xd7, 0x6e, 0x78,
  0x20, 0x95, 0x38, 0x80, 0x58, 0x02, 0xe7, 0x05, 0x80, 0x8b,
  0x06, 0x54, 0x75, 0xa0, 0x7e, 0x2a, 0x43, 0x5e, 0x72, 0xa4,
  0x6e, 0x07, 0xf2, 0x9a, 0x98, 0x39, 0x86, 0x81, 0x1e, 0x62,
  0xd7, 0x16, 0xa7, 0x18, 0x14, 0xa7, 0x59, 0xa5, 0xc6, 0x49,
  0x56, 0xd1, 0x0d, 0x45, 0xea, 0x0d, 0xa6, 0x6a, 0x7c, 0xae,
  0xac, 0xc8, 0xdc, 0x39, 0xb6, 0xa6, 0xb4, 0xa1, 0x71, 0x71,
  0xff, 0xfb, 0xa0, 0xb3, 0x02, 0xb3, 0xaa, 0x20, 0xfe, 0xfa,
  0xcd, 0x18, 0xff, 0x8a, 0xa2, 0x19, 0x82, 0x4f, 0x44, 0x9c,
  0x97, 0xa4, 0x63, 0xce, 0xe5, 0x76, 0xe5, 0x96, 0x92, 0xc1,
  0x92, 0x88, 0x2a, 0x5a, 0xb9, 0xde, 0xf3, 0x24, 0xf5, 0xc5,
  0xfe, 0xfd, 0x42, 0x81, 0x07, 0x51, 0x47, 0x23, 0x15, 0x15,
  0x5e, 0x5e, 0xf0, 0xf9, 0xac, 0xbe, 0xdb, 0xb6, 0x9d, 0xee,
  0x92, 0xab, 0xf8, 0xcf, 0x08, 0x1a, 0xf6, 0x14, 0xf2, 0x9c,
  0x5a, 0xbd, 0x73, 0xa4, 0xd1, 0x02, 0x0b, 0xff, 0xbc, 0x31,
  0x91, 0xa5, 0xf3, 0x07, 0xef, 0x9f, 0xc4, 0xad, 0x56, 0xf9,
  0x86, 0xf9, 0xc6, 0xee, 0x27, 0xeb, 0xd7, 0xe9, 0xd8, 0x57,
  0xb3, 0x0f, 0x96, 0x8e, 0x57, 0x27, 0x5e, 0x5e, 0x75, 0x4e,
  0x00, 0xd5, 0x4c, 0xbb, 0x2f, 0xb3, 0xcc, 0xd5, 0x91, 0xa5,
  0x89, 0x62, 0x47, 0x7d, 0x4c, 0x43, 0x77, 0xd9, 0x3d, 0x75,
  0xcb, 0x6c, 0x85, 0xaa, 0x9c, 0x4f, 0xec, 0x80, 0x30, 0xf2,
  0xeb, 0x6f, 0xec, 0xe7, 0xd1, 0xd8, 0xbe, 0xc8, 0x9d, 0x14,
  0x6d, 0x09, 0xc8, 0xc4, 0x95, 0xcf, 0x21, 0xe6, 0xf8, 0xf0,
  0x25, 0xa3, 0xa2, 0xf6, 0x4f, 0x44, 0xbf, 0x00, 0xca, 0xe7,
  0x68, 0x7b, 0x33, 0x02, 0x03, 0x01, 0x00, 0x01, 0x30, 0x0d,
  0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01,
  0x0b, 0x05, 0x00, 0x03, 0x82, 0x01, 0x01, 0x00, 0x02, 0xd1,
  0x74, 0x56, 0x74, 0x82, 0x03, 0x2b, 0x53, 0x34, 0x82, 0x9e,
  0x16, 0xa2, 0x43, 0x24, 0x6c, 0xf1, 0x2c, 0x5f, 0xc2, 0xa5,
  0x09, 0xc4, 0x72, 0x62, 0x1d, 0xc7, 0x10, 0x57, 0x9a, 0x40,
  0x55, 0x83, 0xa7, 0x5f, 0x46, 0x44, 0x96, 0xeb, 0xcd, 0x09,
  0x40, 0xdd, 0xa8, 0xdb, 0x6a, 0xaa, 0xb5, 0xaf, 0x8d, 0x7c,
  0xb1, 0xa2, 0xc4, 0xff, 0xcc, 0xa5, 0x70, 0x85, 0xa6, 0x30,
  0x95, 0x69, 0x8b, 0x61, 0x7f, 0x4c, 0x34, 0x95, 0xeb, 0xb1,
  0x7f, 0x07, 0x4e, 0x60, 0x90, 0x34, 0xc5, 0x3b, 0x37, 0x66,
  0x94, 0xd7, 0x18, 0x8d, 0x39, 0x64, 0x3b, 0x6e, 0xdc, 0x52,
  0xf6, 0x46, 0x64, 0x33, 0x50, 0x97, 0xc4, 0xce, 0xba, 0x66,
  0xc2, 0x91, 0x2a, 0x1b, 0xde, 0xdc, 0x37, 0x26, 0x31, 0x12,
  0xa8, 0x70, 0x77, 0x7f, 0x4d, 0xc0, 0x65, 0x8e, 0x25, 0x09,
  0xc5, 0x90, 0x77, 0xf3, 0x60, 0x4d, 0x20, 0xf3, 0x27, 0x7a,
  0x34, 0x02, 0xea, 0xfe, 0xa1, 0x48, 0x8b, 0x63, 0xc2, 0xf7,
  0x70, 0xb2, 0x23, 0x65, 0x55, 0xc8, 0x1b, 0xd1, 0x21, 0xe3,
  0x1b, 0xf0, 0x50, 0xbb, 0x63, 0xe2, 0x7f, 0x02, 0xe0, 0xa5,
  0xb7, 0x33, 0x67, 0x16, 0x0b, 0xba, 0xfe, 0x5f, 0x9d, 0xa2,
  0x6f, 0x38, 0xb7, 0x3e, 0x3f, 0x17, 0x6a, 0x6d, 0xe1, 0xa2,
  0xc4, 0xee, 0x77, 0xe6, 0x80, 0xe1, 0x2c, 0x3d, 0x93, 0x3d,
  0x17, 0xc3, 0x58, 0x6f, 0x87, 0x0a, 0xe5, 0xa9, 0x7e, 0xef,
  0xd4, 0x33, 0x94, 0xc3, 0xeb, 0xd5, 0xb4, 0x1a, 0xef, 0x38,
  0xfb, 0xb4, 0xa9, 0x40, 0xa9, 0x56, 0x41, 0x32, 0xd8, 0xd5,
  0x16, 0x42, 0xcd, 0xb5, 0xdc, 0x49, 0x64, 0x01, 0x68, 0x2b,
  0x42, 0xa9, 0xcf, 0x64, 0x12, 0x6a, 0x7c, 0xc8, 0xd8, 0x3b,
  0xfc, 0x14, 0x67, 0xb4, 0x4e, 0x3a, 0x75, 0x32, 0xda, 0xfe,
  0x43, 0xb2, 0x35, 0xa1,
};

class CastAuthUtilTest : public testing::Test {
 public:
  CastAuthUtilTest () {}
  ~CastAuthUtilTest () override {}

  void SetUp() override {
  }

 protected:
  static AuthResponse CreateAuthResponse() {
    std::string keys =
        "CrMCCiBSnZzWf+XraY5w3SbX2PEmWfHm5SNIv2pc9xbhP0EOcxKOAjCCAQoCggEBALwigL"
        "2A9johADuudl41fz3DZFxVlIY0LwWHKM33aYwXs1CnuIL638dDLdZ+q6BvtxNygKRHFcEg"
        "mVDN7BRiCVukmM3SQbY2Tv/oLjIwSoGoQqNsmzNuyrL1U2bgJ1OGGoUepzk/SneO+1RmZv"
        "tYVMBeOcf1UAYL4IrUzuFqVR+LFwDmaaMn5gglaTwSnY0FLNYuojHetFJQ1iBJ3nGg+a0g"
        "QBLx3SXr1ea4NvTWj3/KQ9zXEFvmP1GKhbPz//YDLcsjT5ytGOeTBYysUpr3TOmZer5ufk"
        "0K48YcqZP6OqWRXRy9ZuvMYNyGdMrP+JIcmH1X+mFHnquAt+RIgCqSxRsCAwEAAQqzAgog"
        "mNZt6BxWR4RNlkNNN8SNws5/CHJQGee26JJ/VtaBqhgSjgIwggEKAoIBAQC8IoC9gPY6IQ"
        "A7rnZeNX89w2RcVZSGNC8FhyjN92mMF7NQp7iC+t/HQy3Wfqugb7cTcoCkRxXBIJlQzewU"
        "YglbpJjN0kG2Nk7/6C4yMEqBqEKjbJszbsqy9VNm4CdThhqFHqc5P0p3jvtUZmb7WFTAXj"
        "nH9VAGC+CK1M7halUfixcA5mmjJ+YIJWk8Ep2NBSzWLqIx3rRSUNYgSd5xoPmtIEAS8d0l"
        "69XmuDb01o9/ykPc1xBb5j9RioWz8//2Ay3LI0+crRjnkwWMrFKa90zpmXq+bn5NCuPGHK"
        "mT+jqlkV0cvWbrzGDchnTKz/iSHJh9V/phR56rgLfkSIAqksUbAgMBAAE=";
    std::string signature =
        "eHMoa7dP2ByNtDnxM/Q6yV3ZyUyihBFgOthq937yuiu2uwW2X/i8h1YrJFaWrA0iTTfSLA"
        "a6PBAN1hhnwXlWYy8MvViJ9eJqf5FfCCkOjdRN0QIFPpmIJm/EcIv91bNMWnOGANgSW1Ho"
        "ns+sC0/kROPbPABPLLwfgGizBDSZNapxgj8G+iDvi1JRRvvNdmjUs2AUIPNrSp3Knt3FyZ"
        "5F2SmkKhpo7XVTWgSuWOzUJu6zNHn2krm64Ymd2HxRDyKTm1DBzy1MoXv4/8mbLYdj+KAv"
        "hqKJfRcrGkUXVK++wCHERwxcvfk7e6lN6adcCVYP9pZPMhE/UyAJY6/uE1X0cw==";
    EXPECT_TRUE(SetTrustedCertificateAuthorities(keys, signature));

    AuthResponse response;
    response.add_intermediate_certificate(
        std::string(reinterpret_cast<const char*>(kIntermediateCertificate),
                    arraysize(kIntermediateCertificate)));
    response.set_client_auth_certificate(
        std::string(reinterpret_cast<const char*>(kClientAuthCertificate),
                    arraysize(kClientAuthCertificate)));
    response.set_signature(std::string(
        reinterpret_cast<const char*>(kSignature), arraysize(kSignature)));
    return response;
  }

  static std::string CreatePeerCert() {
    return std::string(reinterpret_cast<const char*>(kPeerCert),
                       arraysize(kPeerCert));
  }

  // Mangles a string by inverting the first byte.
  static void MangleString(std::string* str) {
    (*str)[0] = ~(*str)[0];
  }
};

TEST_F(CastAuthUtilTest, VerifySuccess) {
  AuthResponse auth_response = CreateAuthResponse();
  AuthResult result = VerifyCredentials(
      auth_response, CreatePeerCert());
  EXPECT_TRUE(result.success());
}

TEST_F(CastAuthUtilTest, VerifyBadCA) {
  AuthResponse auth_response = CreateAuthResponse();
  MangleString(auth_response.mutable_intermediate_certificate(0));
  AuthResult result = VerifyCredentials(
      auth_response, CreatePeerCert());
  EXPECT_FALSE(result.success());
  EXPECT_EQ(AuthResult::ERROR_CERT_NOT_SIGNED_BY_TRUSTED_CA, result.error_type);
}

TEST_F(CastAuthUtilTest, VerifyBadClientAuthCert) {
  AuthResponse auth_response = CreateAuthResponse();
  MangleString(auth_response.mutable_client_auth_certificate());
  AuthResult result = VerifyCredentials(
      auth_response, CreatePeerCert());
  EXPECT_FALSE(result.success());
  EXPECT_EQ(AuthResult::ERROR_CERT_PARSING_FAILED,
            result.error_type);
}

TEST_F(CastAuthUtilTest, VerifyBadSignature) {
  AuthResponse auth_response = CreateAuthResponse();
  MangleString(auth_response.mutable_signature());
  AuthResult result = VerifyCredentials(
      auth_response, CreatePeerCert());
  EXPECT_FALSE(result.success());
  EXPECT_EQ(AuthResult::ERROR_SIGNED_BLOBS_MISMATCH,
            result.error_type);
}

TEST_F(CastAuthUtilTest, VerifyBadPeerCert) {
  AuthResponse auth_response = CreateAuthResponse();
  std::string peer_cert = CreatePeerCert();
  MangleString(&peer_cert);
  AuthResult result = VerifyCredentials(
      auth_response, peer_cert);
  EXPECT_FALSE(result.success());
  EXPECT_EQ(AuthResult::ERROR_SIGNED_BLOBS_MISMATCH,
            result.error_type);
}

}  // namespace
}  // namespace cast_channel
}  // namespace api
}  // namespace extensions
