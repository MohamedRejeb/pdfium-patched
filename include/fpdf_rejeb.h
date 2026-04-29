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

#include "fpdf_edit.h"  // for FPDF_GLYPHPATH (used by FPDFRejeb_TextGetGlyphPath)
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

// Returns the glyph outline (FPDF_GLYPHPATH) for the char at |char_index|
// of |text_page| at |font_size|, using the source-stream charcode directly
// — sidesteps upstream FPDFFont_GetGlyphPath's CharCodeFromUnicode round
// trip, which collapses to .notdef for many CID fonts (notably Arabic
// embedded subsets).
//
// The returned handle is consumed by upstream FPDFGlyphPath_CountGlyphSegments
// / FPDFGlyphPath_GetGlyphPathSegment exactly the same way as a path from
// FPDFFont_GetGlyphPath. The handle's lifetime is tied to the underlying
// font; do NOT use it after the document is closed.
//
// Returns NULL for null/out-of-range inputs, generated chars (kGenerated
// — inserted spaces, hyphens, line-break artifacts), Type 3 fonts (whose
// glyphs are content streams, not outlines), and char-pos lookup miss.
//
// Backed by patches/0004-export-text-glyph-path.patch wrapping
// CPDF_Font::GetCharPosList + CFX_Font::LoadGlyphPath.
FPDF_EXPORT FPDF_GLYPHPATH FPDF_CALLCONV
FPDFRejeb_TextGetGlyphPath(FPDF_TEXTPAGE text_page,
                           int char_index,
                           float font_size);

// Writes |page_object|'s graphics-state fill alpha (ExtGState /ca) into
// |fill_alpha| and stroke alpha (ExtGState /CA) into |stroke_alpha|. Both
// values are in [0.0, 1.0]; the PDF default is 1.0 (fully opaque).
//
// When the object has no ExtGState attached (the common case — most PDF
// objects don't set one explicitly), both out-params are filled with 1.0
// and the function returns true. This lets callers `multiply *= alpha`
// blindly without branching on the empty-state case.
//
// Returns false only on null arguments (page_object / either out-param).
//
// Backed by patches/0005-export-pageobj-alpha.patch wrapping
// CPDF_GeneralState::GetFillAlpha / GetStrokeAlpha via
// CPDF_PageObject::general_state() in core/fpdfapi/page/cpdf_generalstate.h.
FPDF_EXPORT FPDF_BOOL FPDF_CALLCONV
FPDFRejeb_PageObjGetAlpha(FPDF_PAGEOBJECT page_object,
                          float* fill_alpha,
                          float* stroke_alpha);

// Returns the CPDF_TextPage::CharType enum value for the char at
// |char_index| of |text_page|. Lets callers tell |FPDFRejeb_TextGetGlyphPath|
// nulls that are *expected* (kGenerated) apart from those that indicate
// a real extraction failure (Type 3 font, empty char-pos lookup, etc.).
//
// Mapping (mirrors PDFium's CPDF_TextPage::CharType enum):
//   0 = kNormal       — real char, has source charcode
//   1 = kGenerated    — synthesized (decomposed ligature, line-break artifact)
//   2 = kNotUnicode   — char without unicode mapping
//   3 = kHyphen       — auto-inserted at line break
//   4 = kPiece        — sub-piece of a multi-glyph char (non-rendered)
//
// Returns -1 on null |text_page| or out-of-range |char_index|.
//
// Backed by patches/0006-export-textpage-charinfo-type.patch wrapping
// CPDF_TextPage::GetCharInfo(index).char_type() in
// core/fpdftext/cpdf_textpage.h.
FPDF_EXPORT int FPDF_CALLCONV
FPDFRejeb_TextGetCharType(FPDF_TEXTPAGE text_page, int char_index);

// Returns |page_object|'s graphics-state blend mode (ExtGState /BM) as an
// int matching PDFium's BlendMode enum (declared in core/fxge/dib/fx_dib.h):
//
//   0  = kNormal       1  = kMultiply     2  = kScreen       3  = kOverlay
//   4  = kDarken       5  = kLighten      6  = kColorDodge   7  = kColorBurn
//   8  = kHardLight    9  = kSoftLight    10 = kDifference   11 = kExclusion
//   12 = kHue          13 = kSaturation   14 = kColor        15 = kLuminosity
//
// Note: PDFium 7811's enum is dense; older revisions had a value gap
// between kExclusion and kHue. Map these by name on the consumer side
// to stay robust across PDFium bumps.
//
// When the object has no ExtGState attached (the common case for
// ordinary page content), returns kNormal (0) — the PDF default — so
// callers can treat blend mode as always-present.
//
// Returns -1 only on null |page_object|.
//
// Backed by patches/0007-export-pageobj-blendmode.patch wrapping
// CPDF_GeneralState::GetBlendType() via CPDF_PageObject::general_state().
FPDF_EXPORT int FPDF_CALLCONV
FPDFRejeb_PageObjGetBlendMode(FPDF_PAGEOBJECT page_object);

// Per-text-object source-CID walk. The four FPDFRejeb_TextObj*At
// accessors below let callers iterate the raw content-stream CID array
// of a text page object — sidestepping the text-page layer entirely.
//
// Why bypass FPDFRejeb_TextGetGlyphPath (text-page layer): PDFium's
// text page decomposes ligatures into PIECE chars and, for fonts with
// a non-invertible ToUnicode CMap, synthesises PIECE entries with
// char_code == 0xFFFFFFFF ("no source CID") — every such char then
// hits .notdef at glyph lookup. Native PDFium renders correctly because
// its text renderer walks the text object's own char_codes_ vector
// directly, never going through the text page. These exports give
// callers the same path.
//
// All four take an FPDF_PAGEOBJECT that the caller has already verified
// is a text object via FPDFPageObj_GetType() == FPDF_PAGEOBJ_TEXT.
// They internally re-validate (returning a sentinel on type mismatch)
// so passing a non-text object is safe but useless.

// Returns the number of CIDs in |text_obj|'s content-stream CID array
// (== CPDF_TextObject::CharCount()). Returns -1 if |text_obj| is null
// or not a text page object.
//
// Backed by patches/0008-export-textobj-cid-walk.patch.
FPDF_EXPORT int FPDF_CALLCONV
FPDFRejeb_TextObjCountCharCodes(FPDF_PAGEOBJECT text_obj);

// Returns the raw stream CID at |index| in |text_obj|. For CIDFonts this
// is the multi-byte CID; for simple fonts it's the 1-byte code (same
// kind of value FPDFRejeb_TextObjGetCharCodes returns, just per-index).
//
// Returns 0xFFFFFFFF on null/non-text |text_obj|, on negative |index|,
// on out-of-range |index|, AND for legitimate TJ-array number-adjustment
// slots (PDFium stores them inline in char_codes_ as 0xFFFFFFFF — they
// represent in-line position tweaks, not real glyphs). Callers should
// filter on this single sentinel value either way.
//
// Backed by patches/0008-export-textobj-cid-walk.patch.
FPDF_EXPORT uint32_t FPDF_CALLCONV
FPDFRejeb_TextObjGetCharCodeAt(FPDF_PAGEOBJECT text_obj, int index);

// Writes the page-space (post text-matrix) glyph origin for the CID at
// |index| of |text_obj| into |out_x| and |out_y|. The coordinate system
// matches FPDFTextObj_GetMatrix's output — same point any caller already
// using upstream's text-object APIs is operating in.
//
// Returns FPDF_TRUE on success. Returns FPDF_FALSE (with |out_x|/|out_y|
// unchanged) on null |text_obj|/out-params, non-text object, negative
// or out-of-range |index|, OR when the CID at |index| is the 0xFFFFFFFF
// TJ-array sentinel (no rendered position to report).
//
// Backed by patches/0008-export-textobj-cid-walk.patch.
FPDF_EXPORT FPDF_BOOL FPDF_CALLCONV
FPDFRejeb_TextObjGetCharPosAt(FPDF_PAGEOBJECT text_obj,
                              int index,
                              float* out_x,
                              float* out_y);

// Loads the glyph outline for the CID at |index| of |text_obj|, sized
// to |font_size|. Mirrors FPDFRejeb_TextGetGlyphPath (patch 0004) —
// same GetCharPosList → LoadGlyphPath chain so fallback-font glyphs
// resolve correctly — but skips the lossy text-page round-trip, so it
// works for CIDs that the text-page layer would expose as 0xFFFFFFFF.
//
// Returns NULL for: null/non-text |text_obj|, negative or out-of-range
// |index|, TJ-array sentinel CID at |index|, Type 3 fonts (whose glyphs
// are content streams not outlines), and char-pos lookup miss.
//
// The returned FPDF_GLYPHPATH is consumed by upstream
// FPDFGlyphPath_CountGlyphSegments / FPDFGlyphPath_GetGlyphPathSegment
// the same way as a path from FPDFFont_GetGlyphPath. Lifetime is tied
// to the underlying font; do NOT use it after the document is closed.
//
// Backed by patches/0008-export-textobj-cid-walk.patch.
FPDF_EXPORT FPDF_GLYPHPATH FPDF_CALLCONV
FPDFRejeb_TextObjGetGlyphPathAt(FPDF_PAGEOBJECT text_obj,
                                int index,
                                float font_size);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // PUBLIC_FPDF_REJEB_H_
