/*
 * Copyright © 2016  Google, Inc.
 * Copyright © 2018  Ebrahim Byagowi
 *
 *  This is part of HarfBuzz, a text shaping library.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
 * IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * THE COPYRIGHT HOLDER SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE COPYRIGHT HOLDER HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * Google Author(s): Sascha Brawer
 */

#ifndef HB_OT_COLOR_CPAL_TABLE_HH
#define HB_OT_COLOR_CPAL_TABLE_HH

#include "hb-open-type.hh"
#include "hb-ot-color.h"
#include "hb-ot-name.h"


/*
 * CPAL -- Color Palette
 * https://docs.microsoft.com/en-us/typography/opentype/spec/cpal
 */
#define HB_OT_TAG_CPAL HB_TAG('C','P','A','L')


namespace OT {


struct CPALV1Tail
{
  friend struct CPAL;

  private:
  inline hb_ot_color_palette_flags_t
  get_palette_flags (const void *base,
		     unsigned int palette_index,
		     unsigned int palette_count) const
  {
    if (!paletteFlagsZ) return HB_OT_COLOR_PALETTE_FLAG_DEFAULT;
    return (hb_ot_color_palette_flags_t) (uint32_t)
	   hb_array ((base+paletteFlagsZ).arrayZ, palette_count)[palette_index];
  }

  inline unsigned int
  get_palette_name_id (const void *base,
		       unsigned int palette_index,
		       unsigned int palette_count) const
  {
    if (!paletteLabelsZ) return HB_NAME_ID_INVALID;
    return hb_array ((base+paletteLabelsZ).arrayZ, palette_count)[palette_index];
  }

  inline unsigned int
  get_color_name_id (const void *base,
		     unsigned int color_index,
		     unsigned int color_count) const
  {
    if (!colorLabelsZ) return HB_NAME_ID_INVALID;
    return hb_array ((base+colorLabelsZ).arrayZ, color_count)[color_index];
  }

  public:
  inline bool sanitize (hb_sanitize_context_t *c,
			const void *base,
			unsigned int palette_count,
			unsigned int color_count) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) &&
		  (!paletteFlagsZ  || (base+paletteFlagsZ).sanitize (c, palette_count)) &&
		  (!paletteLabelsZ || (base+paletteLabelsZ).sanitize (c, palette_count)) &&
		  (!colorLabelsZ   || (base+colorLabelsZ).sanitize (c, color_count)));
  }

  protected:
  LOffsetTo<UnsizedArrayOf<HBUINT32>, false>
		paletteFlagsZ;		/* Offset from the beginning of CPAL table to
					 * the Palette Type Array. Set to 0 if no array
					 * is provided. */
  LOffsetTo<UnsizedArrayOf<NameID>, false>
		paletteLabelsZ;		/* Offset from the beginning of CPAL table to
					 * the palette labels array. Set to 0 if no
					 * array is provided. */
  LOffsetTo<UnsizedArrayOf<NameID>, false>
		colorLabelsZ;		/* Offset from the beginning of CPAL table to
					 * the color labels array. Set to 0
					 * if no array is provided. */
  public:
  DEFINE_SIZE_STATIC (12);
};

typedef HBUINT32 BGRAColor;

struct CPAL
{
  static const hb_tag_t tableTag = HB_OT_TAG_CPAL;

  inline bool has_data (void) const { return numPalettes; }

  inline unsigned int get_size (void) const
  { return min_size + numPalettes * sizeof (colorRecordIndicesZ[0]); }

  inline unsigned int get_palette_count () const { return numPalettes; }
  inline unsigned int get_color_count () const { return numColors; }

  inline hb_ot_color_palette_flags_t get_palette_flags (unsigned int palette_index) const
  { return v1 ().get_palette_flags (this, palette_index, numPalettes); }

  inline unsigned int get_palette_name_id (unsigned int palette_index) const
  { return v1 ().get_palette_name_id (this, palette_index, numPalettes); }

  inline unsigned int get_color_name_id (unsigned int color_index) const
  { return v1 ().get_color_name_id (this, color_index, numColors); }

  inline unsigned int get_palette_colors (unsigned int  palette_index,
					  unsigned int  start_offset,
					  unsigned int *color_count, /* IN/OUT.  May be NULL. */
					  hb_color_t   *colors       /* OUT.     May be NULL. */) const
  {
    if (unlikely (palette_index >= numPalettes))
    {
      if (color_count) *color_count = 0;
      return 0;
    }
    unsigned int start_index = colorRecordIndicesZ[palette_index];
    hb_array_t<const BGRAColor> all_colors ((this+colorRecordsZ).arrayZ, numColorRecords);
    hb_array_t<const BGRAColor> palette_colors = all_colors.sub_array (start_index,
								       numColors);
    if (color_count)
    {
      hb_array_t<const BGRAColor> segment_colors = palette_colors.sub_array (start_offset, *color_count);
      /* Always return numColors colors per palette even if it has out-of-bounds start index. */
      unsigned int count = MIN<unsigned int> (MAX<int> (numColors - start_offset, 0), *color_count);
      *color_count = count;
      for (unsigned int i = 0; i < count; i++)
        colors[i] = segment_colors[i]; /* Bound-checked read. */
    }
    return numColors;
  }

  private:
  inline const CPALV1Tail& v1 (void) const
  {
    if (version == 0) return Null(CPALV1Tail);
    return StructAfter<CPALV1Tail> (*this);
  }

  public:
  inline bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) &&
		  (this+colorRecordsZ).sanitize (c, numColorRecords) &&
		  colorRecordIndicesZ.sanitize (c, numPalettes) &&
		  (version == 0 || v1 ().sanitize (c, this, numPalettes, numColors)));
  }

  protected:
  HBUINT16	version;		/* Table version number */
  /* Version 0 */
  HBUINT16	numColors;		/* Number of colors in each palette. */
  HBUINT16	numPalettes;		/* Number of palettes in the table. */
  HBUINT16	numColorRecords;	/* Total number of color records, combined for
					 * all palettes. */
  LOffsetTo<UnsizedArrayOf<BGRAColor>, false>
		colorRecordsZ;		/* Offset from the beginning of CPAL table to
					 * the first ColorRecord. */
  UnsizedArrayOf<HBUINT16>
		colorRecordIndicesZ;	/* Index of each palette’s first color record in
					 * the combined color record array. */
/*CPALV1Tail	v1;*/
  public:
  DEFINE_SIZE_ARRAY (12, colorRecordIndicesZ);
};

} /* namespace OT */


#endif /* HB_OT_COLOR_CPAL_TABLE_HH */
