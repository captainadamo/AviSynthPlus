// Avisynth v2.5.  Copyright 2002 Ben Rudiak-Gould et al.
// http://www.avisynth.org

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA, or visit
// http://www.gnu.org/copyleft/gpl.html .
//
// Linking Avisynth statically or dynamically with other modules is making a
// combined work based on Avisynth.  Thus, the terms and conditions of the GNU
// General Public License cover the whole combination.
//
// As a special exception, the copyright holders of Avisynth give you
// permission to link Avisynth with independent modules that communicate with
// Avisynth solely through the interfaces defined in avisynth.h, regardless of the license
// terms of these independent modules, and to copy and distribute the
// resulting combined work under terms of your choice, provided that
// every copy of the combined work is accompanied by a complete copy of
// the source code of Avisynth (the version of Avisynth used to produce the
// combined work), being distributed under the terms of the GNU General
// Public License plus this exception.  An independent module is a module
// which is not derived from or based on Avisynth, such as 3rd-party filters,
// import and export plugins, or graphical user interfaces.


// Avisynth filter: YUV merge / Swap planes
// by Klaus Post (kp@interact.dk)
// adapted by Richard Berg (avisynth-dev@richardberg.net)
// iSSE code by Ian Brabham


#include "merge.h"
#include "../core/internal.h"
#include <emmintrin.h>
#include "avs/alignment.h"


/* -----------------------------------
 *     weighted_merge_chroma_yuy2
 * -----------------------------------
 */
static void weighted_merge_chroma_yuy2_sse2(BYTE *src, const BYTE *chroma, int pitch, int chroma_pitch,int width, int height, int weight, int invweight )
{
  __m128i round_mask = _mm_set1_epi32(0x4000);
  __m128i mask = _mm_set_epi16(weight, invweight, weight, invweight, weight, invweight, weight, invweight);
  __m128i luma_mask = _mm_set1_epi16(0x00FF);

  int wMod16 = (width/16) * 16;

  for (int y = 0; y < height; y++) {
    for (int x = 0; x < wMod16; x += 16) {
      __m128i px1 = _mm_load_si128(reinterpret_cast<const __m128i*>(src+x)); 
      __m128i px2 = _mm_load_si128(reinterpret_cast<const __m128i*>(chroma+x)); 

      __m128i src_lo = _mm_unpacklo_epi16(px1, px2); 
      __m128i src_hi = _mm_unpackhi_epi16(px1, px2); 

      src_lo = _mm_srli_epi16(src_lo, 8);
      src_hi = _mm_srli_epi16(src_hi, 8);

      src_lo = _mm_madd_epi16(src_lo, mask);
      src_hi = _mm_madd_epi16(src_hi, mask);

      src_lo = _mm_add_epi32(src_lo, round_mask);
      src_hi = _mm_add_epi32(src_hi, round_mask);

      src_lo = _mm_srli_epi32(src_lo, 15);
      src_hi = _mm_srli_epi32(src_hi, 15);

      __m128i result_chroma = _mm_packs_epi32(src_lo, src_hi);
      result_chroma = _mm_slli_epi16(result_chroma, 8);

      __m128i result_luma = _mm_and_si128(px1, luma_mask);
      __m128i result = _mm_or_si128(result_chroma, result_luma);

      _mm_store_si128(reinterpret_cast<__m128i*>(src+x), result);
    }

    for (int x = wMod16; x < width; x+=2) {
      src[x+1] = (chroma[x+1] * weight + src[x+1] * invweight + 16384) >> 15;
    }

    src += pitch;
    chroma += chroma_pitch;
  }
}

#ifdef X86_32
static void weighted_merge_chroma_yuy2_mmx(BYTE *src, const BYTE *chroma, int pitch, int chroma_pitch,int width, int height, int weight, int invweight )
{
  __m64 round_mask = _mm_set1_pi32(0x4000);
  __m64 mask = _mm_set_pi16(weight, invweight, weight, invweight);
  __m64 luma_mask = _mm_set1_pi16(0x00FF);

  int wMod8 = (width/8) * 8;

  for (int y = 0; y < height; y++) {
    for (int x = 0; x < wMod8; x += 8) {
      __m64 px1 = *reinterpret_cast<const __m64*>(src+x); //V1 Y3 U1 Y2 V0 Y1 U0 Y0
      __m64 px2 = *reinterpret_cast<const __m64*>(chroma+x); //v1 y3 u1 y2 v0 y1 u0 y0

      __m64 src_lo = _mm_unpacklo_pi16(px1, px2); //v0 y1 V0 Y1 u0 y0 U0 Y0
      __m64 src_hi = _mm_unpackhi_pi16(px1, px2); 

      src_lo = _mm_srli_pi16(src_lo, 8); //00 v0 00 V0 00 u0 00 U0
      src_hi = _mm_srli_pi16(src_hi, 8); 

      src_lo = _mm_madd_pi16(src_lo, mask);
      src_hi = _mm_madd_pi16(src_hi, mask);

      src_lo = _mm_add_pi32(src_lo, round_mask);
      src_hi = _mm_add_pi32(src_hi, round_mask);

      src_lo = _mm_srli_pi32(src_lo, 15);
      src_hi = _mm_srli_pi32(src_hi, 15);

      __m64 result_chroma = _mm_packs_pi32(src_lo, src_hi);
      result_chroma = _mm_slli_pi16(result_chroma, 8);

      __m64 result_luma = _mm_and_si64(px1, luma_mask);
      __m64 result = _mm_or_si64(result_chroma, result_luma);

      *reinterpret_cast<__m64*>(src+x) = result;
    }

    for (int x = wMod8; x < width; x+=2) {
      src[x+1] = (chroma[x+1] * weight + src[x+1] * invweight + 16384) >> 15;
    }

    src += pitch;
    chroma += chroma_pitch;
  }
  _mm_empty();
}
#endif

static void weighted_merge_chroma_yuy2_c(BYTE *src, const BYTE *chroma, int pitch, int chroma_pitch,int width, int height, int weight, int invweight) {
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; x+=2) {
      src[x+1] = (chroma[x+1] * weight + src[x+1] * invweight + 16384) >> 15;
    }
    src+=pitch;
    chroma+=chroma_pitch;
  }
}


/* -----------------------------------
 *      weighted_merge_luma_yuy2
 * -----------------------------------
 */
static void weighted_merge_luma_yuy2_sse2(BYTE *src, const BYTE *luma, int pitch, int luma_pitch,int width, int height, int weight, int invweight)
{
  __m128i round_mask = _mm_set1_epi32(0x4000);
  __m128i mask = _mm_set_epi16(weight, invweight, weight, invweight, weight, invweight, weight, invweight);
  __m128i luma_mask = _mm_set1_epi16(0x00FF);
#pragma warning(push)
#pragma warning(disable: 4309)
  __m128i chroma_mask = _mm_set1_epi16(0xFF00);
#pragma warning(pop)

  int wMod16 = (width/16) * 16;

  for (int y = 0; y < height; y++) {
    for (int x = 0; x < wMod16; x += 16) {
      __m128i px1 = _mm_load_si128(reinterpret_cast<const __m128i*>(src+x)); //V1 Y3 U1 Y2 V0 Y1 U0 Y0
      __m128i px2 = _mm_load_si128(reinterpret_cast<const __m128i*>(luma+x)); //v1 y3 u1 y2 v0 y1 u0 y0

      __m128i src_lo = _mm_unpacklo_epi16(px1, px2); //v0 y1 V0 Y1 u0 y0 U0 Y0
      __m128i src_hi = _mm_unpackhi_epi16(px1, px2); 

      src_lo = _mm_and_si128(src_lo, luma_mask); //00 v0 00 V0 00 u0 00 U0
      src_hi = _mm_and_si128(src_hi, luma_mask); 

      src_lo = _mm_madd_epi16(src_lo, mask);
      src_hi = _mm_madd_epi16(src_hi, mask);

      src_lo = _mm_add_epi32(src_lo, round_mask);
      src_hi = _mm_add_epi32(src_hi, round_mask);

      src_lo = _mm_srli_epi32(src_lo, 15);
      src_hi = _mm_srli_epi32(src_hi, 15);

      __m128i result_luma = _mm_packs_epi32(src_lo, src_hi);

      __m128i result_chroma = _mm_and_si128(px1, chroma_mask);
      __m128i result = _mm_or_si128(result_chroma, result_luma);

      _mm_store_si128(reinterpret_cast<__m128i*>(src+x), result);
    }

    for (int x = wMod16; x < width; x+=2) {
      src[x] = (luma[x] * weight + src[x] * invweight + 16384) >> 15;
    }

    src += pitch;
    luma += luma_pitch;
  }
}

#ifdef X86_32
static void weighted_merge_luma_yuy2_mmx(BYTE *src, const BYTE *luma, int pitch, int luma_pitch,int width, int height, int weight, int invweight)
{
  __m64 round_mask = _mm_set1_pi32(0x4000);
  __m64 mask = _mm_set_pi16(weight, invweight, weight, invweight);
  __m64 luma_mask = _mm_set1_pi16(0x00FF);
#pragma warning(push)
#pragma warning(disable: 4309)
  __m64 chroma_mask = _mm_set1_pi16(0xFF00);
#pragma warning(pop)

  int wMod8 = (width/8) * 8;

  for (int y = 0; y < height; y++) {
    for (int x = 0; x < wMod8; x += 8) {
      __m64 px1 = *reinterpret_cast<const __m64*>(src+x); //V1 Y3 U1 Y2 V0 Y1 U0 Y0
      __m64 px2 = *reinterpret_cast<const __m64*>(luma+x); //v1 y3 u1 y2 v0 y1 u0 y0

      __m64 src_lo = _mm_unpacklo_pi16(px1, px2); //v0 y1 V0 Y1 u0 y0 U0 Y0
      __m64 src_hi = _mm_unpackhi_pi16(px1, px2); 

      src_lo = _mm_and_si64(src_lo, luma_mask); //00 v0 00 V0 00 u0 00 U0
      src_hi = _mm_and_si64(src_hi, luma_mask); 

      src_lo = _mm_madd_pi16(src_lo, mask);
      src_hi = _mm_madd_pi16(src_hi, mask);

      src_lo = _mm_add_pi32(src_lo, round_mask);
      src_hi = _mm_add_pi32(src_hi, round_mask);

      src_lo = _mm_srli_pi32(src_lo, 15);
      src_hi = _mm_srli_pi32(src_hi, 15);

      __m64 result_luma = _mm_packs_pi32(src_lo, src_hi);

      __m64 result_chroma = _mm_and_si64(px1, chroma_mask);
      __m64 result = _mm_or_si64(result_chroma, result_luma);

      *reinterpret_cast<__m64*>(src+x) = result;
    }

    for (int x = wMod8; x < width; x+=2) {
      src[x] = (luma[x] * weight + src[x] * invweight + 16384) >> 15;
    }

    src += pitch;
    luma += luma_pitch;
  }
  _mm_empty();
}
#endif

static void weighted_merge_luma_yuy2_c(BYTE *src, const BYTE *luma, int pitch, int luma_pitch,int width, int height, int weight, int invweight) {
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; x+=2) {
      src[x] = (luma[x] * weight + src[x] * invweight + 16384) >> 15;
    }
    src+=pitch;
    luma+=luma_pitch;
  }
}


/* -----------------------------------
 *          replace_luma_yuy2
 * -----------------------------------
 */
static void replace_luma_yuy2_sse2(BYTE *src, const BYTE *luma, int pitch, int luma_pitch,int width, int height)
{
  int mod16_width = width / 16 * 16;
  __m128i luma_mask = _mm_set1_epi16(0x00FF);
#pragma warning(push)
#pragma warning(disable: 4309)
  __m128i chroma_mask = _mm_set1_epi16(0xFF00);
#pragma warning(pop)

  for(int y = 0; y < height; y++) {
    for(int x = 0; x < mod16_width; x+=16) {
      __m128i s = _mm_load_si128(reinterpret_cast<const __m128i*>(src+x));
      __m128i l = _mm_load_si128(reinterpret_cast<const __m128i*>(luma+x));

      __m128i s_chroma = _mm_and_si128(s, chroma_mask);
      __m128i l_luma = _mm_and_si128(l, luma_mask);

      __m128i result = _mm_or_si128(s_chroma, l_luma);

      _mm_store_si128(reinterpret_cast<__m128i*>(src+x), result);
    }

    for (int x = mod16_width; x < width; x+=2) {
      src[x] = luma[x];
    }
    src += pitch;
    luma += luma_pitch;
  }
}

#ifdef X86_32
static void replace_luma_yuy2_mmx(BYTE *src, const BYTE *luma, int pitch, int luma_pitch,int width, int height)
{
  int mod8_width = width / 8 * 8;
  __m64 luma_mask = _mm_set1_pi16(0x00FF);
#pragma warning(push)
#pragma warning(disable: 4309)
  __m64 chroma_mask = _mm_set1_pi16(0xFF00);
#pragma warning(pop)

  for(int y = 0; y < height; y++) {
    for(int x = 0; x < mod8_width; x+=8) {
      __m64 s = *reinterpret_cast<const __m64*>(src+x);
      __m64 l = *reinterpret_cast<const __m64*>(luma+x);

      __m64 s_chroma = _mm_and_si64(s, chroma_mask);
      __m64 l_luma = _mm_and_si64(l, luma_mask);

      __m64 result = _mm_or_si64(s_chroma, l_luma);

      *reinterpret_cast<__m64*>(src+x) = result;
    }

    for (int x = mod8_width; x < width; x+=2) {
      src[x] = luma[x];
    }
    src += pitch;
    luma += luma_pitch;
  }
  _mm_empty();
}
#endif

static void replace_luma_yuy2_c(BYTE *src, const BYTE *luma, int pitch, int luma_pitch,int width, int height ) {
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; x+=2) {
      src[x] = luma[x];
    }
    src+=pitch;
    luma+=luma_pitch;
  }
}


/* -----------------------------------
 *            average_plane
 * -----------------------------------
 */
static void average_plane_sse2(BYTE *p1, const BYTE *p2, int p1_pitch, int p2_pitch, int width, int height) {
  int mod16_width = width / 16 * 16;

  for(int y = 0; y < height; y++) {
    for(int x = 0; x < mod16_width; x+=16) {
      __m128i src1  = _mm_load_si128(reinterpret_cast<const __m128i*>(p1+x));
      __m128i src2  = _mm_load_si128(reinterpret_cast<const __m128i*>(p2+x));

      __m128i dst  = _mm_avg_epu8(src1, src2);

      _mm_store_si128(reinterpret_cast<__m128i*>(p1+x), dst);
    }

    if (mod16_width != width) {
      for (int x = mod16_width; x < width; ++x) {
        p1[x] = (int(p1[x]) + p2[x] + 1) >> 1;
      }
    }
    p1 += p1_pitch;
    p2 += p2_pitch;
  }
}

#ifdef X86_32
static void average_plane_isse(BYTE *p1, const BYTE *p2, int p1_pitch, int p2_pitch, int width, int height) {
  int mod8_width = width / 8 * 8;

  for(int y = 0; y < height; y++) {
    for(int x = 0; x < mod8_width; x+=8) {
      __m64 src1 = *reinterpret_cast<const __m64*>(p1+x);
      __m64 src2 = *reinterpret_cast<const __m64*>(p2+x);
      __m64 dst = _mm_avg_pu8(src1, src2);
      *reinterpret_cast<__m64*>(p1+x) = dst;
    }

    if (mod8_width != width) {
      for (int x = mod8_width; x < width; ++x) {
        p1[x] = (int(p1[x]) + p2[x] + 1) >> 1;
      }
    }
    p1 += p1_pitch;
    p2 += p2_pitch;
  }
  _mm_empty();
}
#endif

static void average_plane_c(BYTE *p1, const BYTE *p2, int p1_pitch, int p2_pitch, int width, int height) {
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      p1[x] = (int(p1[x]) + p2[x] + 1) >> 1;
    }
    p1 += p1_pitch;
    p2 += p2_pitch;
  }
}


/* -----------------------------------
 *       weighted_merge_planar
 * -----------------------------------
 */
void weighted_merge_planar_sse2(BYTE *p1, const BYTE *p2, int p1_pitch, int p2_pitch, int width, int height, int weight, int invweight) {
  __m128i round_mask = _mm_set1_epi32(0x4000);
  __m128i zero = _mm_setzero_si128();
  __m128i mask = _mm_set_epi16(weight, invweight, weight, invweight, weight, invweight, weight, invweight);

  int wMod16 = (width/16) * 16;

  for (int y = 0; y < height; y++) {
    for (int x = 0; x < wMod16; x += 16) {
      __m128i px1 = _mm_load_si128(reinterpret_cast<const __m128i*>(p1+x)); //y7y6 y5y4 y3y2 y1y0
      __m128i px2 = _mm_load_si128(reinterpret_cast<const __m128i*>(p2+x)); //Y7Y6 Y5Y4 Y3Y2 Y1Y0

      __m128i p0123 = _mm_unpacklo_epi8(px1, px2); //Y3y3 Y2y2 Y1y1 Y0y0
      __m128i p4567 = _mm_unpackhi_epi8(px1, px2); //Y7y7 Y6y6 Y5y5 Y4y4

      __m128i p01 = _mm_unpacklo_epi8(p0123, zero); //00Y1 00y1 00Y0 00y0
      __m128i p23 = _mm_unpackhi_epi8(p0123, zero); //00Y3 00y3 00Y2 00y2
      __m128i p45 = _mm_unpacklo_epi8(p4567, zero); //00Y5 00y5 00Y4 00y4
      __m128i p67 = _mm_unpackhi_epi8(p4567, zero); //00Y7 00y7 00Y6 00y6

      p01 = _mm_madd_epi16(p01, mask);
      p23 = _mm_madd_epi16(p23, mask);
      p45 = _mm_madd_epi16(p45, mask);
      p67 = _mm_madd_epi16(p67, mask);

      p01 = _mm_add_epi32(p01, round_mask);
      p23 = _mm_add_epi32(p23, round_mask);
      p45 = _mm_add_epi32(p45, round_mask);
      p67 = _mm_add_epi32(p67, round_mask);

      p01 = _mm_srli_epi32(p01, 15);
      p23 = _mm_srli_epi32(p23, 15);
      p45 = _mm_srli_epi32(p45, 15);
      p67 = _mm_srli_epi32(p67, 15);

      p0123 = _mm_packs_epi32(p01, p23);
      p4567 = _mm_packs_epi32(p45, p67);

      __m128i result = _mm_packus_epi16(p0123, p4567);

      _mm_store_si128(reinterpret_cast<__m128i*>(p1+x), result);
    }

    for (int x = wMod16; x < width; x++) {
      p1[x] = (p1[x]*invweight + p2[x]*weight + 16384) >> 15;
    }

    p1 += p1_pitch;
    p2 += p2_pitch;
  }
}

#ifdef X86_32
void weighted_merge_planar_mmx(BYTE *p1, const BYTE *p2, int p1_pitch, int p2_pitch, int width, int height, int weight, int invweight) {
  __m64 round_mask = _mm_set1_pi32(0x4000);
  __m64 zero = _mm_setzero_si64();
  __m64 mask = _mm_set_pi16(weight, invweight, weight, invweight);

  int wMod8 = (width/8) * 8;

  for (int y = 0; y < height; y++) {
    for (int x = 0; x < wMod8; x += 8) {
      __m64 px1 = *(reinterpret_cast<const __m64*>(p1+x)); //y7y6 y5y4 y3y2 y1y0
      __m64 px2 = *(reinterpret_cast<const __m64*>(p2+x)); //Y7Y6 Y5Y4 Y3Y2 Y1Y0

      __m64 p0123 = _mm_unpacklo_pi8(px1, px2); //Y3y3 Y2y2 Y1y1 Y0y0
      __m64 p4567 = _mm_unpackhi_pi8(px1, px2); //Y7y7 Y6y6 Y5y5 Y4y4

      __m64 p01 = _mm_unpacklo_pi8(p0123, zero); //00Y1 00y1 00Y0 00y0
      __m64 p23 = _mm_unpackhi_pi8(p0123, zero); //00Y3 00y3 00Y2 00y2
      __m64 p45 = _mm_unpacklo_pi8(p4567, zero); //00Y5 00y5 00Y4 00y4
      __m64 p67 = _mm_unpackhi_pi8(p4567, zero); //00Y7 00y7 00Y6 00y6

      p01 = _mm_madd_pi16(p01, mask);
      p23 = _mm_madd_pi16(p23, mask);
      p45 = _mm_madd_pi16(p45, mask);
      p67 = _mm_madd_pi16(p67, mask);

      p01 = _mm_add_pi32(p01, round_mask);
      p23 = _mm_add_pi32(p23, round_mask);
      p45 = _mm_add_pi32(p45, round_mask);
      p67 = _mm_add_pi32(p67, round_mask);

      p01 = _mm_srli_pi32(p01, 15);
      p23 = _mm_srli_pi32(p23, 15);
      p45 = _mm_srli_pi32(p45, 15);
      p67 = _mm_srli_pi32(p67, 15);

      p0123 = _mm_packs_pi32(p01, p23);
      p4567 = _mm_packs_pi32(p45, p67);

      __m64 result = _mm_packs_pu16(p0123, p4567);

      *reinterpret_cast<__m64*>(p1+x) = result;
    }

    for (int x = wMod8; x < width; x++) {
      p1[x] = (p1[x]*invweight + p2[x]*weight + 16384) >> 15;
    }

    p1 += p1_pitch;
    p2 += p2_pitch;
  }
  _mm_empty();
}
#endif

void weighted_merge_planar_c(BYTE *p1, const BYTE *p2, int p1_pitch, int p2_pitch,int rowsize, int height, int weight, int invweight) {

  for (int y=0;y<height;y++) {
    for (int x=0;x<rowsize;x++) {
      p1[x] = (p1[x]*invweight + p2[x]*weight + 32768) >> 16;
    }
    p2+=p2_pitch;
    p1+=p1_pitch;
  }
}


/********************************************************************
***** Declare index of new filters for Avisynth's filter engine *****
********************************************************************/
extern const AVSFunction Merge_filters[] = {
  { "Merge", "cc[weight]f", MergeAll::Create },  // src, src2, weight
  { "MergeChroma", "cc[weight]f", MergeChroma::Create },  // src, chroma src, weight
  { "MergeChroma", "cc[chromaweight]f", MergeChroma::Create },  // Legacy!
  { "MergeLuma", "cc[weight]f", MergeLuma::Create },      // src, luma src, weight
  { "MergeLuma", "cc[lumaweight]f", MergeLuma::Create },      // Legacy!
  { 0 }
};

static void merge_plane(BYTE* srcp, const BYTE* otherp, int src_pitch, int other_pitch, int src_width, int src_height, float weight, IScriptEnvironment *env) {
  if ((weight>0.4961f) && (weight<0.5039f)) 
  {
    //average of two planes
    if ((env->GetCPUFlags() & CPUF_SSE2) && IsPtrAligned(srcp, 16) && IsPtrAligned(otherp, 16)) {
      average_plane_sse2(srcp, otherp, src_pitch, other_pitch, src_width, src_height);
    } 
    else 
#ifdef X86_32
      if (env->GetCPUFlags() & CPUF_INTEGER_SSE) {
        average_plane_isse(srcp, otherp, src_pitch, other_pitch, src_width, src_height);
      } else
#endif
      {
        average_plane_c(srcp, otherp, src_pitch, other_pitch, src_width, src_height);
      }
  } 
  else 
  {
    int iweight = (int)(weight*32767.0f);
    int invweight = 32767-iweight;
    //real merge
    if ((env->GetCPUFlags() & CPUF_SSE2) && IsPtrAligned(srcp, 16) && IsPtrAligned(otherp, 16))
    {
      weighted_merge_planar_sse2(srcp, otherp, src_pitch, other_pitch, src_width, src_height, iweight, invweight);
    }
    else
#ifdef X86_32
      if (env->GetCPUFlags() & CPUF_MMX)
      {
        weighted_merge_planar_mmx(srcp, otherp, src_pitch, other_pitch, src_width, src_height, iweight, invweight);
      }
      else
#endif
      {
        iweight = (int)(weight*65535.0f);
        invweight = 65535-iweight;
        weighted_merge_planar_c(srcp, otherp, src_pitch, other_pitch, src_width, src_height, iweight, invweight);
      }
  }
}

/****************************
******   Merge Chroma   *****
****************************/

MergeChroma::MergeChroma(PClip _child, PClip _clip, float _weight, IScriptEnvironment* env)
  : GenericVideoFilter(_child), clip(_clip), weight(_weight)
{
  const VideoInfo& vi2 = clip->GetVideoInfo();

  if (!vi.IsYUV() || !vi2.IsYUV())
    env->ThrowError("MergeChroma: YUV data only (no RGB); use ConvertToYUY2 or ConvertToYV12");

  if (!(vi.IsSameColorspace(vi2)))
    env->ThrowError("MergeChroma: YUV images must have same data type.");

  if (vi.width!=vi2.width || vi.height!=vi2.height)
    env->ThrowError("MergeChroma: Images must have same width and height!");

  if (weight<0.0f) weight=0.0f;
  if (weight>1.0f) weight=1.0f;
}


PVideoFrame __stdcall MergeChroma::GetFrame(int n, IScriptEnvironment* env)
{
  PVideoFrame src = child->GetFrame(n, env);

  if (weight<0.0039f) return src;

  PVideoFrame chroma = clip->GetFrame(n, env);

  int h = src->GetHeight();
  int w = src->GetRowSize(); // width in pixels

  if (weight<0.9961f) {
    if (vi.IsYUY2()) {
      env->MakeWritable(&src);
      BYTE* srcp = src->GetWritePtr();
      const BYTE* chromap = chroma->GetReadPtr();

      int src_pitch = src->GetPitch(); 
      int chroma_pitch = chroma->GetPitch();

      if ((env->GetCPUFlags() & CPUF_SSE2) && IsPtrAligned(srcp, 16) && IsPtrAligned(chromap, 16))
      {
        weighted_merge_chroma_yuy2_sse2(srcp,chromap,src_pitch,chroma_pitch,w,h,(int)(weight*32768.0f),32768-(int)(weight*32768.0f));
      }
      else
#ifdef X86_32
      if (env->GetCPUFlags() & CPUF_MMX)
      {
        weighted_merge_chroma_yuy2_mmx(srcp,chromap,src_pitch,chroma_pitch,w,h,(int)(weight*32768.0f),32768-(int)(weight*32768.0f));
      }
      else
#endif
      {
        weighted_merge_chroma_yuy2_c(srcp,chromap,src_pitch,chroma_pitch,w,h,(int)(weight*32768.0f),32768-(int)(weight*32768.0f));
      }
    } else {  // Planar
      env->MakeWritable(&src);
      src->GetWritePtr(PLANAR_Y); //Must be requested

      BYTE* srcpU = (BYTE*)src->GetWritePtr(PLANAR_U);
      BYTE* chromapU = (BYTE*)chroma->GetReadPtr(PLANAR_U);
      BYTE* srcpV = (BYTE*)src->GetWritePtr(PLANAR_V);
      BYTE* chromapV = (BYTE*)chroma->GetReadPtr(PLANAR_V);
      int src_pitch_uv = src->GetPitch(PLANAR_U);
      int chroma_pitch_uv = chroma->GetPitch(PLANAR_U);
      int src_width_u = src->GetRowSize(PLANAR_U_ALIGNED);
      int src_width_v = src->GetRowSize(PLANAR_V_ALIGNED);
      int src_height_uv = src->GetHeight(PLANAR_U);

      merge_plane(srcpU, chromapU, src_pitch_uv, chroma_pitch_uv, src_width_u, src_height_uv, weight, env);
      merge_plane(srcpV, chromapV, src_pitch_uv, chroma_pitch_uv, src_width_v, src_height_uv, weight, env);
    }
  } else { // weight == 1.0
    if (vi.IsYUY2()) {
      const BYTE* srcp = src->GetReadPtr();
      env->MakeWritable(&chroma);
      BYTE* chromap = chroma->GetWritePtr();

      int src_pitch = src->GetPitch();  
      int chroma_pitch = chroma->GetPitch(); 

      if ((env->GetCPUFlags() & CPUF_SSE2) && IsPtrAligned(chromap, 16) && IsPtrAligned(srcp, 16))
      {
        replace_luma_yuy2_sse2(chromap,srcp,chroma_pitch,src_pitch,w,h);  // Just swap luma/chroma
      }
      else
#ifdef X86_32
      if (env->GetCPUFlags() & CPUF_MMX)
      {
        replace_luma_yuy2_mmx(chromap,srcp,chroma_pitch,src_pitch,w,h);  // Just swap luma/chroma
      }
      else
#endif
      {
        replace_luma_yuy2_c(chromap,srcp,chroma_pitch,src_pitch,w,h);  // Just swap luma/chroma
      }

      return chroma;
    } else {
      if (src->IsWritable()) {
        src->GetWritePtr(PLANAR_Y); //Must be requested
        env->BitBlt(src->GetWritePtr(PLANAR_U),src->GetPitch(PLANAR_U),chroma->GetReadPtr(PLANAR_U),chroma->GetPitch(PLANAR_U),chroma->GetRowSize(PLANAR_U),chroma->GetHeight(PLANAR_U));
        env->BitBlt(src->GetWritePtr(PLANAR_V),src->GetPitch(PLANAR_V),chroma->GetReadPtr(PLANAR_V),chroma->GetPitch(PLANAR_V),chroma->GetRowSize(PLANAR_V),chroma->GetHeight(PLANAR_V));
      }
      else { // avoid the cost of 2 chroma blits
        PVideoFrame dst = env->NewVideoFrame(vi);
        
        env->BitBlt(dst->GetWritePtr(PLANAR_Y),dst->GetPitch(PLANAR_Y),src->GetReadPtr(PLANAR_Y),src->GetPitch(PLANAR_Y),src->GetRowSize(PLANAR_Y),src->GetHeight(PLANAR_Y));
        env->BitBlt(dst->GetWritePtr(PLANAR_U),dst->GetPitch(PLANAR_U),chroma->GetReadPtr(PLANAR_U),chroma->GetPitch(PLANAR_U),chroma->GetRowSize(PLANAR_U),chroma->GetHeight(PLANAR_U));
        env->BitBlt(dst->GetWritePtr(PLANAR_V),dst->GetPitch(PLANAR_V),chroma->GetReadPtr(PLANAR_V),chroma->GetPitch(PLANAR_V),chroma->GetRowSize(PLANAR_V),chroma->GetHeight(PLANAR_V));
        return dst;
      }
    }
  }
  return src;
}


AVSValue __cdecl MergeChroma::Create(AVSValue args, void* user_data, IScriptEnvironment* env)
{
  return new MergeChroma(args[0].AsClip(), args[1].AsClip(), (float)args[2].AsFloat(1.0f), env);
}


/**************************
******   Merge Luma   *****
**************************/


MergeLuma::MergeLuma(PClip _child, PClip _clip, float _weight, IScriptEnvironment* env)
  : GenericVideoFilter(_child), clip(_clip), weight(_weight)
{
  const VideoInfo& vi2 = clip->GetVideoInfo();

  if (!vi.IsYUV() || !vi2.IsYUV())
    env->ThrowError("MergeLuma: YUV data only (no RGB); use ConvertToYUY2 or ConvertToYV12");

  if (!vi.IsSameColorspace(vi2)) {  // Since this is luma we allow all planar formats to be merged.
    if (vi.IsPlanar() == vi2.IsPlanar()) {
      env->ThrowError("MergeLuma: YUV data is not same type. YUY2 and planar images doesn't mix.");
    }
  }

  if (vi.width!=vi2.width || vi.height!=vi2.height)
    env->ThrowError("MergeLuma: Images must have same width and height!");

  if (weight<0.0f) weight=0.0f;
  if (weight>1.0f) weight=1.0f;
}


PVideoFrame __stdcall MergeLuma::GetFrame(int n, IScriptEnvironment* env)
{
  PVideoFrame src = child->GetFrame(n, env);

  if (weight<0.0039f) return src;

  PVideoFrame luma = clip->GetFrame(n, env);

  if (vi.IsYUY2()) {
    env->MakeWritable(&src);
    BYTE* srcp = src->GetWritePtr();
    const BYTE* lumap = luma->GetReadPtr();

    int isrc_pitch = src->GetPitch(); 
    int iluma_pitch = luma->GetPitch();

    int h = src->GetHeight();
    int w = src->GetRowSize();

    if (weight<0.9961f)
    {
      if ((env->GetCPUFlags() & CPUF_SSE2) && IsPtrAligned(srcp, 16) && IsPtrAligned(lumap, 16))
      {
        weighted_merge_luma_yuy2_sse2(srcp, lumap, isrc_pitch, iluma_pitch, w, h, (int)(weight*32768.0f), 32768-(int)(weight*32768.0f));
      }
      else
#ifdef X86_32
      if (env->GetCPUFlags() & CPUF_MMX)
      {
        weighted_merge_luma_yuy2_mmx(srcp, lumap, isrc_pitch, iluma_pitch, w, h, (int)(weight*32768.0f), 32768-(int)(weight*32768.0f));
      }
      else
#endif
      {
        weighted_merge_luma_yuy2_c(srcp, lumap, isrc_pitch, iluma_pitch, w, h, (int)(weight*32768.0f), 32768-(int)(weight*32768.0f));
      }
    }
    else
    {
      if ((env->GetCPUFlags() & CPUF_SSE2) && IsPtrAligned(srcp, 16) && IsPtrAligned(lumap, 16))
      {
        replace_luma_yuy2_sse2(srcp,lumap,isrc_pitch,iluma_pitch,w,h);
      }
      else
#ifdef X86_32
      if (env->GetCPUFlags() & CPUF_MMX)
      {
        replace_luma_yuy2_mmx(srcp,lumap,isrc_pitch,iluma_pitch,w,h);
      }
      else
#endif
      {
        replace_luma_yuy2_c(srcp,lumap,isrc_pitch,iluma_pitch,w,h);
      }
    }
    return src;
  }  // Planar
  if (weight>0.9961f) {
    const VideoInfo& vi2 = clip->GetVideoInfo();
    if (luma->IsWritable() && vi.IsSameColorspace(vi2)) {
      if (luma->GetRowSize(PLANAR_U)) {
        luma->GetWritePtr(PLANAR_Y); //Must be requested BUT only if we actually do something
        env->BitBlt(luma->GetWritePtr(PLANAR_U),luma->GetPitch(PLANAR_U),src->GetReadPtr(PLANAR_U),src->GetPitch(PLANAR_U),src->GetRowSize(PLANAR_U),src->GetHeight(PLANAR_U));
        env->BitBlt(luma->GetWritePtr(PLANAR_V),luma->GetPitch(PLANAR_V),src->GetReadPtr(PLANAR_V),src->GetPitch(PLANAR_V),src->GetRowSize(PLANAR_V),src->GetHeight(PLANAR_V));
      }
      return luma;
    }
    else { // avoid the cost of 2 chroma blits
      PVideoFrame dst = env->NewVideoFrame(vi);
      
      env->BitBlt(dst->GetWritePtr(PLANAR_Y),dst->GetPitch(PLANAR_Y),luma->GetReadPtr(PLANAR_Y),luma->GetPitch(PLANAR_Y),luma->GetRowSize(PLANAR_Y),luma->GetHeight(PLANAR_Y));
      if (src->GetRowSize(PLANAR_U) && dst->GetRowSize(PLANAR_U)) {
        env->BitBlt(dst->GetWritePtr(PLANAR_U),dst->GetPitch(PLANAR_U),src->GetReadPtr(PLANAR_U),src->GetPitch(PLANAR_U),src->GetRowSize(PLANAR_U),src->GetHeight(PLANAR_U));
        env->BitBlt(dst->GetWritePtr(PLANAR_V),dst->GetPitch(PLANAR_V),src->GetReadPtr(PLANAR_V),src->GetPitch(PLANAR_V),src->GetRowSize(PLANAR_V),src->GetHeight(PLANAR_V));
      }
      return dst;
    }
  } else { // weight <= 0.9961f
    env->MakeWritable(&src);
    BYTE* srcpY = (BYTE*)src->GetWritePtr(PLANAR_Y);
    BYTE* lumapY = (BYTE*)luma->GetReadPtr(PLANAR_Y);
    int src_pitch = src->GetPitch(PLANAR_Y);
    int luma_pitch = luma->GetPitch(PLANAR_Y);
    int src_width = src->GetRowSize(PLANAR_Y);
    int src_height = src->GetHeight(PLANAR_Y);

    merge_plane(srcpY, lumapY, src_pitch, luma_pitch, src_width, src_height, weight, env);
  }

  return src;
}


AVSValue __cdecl MergeLuma::Create(AVSValue args, void* user_data, IScriptEnvironment* env)
{
  return new MergeLuma(args[0].AsClip(), args[1].AsClip(), (float)args[2].AsFloat(1.0f), env);
}


/*************************
******   Merge All   *****
*************************/


MergeAll::MergeAll(PClip _child, PClip _clip, float _weight, IScriptEnvironment* env)
  : GenericVideoFilter(_child), clip(_clip), weight(_weight)
{
  const VideoInfo& vi2 = clip->GetVideoInfo();

  if (!vi.IsSameColorspace(vi2))
    env->ThrowError("Merge: Pixel types are not the same. Both must be the same.");

  if (vi.width!=vi2.width || vi.height!=vi2.height)
    env->ThrowError("Merge: Images must have same width and height!");

  if (weight<0.0f) weight=0.0f;
  if (weight>1.0f) weight=1.0f;
}


PVideoFrame __stdcall MergeAll::GetFrame(int n, IScriptEnvironment* env)
{
  if (weight<0.0039f) return child->GetFrame(n, env);
  if (weight>0.9961f) return clip->GetFrame(n, env);

  PVideoFrame src  = child->GetFrame(n, env);
  PVideoFrame src2 =  clip->GetFrame(n, env);

  env->MakeWritable(&src);
  BYTE* srcp  = src->GetWritePtr();
  const BYTE* srcp2 = src2->GetReadPtr();

  const int src_pitch = src->GetPitch();
  const int src_rowsize = src->GetRowSize();

  merge_plane(srcp, srcp2, src_pitch, src2->GetPitch(), src_rowsize, src->GetHeight(), weight, env);

  if (vi.IsPlanar()) {
    BYTE* srcpU  = (BYTE*)src->GetWritePtr(PLANAR_U);
    BYTE* srcpV  = (BYTE*)src->GetWritePtr(PLANAR_V);
    BYTE* srcp2U = (BYTE*)src2->GetReadPtr(PLANAR_U);
    BYTE* srcp2V = (BYTE*)src2->GetReadPtr(PLANAR_V);
 
    int src_rowsize = src->GetRowSize(PLANAR_U);

    merge_plane(srcpU, srcp2U, src->GetPitch(PLANAR_U), src2->GetPitch(PLANAR_U), src_rowsize, src->GetHeight(PLANAR_U), weight, env);
    merge_plane(srcpV, srcp2V, src->GetPitch(PLANAR_V), src2->GetPitch(PLANAR_V), src_rowsize, src->GetHeight(PLANAR_V), weight, env);
  }

  return src;
}


AVSValue __cdecl MergeAll::Create(AVSValue args, void* user_data, IScriptEnvironment* env)
{
  return new MergeAll(args[0].AsClip(), args[1].AsClip(), (float)args[2].AsFloat(0.5f), env);
}
