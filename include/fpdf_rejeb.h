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
// object is hidden by the active layer state — through the marked content
// around it or, for a Form or Image XObject, through /OC in the XObject's
// own dictionary (patches/0021-ocg-visibility-xobject-oc.patch).
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

// Returns which kind of pattern (if any) is used for |path_obj|'s fill:
//   0 = no pattern (solid colour, no fill, or default state)
//   1 = tiling pattern   (CPDF_TilingPattern — repeating tile)
//   2 = shading pattern  (CPDF_ShadingPattern — gradient/function fill)
//
// Returns -1 on null |path_obj| or non-path page object.
//
// Lets a downstream renderer decide whether the path can be drawn via the
// vector chain (kind 0) or needs to fall back to FPDFRejeb_PathObjGetRenderedBitmap
// (kind 1 or 2). PDFium's CPDF_Pattern doesn't expose a public type
// discriminator at 7811, so this export wraps the AsTilingPattern() /
// AsShadingPattern() virtual-cast pair.
//
// Backed by patches/0009-export-pathobj-pattern-and-type3.patch.
FPDF_EXPORT int FPDF_CALLCONV
FPDFRejeb_PathObjGetFillPatternKind(FPDF_PAGEOBJECT path_obj);

// Renders only |path_obj| into a fresh FPDF_BITMAP, scaled by |scale|. The
// returned bitmap covers the path's page-space bounding box rounded outward
// to whole pixels. Caller takes ownership and MUST release with
// FPDFBitmap_Destroy.
//
// Mirrors upstream FPDFTextObj_GetRenderedBitmap step-for-step but for a
// path object — uses the same CPDF_RenderStatus::RenderSingleObject path.
// Routes pattern / shading / image-resource lookups through the page's
// resource dictionary, so pattern fills resolve correctly. Pass NULL for
// |page| only if the path object is detached from any page (rare).
//
// Returns NULL on: null |document|, null |path_obj|, page that doesn't
// belong to |document|, scale <= 0, empty bbox, or bitmap allocation
// failure.
//
// Use case: downstream callers fall back to this rasteriser when
// FPDFRejeb_PathObjGetFillPatternKind reports a pattern (kind 1 or 2)
// that the vector chain can't reproduce — single-object rasterisation
// avoids re-rendering the whole page just to grab one pattern-filled path.
//
// Backed by patches/0009-export-pathobj-pattern-and-type3.patch.
FPDF_EXPORT FPDF_BITMAP FPDF_CALLCONV
FPDFRejeb_PathObjGetRenderedBitmap(FPDF_DOCUMENT document,
                                   FPDF_PAGE page,
                                   FPDF_PAGEOBJECT path_obj,
                                   float scale);

// Type 3 procedural-glyph vector enumeration. The three exports below let
// callers walk the page-object content of a Type 3 font's per-char glyph
// procedure — recovering the vector geometry that FPDFRejeb_TextGetGlyphPath
// (patch 0004) and FPDFRejeb_TextObjGetGlyphPathAt (patch 0008) bail on
// for Type 3 fonts (their glyphs are content streams, not outlines).
//
// Recovery flow for a single Type 3 char at index i of |text_obj|:
//   1. count = FPDFRejeb_TextObjGetType3CharObjectCount(text_obj, i)
//      Bail if -1 (not Type 3, or no glyph for the char).
//   2. for j in 0..count: obj = FPDFRejeb_TextObjGetType3CharObjectAt(text_obj, i, j)
//      Use upstream FPDFPageObj_GetType / FPDFPath_* / etc. on each |obj|.
//   3. font_matrix = FPDFRejeb_TextObjGetType3FontMatrix(text_obj, ...)
//      Compose with the text-object matrix (from FPDFTextObj_GetMatrix)
//      to map per-glyph form-space geometry into page space.
//
// The returned page objects are owned by the font (which is owned by the
// document) — do NOT call FPDFPageObj_Destroy on them. Lifetime is tied
// to the document.

// Returns the number of page objects in the Type 3 char's glyph procedure
// for char |char_index| of |text_obj|. Returns -1 on null / non-text /
// non-Type-3 / out-of-range / sentinel charcode / no-glyph-loaded. Returns
// 0 only if the glyph procedure exists but is empty (rare — usually the
// font would not declare the glyph).
//
// Backed by patches/0009-export-pathobj-pattern-and-type3.patch.
FPDF_EXPORT int FPDF_CALLCONV
FPDFRejeb_TextObjGetType3CharObjectCount(FPDF_PAGEOBJECT text_obj,
                                         int char_index);

// Returns the |obj_index|-th page object in the Type 3 char's glyph procedure
// (use FPDFRejeb_TextObjGetType3CharObjectCount to learn the upper bound).
// Returns NULL on out-of-range, non-Type-3, etc. The returned page object
// is owned by the font; do NOT FPDFPageObj_Destroy it.
//
// Backed by patches/0009-export-pathobj-pattern-and-type3.patch.
FPDF_EXPORT FPDF_PAGEOBJECT FPDF_CALLCONV
FPDFRejeb_TextObjGetType3CharObjectAt(FPDF_PAGEOBJECT text_obj,
                                      int char_index,
                                      int obj_index);

// Writes the 6 components of the Type 3 font's per-glyph matrix
// (CPDF_Type3Font::GetFontMatrix) into the out-params. The matrix maps the
// font's per-glyph unit space (typically /FontBBox normalised to 1/1000
// units) into the font's text space — callers walking the per-char form
// objects need to compose this with the text-object matrix to land their
// extracted geometry in page space.
//
// Returns FPDF_FALSE on any null arg or non-text / non-Type-3 |text_obj|.
// On FPDF_FALSE the out-params are unchanged.
//
// Backed by patches/0009-export-pathobj-pattern-and-type3.patch.
FPDF_EXPORT FPDF_BOOL FPDF_CALLCONV
FPDFRejeb_TextObjGetType3FontMatrix(FPDF_PAGEOBJECT text_obj,
                                    float* out_a,
                                    float* out_b,
                                    float* out_c,
                                    float* out_d,
                                    float* out_e,
                                    float* out_f);

// Detaches |page_object| from the content stream it was parsed out of and
// marks it dirty, as if it were a freshly created object. Serialized paint
// order is stream order first and page-object-list order only within a
// stream, and FPDFPage_GenerateContent() writes stream-less objects into a
// NEW stream appended after all existing ones — so without this call a
// freshly inserted object can never paint BELOW loaded content: loaded
// objects rejoin their original (earlier) stream even across
// FPDFPage_RemoveObject / FPDFPage_InsertObject.
//
// Intended use: detach a loaded object with FPDFPage_RemoveObject (which
// records its old stream as dirty so its bytes are rewritten without it),
// call this, then FPDFPage_InsertObject it back at the desired list
// position. Once every object of the page is stream-less, GenerateContent
// serializes the whole page into one stream in exact list order.
//
// Returns FPDF_FALSE on a null |page_object|.
//
// Backed by patches/0014-export-pageobj-reset-content-stream.patch.
FPDF_EXPORT FPDF_BOOL FPDF_CALLCONV
FPDFRejeb_PageObjResetContentStream(FPDF_PAGEOBJECT page_object);

// What FPDFPage_GenerateContent() would lose if it re-serialized an object.
//
// PDFium rewrites a content stream from its in-memory model, and that
// model writes back less than it parsed. Every flag below names content
// the generator drops or replaces, so a rewrite of the object's stream
// changes how the page looks while text extraction stays identical.
#define FPDFREJEB_REGENLOSS_FILL_COLORSPACE 0x0001
#define FPDFREJEB_REGENLOSS_STROKE_COLORSPACE 0x0002
#define FPDFREJEB_REGENLOSS_TEXT_SPACING 0x0004
#define FPDFREJEB_REGENLOSS_INLINE_IMAGE 0x0008
#define FPDFREJEB_REGENLOSS_SHADING 0x0010
#define FPDFREJEB_REGENLOSS_TYPE3_TEXT 0x0020
#define FPDFREJEB_REGENLOSS_TEXT_CLIP 0x0040
#define FPDFREJEB_REGENLOSS_GRAPHICS_STATE 0x0080
#define FPDFREJEB_REGENLOSS_MITER_LIMIT 0x0100
#define FPDFREJEB_REGENLOSS_INLINE_FONT 0x0200

// Returns the FPDFREJEB_REGENLOSS_* flags that apply to |page_object|, 0
// when the generator writes it back faithfully, or -1 on a null
// |page_object|. Inactive objects report 0: the generator skips them.
//
// A colour space flag is raised only when the object actually paints with
// that colour (a stroked path, filled text, a stencil mask image, a form).
// FILL/STROKE_COLORSPACE covers everything that is not DeviceRGB or
// DeviceGray — DeviceCMYK, ICCBased, Separation, Pattern — all written
// back as black. GRAPHICS_STATE covers ExtGState entries beyond alpha and
// blend mode (soft mask, transfer function, overprint, ...).
//
// |page| is the page owning the object and is used to inspect the
// ExtGState dictionaries the object references. Pass NULL to skip that
// inspection; the flags are then a lower bound.
//
// Not reported, because PDFium keeps no readable record of them: flatness
// (`i`) and rendering intent (`ri`) set by their own operators. Neither
// changes how PDFium renders the page.
//
// Backed by patches/0016-export-pageobj-regen-loss-flags.patch.
FPDF_EXPORT int FPDF_CALLCONV
FPDFRejeb_PageObjGetRegenLossFlags(FPDF_PAGE page, FPDF_PAGEOBJECT page_object);

// Returns the index of the content stream |page_object| belongs to — its
// position in the page's /Contents array, 0 for a single stream — or -1
// for a null object or one that was never written to a stream.
//
// Backed by patches/0016-export-pageobj-regen-loss-flags.patch.
FPDF_EXPORT int FPDF_CALLCONV
FPDFRejeb_PageObjGetContentStream(FPDF_PAGEOBJECT page_object);

// Removes loaded objects from |page| without rewriting anything else.
//
// FPDFPage_RemoveObject() makes FPDFPage_GenerateContent() re-serialize
// the object's whole content stream, which loses whatever the generator
// cannot write back (see FPDFREJEB_REGENLOSS_*). This call instead
// overwrites the objects' own bytes with spaces inside the original stream
// data, so every other byte of the page stays as it was:
//   - an image painted with `Do`: its `/Name Do` operator. The graphics
//     state set up around it (q, cm, clip, Q) paints nothing on its own.
//   - any other object: the `q` ... `Q` pair around it, provided that
//     pair paints nothing else.
//
// All or nothing: when one of the |count| objects cannot be erased this
// way, FPDF_FALSE comes back and nothing has changed. That is the case
// for an object that is not an active top-level object of |page| parsed
// from a content stream, that has pending modifications, that shares its
// `q` ... `Q` pair with other content (images excepted), or whose bytes
// are not what was recorded when the page was parsed.
//
// On success the objects are deactivated — they stay in the page's object
// list, so indices do not shift, but they no longer render and must not
// be used further — and resources nothing references any more are dropped
// from the page, which is what lets image data leave the saved file.
//
// Backed by patches/0017-erase-objects-in-place.patch.
FPDF_EXPORT FPDF_BOOL FPDF_CALLCONV
FPDFRejeb_PageEraseObjects(FPDF_PAGE page,
                           const FPDF_PAGEOBJECT* page_objects,
                           int count);

// Moves the content stream holding |page_object| to the front of the
// page's /Contents, so everything in it paints below the rest of the page.
//
// FPDFPage_GenerateContent() always appends new objects as a last stream,
// which paints on top. Generating first and moving that stream afterwards
// puts new content underneath the original page without re-serializing a
// single original object.
//
// Returns FPDF_TRUE when the stream is at the front afterwards, including
// when it already was. Returns FPDF_FALSE, changing nothing, when the
// object has no stream yet, when the page has pending modifications (call
// FPDFPage_GenerateContent() first), or when any content stream leaves
// the transformation matrix changed for the next one — the moved stream
// was written for the matrix in effect at the end of the page.
//
// Backed by patches/0018-export-move-content-stream-to-front.patch.
FPDF_EXPORT FPDF_BOOL FPDF_CALLCONV
FPDFRejeb_PageMoveObjectStreamToFront(FPDF_PAGE page,
                                      FPDF_PAGEOBJECT page_object);

// Applies the page-space transform |matrix| to a loaded object without
// rewriting anything else — what FPDFPageObj_Transform() followed by
// FPDFPageObj_TransformClipPath() does, minus the re-serialization of
// the object's whole content stream.
//
// The transform is written as one `cm` right after the `q` of the
// `q` ... `Q` pair around the object, so everything that pair sets up for
// it moves along: its clip, its soft mask, whatever else. Requires that
// pair to paint nothing but the object. An image that shares its pair
// with other content gets a `q cm ... Q` of its own around its paint
// operator instead; the clip in effect there stays where it is, so that
// only happens while the moved image remains inside it.
//
// Returns FPDF_FALSE, changing nothing, when neither is possible, when
// |matrix| or the matrix in effect around the object cannot be inverted,
// when a soft mask set outside the pair would stay behind, or when the
// object is not an active, unmodified top-level object of |page| parsed
// from a content stream.
//
// Backed by patches/0020-transform-object-in-place.patch.
FPDF_EXPORT FPDF_BOOL FPDF_CALLCONV
FPDFRejeb_PageObjTransformInPlace(FPDF_PAGE page,
                                  FPDF_PAGEOBJECT page_object,
                                  const FS_MATRIX* matrix);

// Renders |page_object| alone, where and how it paints on |page|: placed
// by the matrix of every Form XObject around it, and clipped by its own
// clip path, by the clip path of each of those forms and by the page.
// The object may sit at any depth inside forms, and may be of any type —
// including a shading painted by the `sh` operator, which has no other
// accessor.
//
// |out_rect| receives the part of the page the bitmap covers, in page
// user space: what can show of the object, grown to whole points counted
// from the page's top-left corner. The bitmap is that rect at |scale|
// pixels per point, BGRA, transparent where the object paints nothing.
// Caller owns it — release with FPDFBitmap_Destroy().
//
// Of the forms around the object, matrix, clip and group opacity are
// applied; their soft mask and blend mode are not.
//
// Returns NULL, leaving |out_rect| alone, on null arguments, scale <= 0,
// an object that is not on |page|, nothing of the object showing, or
// bitmap allocation failure.
//
// Backed by patches/0022-export-pageobj-rendered-bitmap-on-page.patch.
FPDF_EXPORT FPDF_BITMAP FPDF_CALLCONV
FPDFRejeb_PageObjGetRenderedBitmapOnPage(FPDF_PAGE page,
                                         FPDF_PAGEOBJECT page_object,
                                         float scale,
                                         FS_RECTF* out_rect);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // PUBLIC_FPDF_REJEB_H_
