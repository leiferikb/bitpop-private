/**************************************************************************
 * 
 * Copyright 2003 Tungsten Graphics, Inc., Cedar Park, Texas.
 * All Rights Reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL TUNGSTEN GRAPHICS AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * 
 **************************************************************************/

#include "i915_context.h"
#include "i915_state.h"
#include "i915_screen.h"
#include "i915_surface.h"
#include "i915_batch.h"
#include "i915_resource.h"

#include "draw/draw_context.h"
#include "pipe/p_defines.h"
#include "util/u_inlines.h"
#include "util/u_memory.h"
#include "pipe/p_screen.h"


/*
 * Draw functions
 */


static void
i915_draw_vbo(struct pipe_context *pipe, const struct pipe_draw_info *info)
{
   struct i915_context *i915 = i915_context(pipe);
   struct draw_context *draw = i915->draw;
   void *mapped_indices = NULL;
   unsigned i;

   if (i915->dirty)
      i915_update_derived(i915);

   /*
    * Map vertex buffers
    */
   for (i = 0; i < i915->num_vertex_buffers; i++) {
      void *buf = i915_buffer(i915->vertex_buffer[i].buffer)->data;
      draw_set_mapped_vertex_buffer(draw, i, buf);
   }

   /*
    * Map index buffer, if present
    */
   if (info->indexed && i915->index_buffer.buffer)
      mapped_indices = i915_buffer(i915->index_buffer.buffer)->data;
   draw_set_mapped_index_buffer(draw, mapped_indices);

   draw_set_mapped_constant_buffer(draw, PIPE_SHADER_VERTEX, 0,
                                   i915->current.constants[PIPE_SHADER_VERTEX],
                                   (i915->current.num_user_constants[PIPE_SHADER_VERTEX] * 
                                      4 * sizeof(float)));

   /*
    * Do the drawing
    */
   draw_vbo(i915->draw, info);

   /*
    * unmap vertex/index buffers
    */
   for (i = 0; i < i915->num_vertex_buffers; i++) {
      draw_set_mapped_vertex_buffer(draw, i, NULL);
   }

   if (mapped_indices)
      draw_set_mapped_index_buffer(draw, NULL);
}


/*
 * Generic context functions
 */


static void i915_destroy(struct pipe_context *pipe)
{
   struct i915_context *i915 = i915_context(pipe);
   int i;

   draw_destroy(i915->draw);
   
   if(i915->batch)
      i915->iws->batchbuffer_destroy(i915->batch);

   /* unbind framebuffer */
   for (i = 0; i < PIPE_MAX_COLOR_BUFS; i++) {
      pipe_surface_reference(&i915->framebuffer.cbufs[i], NULL);
   }
   pipe_surface_reference(&i915->framebuffer.zsbuf, NULL);

   FREE(i915);
}

struct pipe_context *
i915_create_context(struct pipe_screen *screen, void *priv)
{
   struct i915_context *i915;

   i915 = CALLOC_STRUCT(i915_context);
   if (i915 == NULL)
      return NULL;

   i915->iws = i915_screen(screen)->iws;
   i915->base.winsys = NULL;
   i915->base.screen = screen;
   i915->base.priv = priv;

   i915->base.destroy = i915_destroy;

   i915->base.clear = i915_clear;

   i915->base.draw_vbo = i915_draw_vbo;

   /*
    * Create drawing context and plug our rendering stage into it.
    */
   i915->draw = draw_create(&i915->base);
   assert(i915->draw);
   if (!debug_get_bool_option("I915_NO_VBUF", FALSE)) {
      draw_set_rasterize_stage(i915->draw, i915_draw_vbuf_stage(i915));
   } else {
      draw_set_rasterize_stage(i915->draw, i915_draw_render_stage(i915));
   }

   i915_init_surface_functions(i915);
   i915_init_state_functions(i915);
   i915_init_flush_functions(i915);
   i915_init_resource_functions(i915);

   draw_install_aaline_stage(i915->draw, &i915->base);
   draw_install_aapoint_stage(i915->draw, &i915->base);

   i915->dirty = ~0;
   i915->hardware_dirty = ~0;

   /* Batch stream debugging is a bit hacked up at the moment:
    */
   i915->batch = i915->iws->batchbuffer_create(i915->iws);

   return &i915->base;
}
