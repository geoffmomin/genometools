/*
  Copyright (c) 2007-2008 Sascha Steinbiss <ssteinbiss@stud.zbh.uni-hamburg.de>
  Copyright (c)      2008 Gordon Gremme <gremme@zbh.uni-hamburg.de>
  Copyright (c) 2007-2008 Center for Bioinformatics, University of Hamburg

  Permission to use, copy, modify, and distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#ifndef STYLE_H
#define STYLE_H

#include "lua.h"
#include "annotationsketch/style_api.h"
#include "extended/genome_node.h"

/* Creates a GtStyle object wich reuses the given Lua state. */
GtStyle*       gt_style_new_with_state(lua_State*);
/* Retrieves a color value from the GtFeatureNode for <key>.
   If not set, false is returned and a default color is written. */
bool           gt_style_get_color(const GtStyle*, const char *section,
                                  const char *key, GtColor*, GtFeatureNode*);
/* Retrieve string value of <key> in <section>.
   If not set, false is returned. */
bool           gt_style_get_str(const GtStyle*, const char *section,
                                const char *key, GtStr*, GtFeatureNode*);
/* Retrieve numeric value of <key> in <section>.
   If not set, false is returned.*/
bool           gt_style_get_num(const GtStyle*, const char *section,
                                const char *key, double*, GtFeatureNode*);
/* Retrieve boolean value of <key> in <section>.
   If not set, false is returned.*/
bool           gt_style_get_bool(const GtStyle*, const char *section,
                                 const char *key, bool*, GtFeatureNode*);
int            gt_style_unit_test(GtError*);
/* Deletes a GT_Style object but leaves the internal Lua state intact. */
void           gt_style_delete_without_state(GtStyle*);

#endif
