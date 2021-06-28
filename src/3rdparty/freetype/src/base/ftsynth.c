/****************************************************************************
 *
 * ftsynth.c
 *
 *   FreeType synthesizing code for emboldening and slanting (body).
 *
 * Copyright (C) 2000-2019 by
 * David Turner, Robert Wilhelm, and Werner Lemberg.
 *
 * This file is part of the FreeType project, and may only be used,
 * modified, and distributed under the terms of the FreeType project
 * license, LICENSE.TXT.  By continuing to use, modify, or distribute
 * this file you indicate that you have read the license and
 * understand and accept it fully.
 *
 */


#include <ft2build.h>
#include FT_SYNTHESIS_H
#include FT_INTERNAL_DEBUG_H
#include FT_INTERNAL_OBJECTS_H
#include FT_OUTLINE_H
#include FT_BITMAP_H
#include FT_INTERNAL_SFNT_H
#include FT_SERVICE_SFNT_H

  /**************************************************************************
   *
   * The macro FT_COMPONENT is used in trace mode.  It is an implicit
   * parameter of the FT_TRACE() and FT_ERROR() macros, used to print/log
   * messages during execution.
   */
#undef  FT_COMPONENT
#define FT_COMPONENT  synth


  /*************************************************************************/
  /*************************************************************************/
  /****                                                                 ****/
  /****   EXPERIMENTAL OBLIQUING SUPPORT                                ****/
  /****                                                                 ****/
  /*************************************************************************/
  /*************************************************************************/

  /* documentation is in ftsynth.h */

  FT_EXPORT_DEF( void )
  FT_GlyphSlot_Oblique( FT_GlyphSlot  slot )
  {
    FT_Matrix    transform;
    FT_Outline*  outline;


    if ( !slot )
      return;

    outline = &slot->outline;

    /* only oblique outline glyphs */
    if ( slot->format != FT_GLYPH_FORMAT_OUTLINE )
      return;

    /* we don't touch the advance width */

    /* For italic, simply apply a shear transform, with an angle */
    /* of about 12 degrees.                                      */

    transform.xx = 0x10000L;
    transform.yx = 0x00000L;

    transform.xy = 0x0366AL;
    transform.yy = 0x10000L;

    FT_Outline_Transform( outline, &transform );
  }


  /*************************************************************************/
  /*************************************************************************/
  /****                                                                 ****/
  /****   EXPERIMENTAL EMBOLDENING SUPPORT                              ****/
  /****                                                                 ****/
  /*************************************************************************/
  /*************************************************************************/


  /* documentation is in ftsynth.h */

  FT_EXPORT_DEF( void )
  FT_GlyphSlot_Embolden( FT_GlyphSlot  slot )
  {
    FT_Library  library;
    FT_Face     face;
    FT_Error    error;
    FT_Pos      xstr, ystr;


    if ( !slot )
      return;

    library = slot->library;
    face    = slot->face;

    if ( slot->format != FT_GLYPH_FORMAT_OUTLINE &&
         slot->format != FT_GLYPH_FORMAT_BITMAP  )
      return;

    /* some reasonable strength */
    xstr = FT_MulFix( face->units_per_EM,
                      face->size->metrics.y_scale ) / 24;
    ystr = xstr;

    if ( slot->format == FT_GLYPH_FORMAT_OUTLINE )
      FT_Outline_EmboldenXY( &slot->outline, xstr, ystr );

    else /* slot->format == FT_GLYPH_FORMAT_BITMAP */
    {
      /* round to full pixels */
      xstr &= ~63;
      if ( xstr == 0 )
        xstr = 1 << 6;
      ystr &= ~63;

      /*
       * XXX: overflow check for 16-bit system, for compatibility
       *      with FT_GlyphSlot_Embolden() since FreeType 2.1.10.
       *      unfortunately, this function return no informations
       *      about the cause of error.
       */
      if ( ( ystr >> 6 ) > FT_INT_MAX || ( ystr >> 6 ) < FT_INT_MIN )
      {
        FT_TRACE1(( "FT_GlyphSlot_Embolden:" ));
        FT_TRACE1(( "too strong emboldening parameter ystr=%d\n", ystr ));
        return;
      }
      error = FT_GlyphSlot_Own_Bitmap( slot );
      if ( error )
        return;

      error = FT_Bitmap_Embolden( library, &slot->bitmap, xstr, ystr );
      if ( error )
        return;
    }

    if ( slot->advance.x )
      slot->advance.x += xstr;

    if ( slot->advance.y )
      slot->advance.y += ystr;

    slot->metrics.width        += xstr;
    slot->metrics.height       += ystr;
    slot->metrics.horiAdvance  += xstr;
    slot->metrics.vertAdvance  += ystr;
    slot->metrics.horiBearingY += ystr;

    /* XXX: 16-bit overflow case must be excluded before here */
    if ( slot->format == FT_GLYPH_FORMAT_BITMAP )
      slot->bitmap_top += (FT_Int)( ystr >> 6 );
  }

  /* Enlarge `bitmap' horizontally and vertically by `xpixels' */
  /* and `ypixels', respectively.                              */

  static FT_Error
  ft_bitmap_assure_buffer( FT_Memory   memory,
                              FT_Bitmap*  bitmap,
                              FT_UInt     xpixels,
                              FT_UInt     ypixels,
                              FT_Bool     copysrc)
  {
    FT_Error        error;
    unsigned int    pitch;
    unsigned int    new_pitch;
    FT_UInt         bpp;
    FT_UInt         width, height;
    unsigned char*  buffer = NULL;


    width  = bitmap->width;
    height = bitmap->rows;
    pitch  = (unsigned int)FT_ABS( bitmap->pitch );

    switch ( bitmap->pixel_mode )
    {
    case FT_PIXEL_MODE_MONO:
      bpp       = 1;
      new_pitch = ( width + xpixels + 7 ) >> 3;
      break;
    case FT_PIXEL_MODE_GRAY2:
      bpp       = 2;
      new_pitch = ( width + xpixels + 3 ) >> 2;
      break;
    case FT_PIXEL_MODE_GRAY4:
      bpp       = 4;
      new_pitch = ( width + xpixels + 1 ) >> 1;
      break;
    case FT_PIXEL_MODE_GRAY:
    case FT_PIXEL_MODE_LCD:
    case FT_PIXEL_MODE_LCD_V:
      bpp       = 8;
      new_pitch = width + xpixels;
      break;
    case FT_PIXEL_MODE_BGRA:
      bpp       = 32;
      new_pitch = (width + xpixels) * 4;
      break;
    default:
      return FT_THROW( Invalid_Glyph_Format );
    }

    /* if no need to allocate memory */
    if ( ypixels == 0 && new_pitch <= pitch )
    {
      /* zero the padding */
      FT_UInt  bit_width = pitch * 8;
      FT_UInt  bit_last  = ( width + xpixels ) * bpp;


      if ( bit_last < bit_width )
      {
        FT_Byte*  line  = bitmap->buffer + ( bit_last >> 3 );
        FT_Byte*  end   = bitmap->buffer + pitch;
        FT_UInt   shift = bit_last & 7;
        FT_UInt   mask  = 0xFF00U >> shift;
        FT_UInt   count = height;


        for ( ; count > 0; count--, line += pitch, end += pitch )
        {
          FT_Byte*  write = line;


          if ( shift > 0 )
          {
            write[0] = (FT_Byte)( write[0] & mask );
            write++;
          }
          if ( write < end )
            FT_MEM_ZERO( write, end - write );
        }
      }

      return FT_Err_Ok;
    }

    /* otherwise allocate new buffer */
    if ( FT_QALLOC_MULT( buffer, bitmap->rows + ypixels, new_pitch ) )
      return error;

    /* new rows get added at the top of the bitmap, */
    /* thus take care of the flow direction         */
    if ( bitmap->pitch > 0 )
    {
      FT_UInt  len = ( width * bpp + 7 ) >> 3;

      unsigned char*  in  = bitmap->buffer;
      unsigned char*  out = buffer;

      unsigned char*  limit = bitmap->buffer + pitch * bitmap->rows;
      unsigned int    delta = new_pitch - len;

      if (copysrc)
      {
        FT_MEM_ZERO( out, new_pitch * ypixels );
        out += new_pitch * ypixels;

        while ( in < limit )
        {
          FT_MEM_COPY( out, in, len );
          in  += pitch;
          out += len;

          /* we use FT_QALLOC_MULT, which doesn't zero out the buffer;      */
          /* consequently, we have to manually zero out the remaining bytes */
          FT_MEM_ZERO( out, delta );
          out += delta;
        }
      }
    }
    else
    {
      FT_UInt  len = ( width * bpp + 7 ) >> 3;

      unsigned char*  in  = bitmap->buffer;
      unsigned char*  out = buffer;

      unsigned char*  limit = bitmap->buffer + pitch * bitmap->rows;
      unsigned int    delta = new_pitch - len;

      if (copysrc)
      {
        while ( in < limit )
        {
          FT_MEM_COPY( out, in, len );
          in  += pitch;
          out += len;

          FT_MEM_ZERO( out, delta );
          out += delta;
        }

        FT_MEM_ZERO( out, new_pitch * ypixels );
      }
    }

    FT_FREE( bitmap->buffer );
    bitmap->buffer = buffer;

    /* set pitch only, width and height are left untouched */
    if ( bitmap->pitch < 0 )
      bitmap->pitch = -(int)new_pitch;
    else
      bitmap->pitch = (int)new_pitch;

    return FT_Err_Ok;
  }


  FT_EXPORT_DEF( FT_Error )
  FT_Bitmap_Colr_Embolden_Ex(FT_GlyphSlot slot, FT_Pos xStrength, FT_Pos yStrength)
  {
    FT_Error        error;
    FT_Int          xstr, ystr;
    FT_Library      library = slot->library;
    FT_Face         face    = slot->face;
    FT_LayerIterator  iterator;

    FT_UInt  base_glyph = slot->glyph_index;
    FT_Bool  have_layers;
    FT_UInt  glyph_index;
    FT_UInt  color_index;

    if ( !library )
      return FT_THROW( Invalid_Library_Handle );

    if ( ( ( FT_PIX_ROUND( xStrength ) >> 6 ) > FT_INT_MAX ) ||
         ( ( FT_PIX_ROUND( yStrength ) >> 6 ) > FT_INT_MAX ) )
      return FT_THROW( Invalid_Argument );

    xstr = (FT_Int)FT_PIX_ROUND( xStrength ) >> 6;
    ystr = (FT_Int)FT_PIX_ROUND( yStrength ) >> 6;
    if ( xstr == 0 && ystr == 0 )
      return FT_Err_Ok;
    else if ( xstr < 0 || ystr < 0 )
      return FT_THROW( Invalid_Argument );

    iterator.p  = NULL;
    have_layers = FT_Get_Color_Glyph_Layer( face,
                                            base_glyph,
                                            &glyph_index,
                                            &color_index,
                                            &iterator );
    if (have_layers)
    {
      FT_Bitmap* bitmap = &slot->bitmap;
      error = ft_bitmap_assure_buffer( library->memory, bitmap,
                                        (FT_UInt)xstr, (FT_UInt)ystr, 0);
      if ( error )
        return error;

      bitmap->width += (FT_UInt)xstr;
      bitmap->rows += (FT_UInt)ystr;
      FT_MEM_ZERO( slot->bitmap.buffer, bitmap->rows * (unsigned int)bitmap->pitch );

      error = FT_New_GlyphSlot( face, NULL );
      if ( !error )
      {
        TT_Face      ttface = (TT_Face)face;
        SFNT_Service sfnt   = (SFNT_Service)ttface->sfnt;

        do
        {
          FT_Int32  load_flags = slot->internal->load_flags;
          load_flags &= ~FT_LOAD_COLOR;
          load_flags &= ~FT_LOAD_NO_HINTING;
          load_flags |= FT_LOAD_RENDER;

          error = FT_Load_Glyph( face, glyph_index, load_flags );
          if ( error )
            break;

          error = FT_Bitmap_Embolden( library, &face->glyph->bitmap, xStrength, yStrength );
          if ( error )
            break;

          error = sfnt->colr_blend( ttface,
                                    color_index,
                                    slot,
                                    face->glyph );
          if ( error )
            break;

        } while ( FT_Get_Color_Glyph_Layer( face,
                                            base_glyph,
                                            &glyph_index,
                                            &color_index,
                                            &iterator ) );

        FT_Done_GlyphSlot( face->glyph );
      }
    }

    return FT_Err_Ok;
  }

  FT_EXPORT_DEF( void )
  FT_GlyphSlot_Embolden_Ex( FT_GlyphSlot  slot )
  {
    FT_Library  library;
    FT_Face     face;
    FT_Error    error;
    FT_Pos      xstr, ystr;

    if ( !slot )
      return;

    library = slot->library;
    face    = slot->face;

    if ( slot->format != FT_GLYPH_FORMAT_OUTLINE &&
         slot->format != FT_GLYPH_FORMAT_BITMAP  )
      return;

    /* some reasonable strength */
    xstr = FT_MulFix( face->units_per_EM,
                      face->size->metrics.y_scale ) / 48;
    ystr = xstr;

    if ( slot->format == FT_GLYPH_FORMAT_OUTLINE )
    {
      FT_Outline_EmboldenXY( &slot->outline, xstr, ystr );
    }
    else /* slot->format == FT_GLYPH_FORMAT_BITMAP */
    {
      /* round to full pixels */
      xstr &= ~63;
      if ( xstr == 0 )
        xstr = 1 << 6;
      ystr &= ~63;
      /*
       * XXX: overflow check for 16-bit system, for compatibility
       *      with FT_GlyphSlot_Embolden() since FreeType 2.1.10.
       *      unfortunately, this function return no informations
       *      about the cause of error.
       */
      if ( ( ystr >> 6 ) > FT_INT_MAX || ( ystr >> 6 ) < FT_INT_MIN )
      {
        FT_TRACE1(( "FT_GlyphSlot_Embolden:" ));
        FT_TRACE1(( "too strong emboldening parameter ystr=%d\n", ystr ));
        return;
      }
      error = FT_GlyphSlot_Own_Bitmap( slot );
      if ( error )
        return;

      if (slot->bitmap.pixel_mode == FT_PIXEL_MODE_BGRA)
        error = FT_Bitmap_Colr_Embolden_Ex(slot, xstr, ystr);
      else
        error = FT_Bitmap_Embolden( library, &slot->bitmap, xstr, ystr );
      if ( error )
        return;
    }

    if ( slot->advance.x )
      slot->advance.x += xstr;

    if ( slot->advance.y )
      slot->advance.y += ystr;

    slot->metrics.width        += xstr;
    slot->metrics.height       += ystr;
    slot->metrics.horiAdvance  += xstr;
    slot->metrics.vertAdvance  += ystr;
    slot->metrics.horiBearingY += ystr;

    /* XXX: 16-bit overflow case must be excluded before here */
    slot->bitmap_top += (FT_Int)( ystr >> 6 );
    if ( slot->format != FT_GLYPH_FORMAT_BITMAP )
    {
      slot->bitmap.width += (FT_UInt)( xstr >> 6 );
      slot->bitmap.rows += (FT_UInt)( ystr >> 6 );
    }
  }
/* END */
