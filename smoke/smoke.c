/* smoke.c — minimal runtime smoke test for the patched PDFium build.
 *
 * Compiled and run by scripts/04-build.sh after packaging. The binary
 * exits 0 if PDFium loads, initialises, and exposes every FPDFRejeb_*
 * symbol declared in include/fpdf_rejeb.h. Linker failures or runtime
 * crashes here mean a patch regressed the build; catch before publish.
 *
 * The intent is not to validate FPDFRejeb_* return values — calls are
 * made with NULL/zero args precisely so PDFium's null-arg fast paths
 * return immediately. We only care that the symbols link and don't
 * crash on entry. Real behavioural tests live in downstream consumers.
 */

#include <stdio.h>
#include "fpdfview.h"
#include "fpdf_rejeb.h"

int main(void) {
  FPDF_LIBRARY_CONFIG config;
  config.version            = 2;
  config.m_pUserFontPaths   = NULL;
  config.m_pIsolate         = NULL;
  config.m_v8EmbedderSlot   = 0;
  FPDF_InitLibraryWithConfig(&config);

  /* One call per FPDFRejeb_* symbol with NULL/zero args. PDFium's
   * null-arg fast paths return immediately, so we only verify that
   * the symbol links and doesn't crash on entry. */
  {
    unsigned long out_size = 0;
    (void)FPDFRejeb_TextObjGetCharCodes(NULL, NULL, 0, &out_size);
  }
  {
    FPDF_OCCONTEXT ctx = FPDFRejeb_OCContextCreate(NULL);
    (void)FPDFRejeb_OCContextCheckObjectVisible(ctx, NULL);
    FPDFRejeb_OCContextDestroy(ctx);
  }
  {
    (void)FPDFRejeb_GetOCGCount(NULL);
    (void)FPDFRejeb_GetOCGName(NULL, 0, NULL, 0);
  }

  FPDF_DestroyLibrary();
  printf("OK\n");
  return 0;
}
