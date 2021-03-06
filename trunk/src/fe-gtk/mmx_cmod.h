#ifndef RAGE_MMX_CMOD_H
#define RAGE_MMX_CMOD_H

void shade_ximage_15_mmx(void *data, int bpl, int w, int h, int rm, int gm, int bm);
void shade_ximage_16_mmx(void *data, int bpl, int w, int h, int rm, int gm, int bm);
void shade_ximage_32_mmx(void *data, int bpl, int w, int h, int rm, int gm, int bm);
int have_mmx (void);

#endif /* RAGE_MMX_CMOD_H */
