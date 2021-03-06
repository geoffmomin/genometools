/*
  Copyright (c) 2006-2011 Gordon Gremme <gordon@gremme.org>
  Copyright (c) 2006-2008 Center for Bioinformatics, University of Hamburg

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

#ifndef MERGE_FEATURE_STREAM_API_H
#define MERGE_FEATURE_STREAM_API_H

#include <stdio.h>
#include "extended/node_stream_api.h"

/* Implements the <GtNodeStream> interface. A <GtMergeFeatureStream> merges
   adjacent features of the same type. */
typedef struct GtMergeFeatureStream GtMergeFeatureStream;

/* Create a <GtMergeFeatureStream*> which merges adjacent features of the same
   type it retrieves from <in_stream> and returns them (and all other unmodified
   features). */
GtNodeStream* gt_merge_feature_stream_new(GtNodeStream *in_stream);

#endif
