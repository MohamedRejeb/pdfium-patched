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

#include <stdint.h>

#include "fpdfview.h"

#ifdef __cplusplus
extern "C" {
#endif

// Returns the character codes of |text_object| via the two-pass query
// pattern: pass NULL buffer + 0 size first to learn |out_size|, then
// allocate and call again. For CID fonts these are CIDs; for simple
// fonts they are 1-byte codes. They are NOT unicode and NOT the same
// as glyph IDs.
//
// Returns false on null args or non-text page object; true on success
// even if the buffer was too small (|out_size| is still set).
//
// Backed by patches/0001-export-textobj-charcodes.patch wrapping
// CPDF_TextObject::GetCharCodes() in core/fpdfapi/page/cpdf_textobject.h.
FPDF_EXPORT FPDF_BOOL FPDF_CALLCONV
FPDFRejeb_TextObjGetCharCodes(FPDF_PAGEOBJECT text_object,
                              uint32_t* buffer,
                              unsigned long buffer_size,
                              unsigned long* out_size);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // PUBLIC_FPDF_REJEB_H_
