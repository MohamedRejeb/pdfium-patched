// Copyright (c) 2026 Mohamed Rejeb. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Additional FPDF_EXPORT declarations layered on top of upstream PDFium.
// All symbols here are prefixed FPDFRejeb_ to stay out of upstream's
// namespace. Patches in this repo provide the matching definitions in
// fpdfsdk/fpdf_rejeb.cpp inside the patched PDFium tree.
//
// At PATCHES_VERSION=0 (M0) this header has no declarations — it ships
// as a placeholder so the release artifact's include/ layout is stable
// for downstream consumers from day one.

#ifndef PUBLIC_FPDF_REJEB_H_
#define PUBLIC_FPDF_REJEB_H_

#include "fpdfview.h"

#ifdef __cplusplus
extern "C" {
#endif

// Declarations are added here by future patches (0001, 0002, 0003, ...).

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // PUBLIC_FPDF_REJEB_H_
