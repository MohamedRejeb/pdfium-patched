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

// Opaque handle for an Optional Content (layer) visibility context bound
// to a document. Created by FPDFRejeb_OCContextCreate, destroyed by
// FPDFRejeb_OCContextDestroy. Must NOT outlive the underlying FPDF_DOCUMENT.
//
// Modeled on upstream FPDF_* opaque-pointer typedefs (e.g. FPDF_DOCUMENT)
// to keep the type system honest: a caller cannot pass an FPDF_DOCUMENT
// where an FPDF_OCCONTEXT is expected.
typedef struct fpdf_occontext_t__* FPDF_OCCONTEXT;

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

// Creates an Optional Content (layer) visibility context for |document|,
// configured for kView usage. Returns an opaque handle owned by the caller —
// must be released via FPDFRejeb_OCContextDestroy. Returns NULL on error
// (e.g. null document).
//
// Backed by patches/0002-export-ocg-visibility.patch wrapping
// CPDF_OCContext in core/fpdfapi/page/cpdf_occontext.h.
FPDF_EXPORT FPDF_OCCONTEXT FPDF_CALLCONV
FPDFRejeb_OCContextCreate(FPDF_DOCUMENT document);

// Releases the OC context returned by FPDFRejeb_OCContextCreate. Safe to
// call with NULL. After this returns the handle is invalid.
FPDF_EXPORT void FPDF_CALLCONV
FPDFRejeb_OCContextDestroy(FPDF_OCCONTEXT context);

// Returns true if |page_object| is currently visible under the OC layer
// state captured by |context|. Returns false on null args or when the
// object is hidden by the active layer state.
FPDF_EXPORT FPDF_BOOL FPDF_CALLCONV
FPDFRejeb_OCContextCheckObjectVisible(FPDF_OCCONTEXT context,
                                      FPDF_PAGEOBJECT page_object);

// Returns the number of Optional Content Groups (layers) declared in
// |document|'s OCProperties dictionary. Returns -1 on null document, 0
// if the document declares no OCGs.
FPDF_EXPORT int FPDF_CALLCONV
FPDFRejeb_GetOCGCount(FPDF_DOCUMENT document);

// Writes the UTF-16LE name of OCG |index| into |buffer| using the standard
// PDFium two-pass query pattern: pass NULL buffer + 0 buflen first to learn
// the byte length, then allocate and call again. Returns the number of
// bytes written (or required) including the NUL terminator. Returns 0 on
// null/invalid arguments or out-of-range |index|.
FPDF_EXPORT unsigned long FPDF_CALLCONV
FPDFRejeb_GetOCGName(FPDF_DOCUMENT document,
                     int index,
                     void* buffer,
                     unsigned long buflen);

// Returns the source-stream charcode for the char at |char_index| of
// |text_page|. Indexed accessor that handles RTL/visual reordering and
// ligature decomposition the same way PDFium does internally — each
// CPDF_TextPage::CharInfo already carries the resolved source charcode,
// so callers don't need to walk the underlying CPDF_TextObject manually.
//
// Returns the same kind of value FPDFRejeb_TextObjGetCharCodes does
// (CIDs for CID fonts, 1-byte codes for simple fonts).
//
// Returns 0 on null |text_page| or out-of-range |char_index|. Note that
// 0 is also a valid charcode for some fonts (typically .notdef), so
// callers that need to disambiguate should validate |char_index| against
// FPDFText_CountChars first.
//
// Backed by patches/0003-export-textpage-source-charcode.patch wrapping
// CPDF_TextPage::GetCharInfo(index).char_code() in
// core/fpdftext/cpdf_textpage.h.
FPDF_EXPORT uint32_t FPDF_CALLCONV
FPDFRejeb_TextGetSourceCharCode(FPDF_TEXTPAGE text_page, int char_index);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // PUBLIC_FPDF_REJEB_H_
