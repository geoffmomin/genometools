/*
  Copyright (c) 2009 Stefan Kurtz <kurtz@zbh.uni-hamburg.de>
  Copyright (c) 2009 Center for Bioinformatics, University of Hamburg

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

#include "extended/alignment.h"
#include "encseq-def.h"
#include "spacedef.h"
#include "format64.h"
#include "idxlocalisw.h"

#define REPLACEMENTBIT   ((Uchar) 1)
#define DELETIONBIT      (((Uchar) 1) << 1)
#define INSERTIONBIT     (((Uchar) 1) << 2)

typedef struct
{
  unsigned long umax,
                vmax;
} Maxscorecoord;

typedef Uchar Retracebits;

static Scoretype swlocalsimilarityscore(Scoretype *scol,
                                        Maxscorecoord *maxpair,
                                        const Scorevalues *scorevalues,
                                        const Uchar *useq,
                                        unsigned long ulen,
                                        const Encodedsequence *vencseq,
                                        Seqpos startpos,
                                        Seqpos endpos)
{
  Scoretype val, we, nw, *scolptr, maximalscore = 0;
  const Uchar *uptr;
  Uchar vcurrent;
  Seqpos j;

  maxpair->umax = maxpair->vmax = 0;
  for (scolptr = scol; scolptr <= scol + ulen; scolptr++)
  {
    *scolptr = 0;
  }
  for (j = startpos; j < endpos; j++)
  {
    nw = 0;
    vcurrent = getencodedchar(vencseq,j,Forwardmode);
    gt_assert(vcurrent != (Uchar) SEPARATOR);
    for (scolptr = scol+1, uptr = useq; uptr < useq + ulen; scolptr++, uptr++)
    {
      gt_assert(*uptr != (Uchar) SEPARATOR);
      we = *scolptr;
      *scolptr = *(scolptr-1) + scorevalues->gapextend;
      if ((val = nw + REPLACEMENTSCORE(scorevalues,*uptr,vcurrent)) > *scolptr)
      {
        *scolptr = val;
      }
      if ((val = we + scorevalues->gapextend) > *scolptr)
      {
        *scolptr = val;
      }
      if (*scolptr < 0)
      {
        *scolptr = 0;
      } else
      {
        if (*scolptr > maximalscore)
        {
          maximalscore = *scolptr;
          maxpair->umax = (unsigned long) (uptr - useq + 1);
          maxpair->vmax = (unsigned long) (j - startpos + 1);
        }
      }
      nw = we;
    }
  }
  return maximalscore;
}

typedef struct
{
  Scoretype similarity;
  unsigned long lu;
  Seqpos lv;
} DPpoint;

typedef struct
{
  unsigned long len1,
                start1;
  Seqpos start2, len2;
  Scoretype similarity;
} DPregion;

static void swlocalsimilarityregion(DPpoint *scol,
                                    DPregion *maxentry,
                                    const Scorevalues *scorevalues,
                                    const Uchar *useq,
                                    unsigned long ulen,
                                    const Encodedsequence *vencseq,
                                    Seqpos startpos,
                                    Seqpos endpos)
{
  Scoretype val;
  DPpoint *scolptr, we, nw;
  const Uchar *uptr;
  Uchar vcurrent;
  Seqpos j;

  maxentry->similarity = 0;
  maxentry->len1 = 0;
  maxentry->len2 = 0;
  maxentry->start1 = 0;
  maxentry->start2 = 0;
  for (scolptr = scol; scolptr <= scol + ulen; scolptr++)
  {
    scolptr->similarity = 0;
    scolptr->lu = 0;
    scolptr->lv = 0;
  }
  for (j = startpos; j < endpos; j++)
  {
    vcurrent = getencodedchar(vencseq,j,Forwardmode);
    gt_assert(vcurrent != (Uchar) SEPARATOR);
    nw = *scol;
    for (scolptr = scol+1, uptr = useq; uptr < useq + ulen; scolptr++, uptr++)
    {
      gt_assert(*uptr != (Uchar) SEPARATOR);
      we = *scolptr;
      scolptr->similarity = (scolptr-1)->similarity + scorevalues->gapextend;
      scolptr->lu = (scolptr-1)->lu + 1;
      scolptr->lv = (scolptr-1)->lv;
      if ((val = nw.similarity + REPLACEMENTSCORE(scorevalues,*uptr,vcurrent))
               > scolptr->similarity)
      {
        scolptr->similarity = val;
        scolptr->lu = nw.lu + 1;
        scolptr->lv = nw.lv + 1;
      }
      if ((val = we.similarity + scorevalues->gapextend)
               > scolptr->similarity)
      {
        scolptr->similarity = val;
        scolptr->lu = we.lu;
        scolptr->lv = we.lv + 1;
      }
      if (scolptr->similarity < 0)
      {
        scolptr->similarity = 0;
        scolptr->lu = 0;
        scolptr->lv = 0;
      } else
      {
        if (scolptr->similarity > maxentry->similarity)
        {
          maxentry->similarity = scolptr->similarity;
          maxentry->len1 = scolptr->lu;
          maxentry->len2 = scolptr->lv;
          maxentry->start1 = (unsigned long) (uptr - useq) - scolptr->lu + 1;
          maxentry->start2 = (j - startpos) - scolptr->lv + 1;
        }
      }
      nw = we;
    }
  }
}

static void swmaximalDPedges(Retracebits *edges,
                             Scoretype *scol,
                             const Scorevalues *scorevalues,
                             const Uchar *useq,unsigned long ulen,
                             const Encodedsequence *vencseq,
                             Seqpos startpos,
                             Seqpos endpos)
{
  Scoretype val, we, nw, *scolptr;
  const Uchar *uptr;
  Uchar vcurrent;
  Seqpos j;
  Retracebits *eptr;

  eptr = edges;
  *eptr = 0;
  for (*scol = 0, scolptr = scol+1, uptr = useq, eptr++; uptr < useq + ulen;
       scolptr++, uptr++, eptr++)
  {
    *scolptr = *(scolptr-1) + scorevalues->gapextend;
    *eptr = DELETIONBIT;
  }
  for (j = startpos; j < endpos; j++)
  {
    vcurrent = getencodedchar(vencseq,j,Forwardmode);
    gt_assert(vcurrent != (Uchar) SEPARATOR);
    nw = *scol;
    *scol = nw + scorevalues->gapextend;
    *eptr = INSERTIONBIT;
    for (scolptr = scol+1, uptr = useq, eptr++; uptr < useq + ulen;
         scolptr++, uptr++, eptr++)
    {
      gt_assert(*uptr != (Uchar) SEPARATOR);
      we = *scolptr;
      *scolptr = *(scolptr-1) + scorevalues->gapextend;
      *eptr = DELETIONBIT;
      if ((val = nw + REPLACEMENTSCORE(scorevalues,*uptr,vcurrent))
               >= *scolptr)
      {
        if (val == *scolptr)
        {
          *eptr = *eptr | REPLACEMENTBIT;
        } else
        {
          *eptr = REPLACEMENTBIT;
        }
        *scolptr = val;
      }
      if ((val = we + scorevalues->gapextend) >= *scolptr)
      {
        if (val == *scolptr)
        {
          *eptr = *eptr | INSERTIONBIT;
        } else
        {
          *eptr = INSERTIONBIT;
        }
        *scolptr = val;
      }
      nw = we;
    }
  }
}

static void swtracebackDPedges(GtAlignment *alignment,unsigned long ulen,
                               Seqpos vlen,const Retracebits *edges)
{
  const Retracebits *eptr = edges + (ulen+1) * (vlen+1) - 1;

  while (true)
  {
    if (*eptr & DELETIONBIT)
    {
      gt_alignment_add_deletion(alignment);
      eptr--;
    } else
    {
      if (*eptr & REPLACEMENTBIT)
      {
        gt_alignment_add_replacement(alignment);
        eptr -= (ulen+2);
      } else
      {
        if (*eptr & INSERTIONBIT)
        {
          gt_alignment_add_insertion(alignment);
          eptr -= (ulen+1);
        } else
        {
          break;
        }
      }
    }
  }
}

static void swproducealignment(GtAlignment *alignment,
                               Retracebits *edges,Scoretype *scol,
                               const Scorevalues *scorevalues,
                               const Uchar *useq,unsigned long ulen,
                               const Encodedsequence *vencseq,
                               Seqpos startpos,
                               Seqpos endpos)
{
  swmaximalDPedges(edges,scol,scorevalues,useq,ulen,vencseq,startpos,endpos);
  swtracebackDPedges(alignment,ulen,endpos - startpos,edges);
}

struct SWdpresource
{
  bool showalignment;
  GtAlignment *alignment;
  const Scorevalues *scorevalues;
  Scoretype *swcol, scorethreshold;
  DPpoint *swentrycol;
  unsigned long allocatedswcol, allocatedmaxedges;
  Retracebits *maxedges;
};

static void applysmithwaterman(SWdpresource *dpresource,
                               const Encodedsequence *encseq,
                               unsigned long encsequnit,
                               Seqpos startpos,
                               Seqpos endpos,
                               uint64_t queryunit,
                               const Uchar *query,
                               unsigned long querylen)
{
  Scoretype score;
  Maxscorecoord maxpair;
  DPregion maxentry;

  if (dpresource->allocatedswcol < querylen + 1)
  {
    dpresource->allocatedswcol = querylen + 1;
    ALLOCASSIGNSPACE(dpresource->swcol,dpresource->swcol,Scoretype,
                     dpresource->allocatedswcol);
    ALLOCASSIGNSPACE(dpresource->swentrycol,dpresource->swentrycol,DPpoint,
                     dpresource->allocatedswcol);
  }
  score = swlocalsimilarityscore(dpresource->swcol,&maxpair,
                                 dpresource->scorevalues,
                                 query,querylen,encseq,startpos,endpos);
  if (score >= dpresource->scorethreshold)
  {
    swlocalsimilarityregion(dpresource->swentrycol,
                            &maxentry,
                            dpresource->scorevalues,
                            query,maxpair.umax,
                            encseq,startpos,startpos + maxpair.vmax);
    printf("%lu\t" FormatSeqpos "\t" FormatSeqpos "\t" Formatuint64_t
           "\t%lu%lu\t%ld\n",
            encsequnit,
            PRINTSeqposcast(maxentry.start2),
            PRINTSeqposcast(maxentry.len2),
            PRINTuint64_tcast(queryunit),
            maxentry.start1,
            maxentry.len1,
            maxentry.similarity);
    if (dpresource->showalignment)
    {
      if (dpresource->allocatedmaxedges
          < (querylen + 1) * (endpos - startpos + 1))
      {
        dpresource->allocatedmaxedges
          = (querylen + 1) * (endpos - startpos + 1);
        ALLOCASSIGNSPACE(dpresource->maxedges,dpresource->maxedges,Retracebits,
                         dpresource->allocatedmaxedges);
      }
      gt_alignment_reset(dpresource->alignment);
      swproducealignment(dpresource->alignment,
                         dpresource->maxedges,
                         dpresource->swcol,
                         dpresource->scorevalues,
                         query + maxentry.start1,maxentry.len1,
                         encseq, maxentry.start2,
                         maxentry.start2 + maxentry.len2);
    }
  }
}

void multiapplysmithwaterman(SWdpresource *dpresource,
                             const Encodedsequence *encseq,
                             uint64_t queryunit,
                             const Uchar *query,
                             unsigned long querylen)
{
  Seqinfo seqinfo;
  unsigned long seqnum, numofdbsequences = getencseqnumofdbsequences(encseq);

  for (seqnum = 0; seqnum < numofdbsequences; seqnum++)
  {
    getencseqSeqinfo(&seqinfo,encseq,seqnum);
    applysmithwaterman(dpresource,
                       encseq,
                       seqnum,
                       seqinfo.seqstartpos,
                       seqinfo.seqstartpos + seqinfo.seqlength,
                       queryunit,
                       query,
                       querylen);
  }
}

SWdpresource *newSWdpresource(const Scorevalues *scorevalues,
                              Scoretype scorethreshold,
                              bool showalignment)
{
  SWdpresource *swdpresource;

  ALLOCASSIGNSPACE(swdpresource,NULL,SWdpresource,1);
  swdpresource->showalignment = showalignment;
  swdpresource->scorevalues = scorevalues;
  swdpresource->scorethreshold = scorethreshold;
  swdpresource->alignment = gt_alignment_new();
  swdpresource->swcol = NULL;
  swdpresource->swentrycol = NULL;
  swdpresource->maxedges = NULL;
  swdpresource->allocatedswcol = 0;
  swdpresource->allocatedmaxedges = 0;
  return swdpresource;
}

void freeSWdpresource(SWdpresource *swdpresource)
{
  gt_alignment_delete(swdpresource->alignment);
  swdpresource->alignment = NULL;
  FREESPACE(swdpresource->swcol);
  FREESPACE(swdpresource->swentrycol);
  FREESPACE(swdpresource->maxedges);
  swdpresource->allocatedswcol = 0;
  swdpresource->allocatedmaxedges = 0;
}
