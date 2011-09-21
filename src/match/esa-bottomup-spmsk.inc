/*
  Copyright (c) 2011 Stefan Kurtz <kurtz@zbh.uni-hamburg.de>
  Copyright (c) 2011 Center for Bioinformatics, University of Hamburg

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

#include <limits.h>
#include "core/ma.h"
#include "esa-seqread.h"

static void initBUinfo_spmsk(GtBUinfo_spmsk *,GtBUstate_spmsk *);

static void freeBUinfo_spmsk(GtBUinfo_spmsk *,GtBUstate_spmsk *);

static int processleafedge_spmsk(bool,
    unsigned long,
    unsigned long,
    GtBUinfo_spmsk *,
    unsigned long,
    GtBUstate_spmsk *,
    GtError *err);

#ifndef GT_ESA_BOTTOM_UP_IGNORE_PROCESSBRANCHING_EDGE
static int processbranchingedge_spmsk(bool firstsucc,
    unsigned long,
    unsigned long,
    GtBUinfo_spmsk *,
    unsigned long,
    unsigned long,
    unsigned long,
    GtBUinfo_spmsk *,
    GtBUstate_spmsk *,
    GtError *);
#endif

static int processlcpinterval_spmsk(unsigned long,
    unsigned long,
    unsigned long,
    GtBUinfo_spmsk *,
    GtBUstate_spmsk *,
    GtError *err);

#define TOP_ESA_BOTTOMUP_spmsk\
        stack->spaceGtBUItvinfo[stack->nextfreeGtBUItvinfo-1]
#define POP_ESA_BOTTOMUP_spmsk\
        (stack->spaceGtBUItvinfo + (--stack->nextfreeGtBUItvinfo))

#define PUSH_ESA_BOTTOMUP_spmsk(LCP,LB)\
        if (stack->nextfreeGtBUItvinfo >= stack->allocatedGtBUItvinfo)\
        {\
          gt_assert(stack->nextfreeGtBUItvinfo == stack->allocatedGtBUItvinfo);\
          stack->spaceGtBUItvinfo\
            = allocateBUstack_spmsk(stack->spaceGtBUItvinfo,\
                              stack->allocatedGtBUItvinfo,\
                              stack->allocatedGtBUItvinfo+incrementstacksize,\
                              bustate);\
          stack->allocatedGtBUItvinfo += incrementstacksize;\
        }\
        gt_assert(stack->spaceGtBUItvinfo != NULL);\
        stack->spaceGtBUItvinfo[stack->nextfreeGtBUItvinfo].lcp = LCP;\
        stack->spaceGtBUItvinfo[stack->nextfreeGtBUItvinfo].lb = LB;\
        stack->spaceGtBUItvinfo[stack->nextfreeGtBUItvinfo].rb = ULONG_MAX;\
        stack->nextfreeGtBUItvinfo++

typedef struct
{
  unsigned long lcp, lb, rb;
  GtBUinfo_spmsk info;
} GtBUItvinfo_spmsk;

typedef struct
{
  GtBUItvinfo_spmsk *spaceGtBUItvinfo;
  unsigned long allocatedGtBUItvinfo,
                nextfreeGtBUItvinfo;
} GtArrayGtBUItvinfo_spmsk;

GtArrayGtBUItvinfo_spmsk *gt_GtArrayGtBUItvinfo_new_spmsk(void)
{
  GtArrayGtBUItvinfo_spmsk *stack = gt_malloc(sizeof (*stack));

  stack->spaceGtBUItvinfo = NULL;
  stack->allocatedGtBUItvinfo = stack->nextfreeGtBUItvinfo = 0;
  return stack;
}

void gt_GtArrayGtBUItvinfo_delete_spmsk(GtArrayGtBUItvinfo_spmsk *stack,
                                  GtBUstate_spmsk *state)
{
  unsigned long idx;

  for (idx=0; idx<stack->allocatedGtBUItvinfo; idx++)
  {
    freeBUinfo_spmsk(&stack->spaceGtBUItvinfo[idx].info,state);
  }
  gt_free(stack->spaceGtBUItvinfo);
  gt_free(stack);
}

static GtBUItvinfo_spmsk *allocateBUstack_spmsk(GtBUItvinfo_spmsk *ptr,
                                   unsigned long currentallocated,
                                   unsigned long allocated,
                                   GtBUstate_spmsk *state)
{
  unsigned long idx;
  GtBUItvinfo_spmsk *itvinfo;

  itvinfo = gt_realloc(ptr,sizeof(*itvinfo) * allocated);
  gt_assert(allocated > currentallocated);
  for (idx=currentallocated; idx<allocated; idx++)
  {
    initBUinfo_spmsk(&itvinfo[idx].info,state);
  }
  gt_assert(itvinfo != NULL);
  return itvinfo;
}

#ifndef GT_ESA_BOTTOM_UP_RAM

int gt_esa_bottomup_spmsk(Sequentialsuffixarrayreader *ssar,
                    GtBUstate_spmsk *bustate,
                    GtError *err)
{
  const unsigned long incrementstacksize = 32UL;
  unsigned long lcpvalue,
                previoussuffix = 0,
                idx,
                nonspecials,
                lastsuftabvalue = 0;
  GtBUItvinfo_spmsk *lastinterval = NULL;
  bool haserr = false, firstedge, firstedgefromroot = true;
  GtArrayGtBUItvinfo_spmsk *stack;

  stack = gt_GtArrayGtBUItvinfo_new_spmsk();
  PUSH_ESA_BOTTOMUP_spmsk(0,0);
  nonspecials = gt_Sequentialsuffixarrayreader_nonspecials(ssar);
  for (idx = 0; idx < nonspecials; idx++)
  {
    NEXTSEQUENTIALLCPTABVALUEWITHLAST(lcpvalue,lastsuftabvalue,ssar);
    NEXTSEQUENTIALSUFTABVALUE(previoussuffix,ssar);
    if (lcpvalue <= TOP_ESA_BOTTOMUP_spmsk.lcp)
    {
      if (TOP_ESA_BOTTOMUP_spmsk.lcp > 0 || !firstedgefromroot)
      {
        firstedge = false;
      } else
      {
        firstedge = true;
        firstedgefromroot = false;
      }
      if (processleafedge_spmsk(firstedge,
                          TOP_ESA_BOTTOMUP_spmsk.lcp,
                          TOP_ESA_BOTTOMUP_spmsk.lb,
                          &TOP_ESA_BOTTOMUP_spmsk.info,
                          previoussuffix,bustate,err) != 0)
      {
        haserr = true;
        break;
      }
    }
    gt_assert(lastinterval == NULL);
    while (lcpvalue < TOP_ESA_BOTTOMUP_spmsk.lcp)
    {
      lastinterval = POP_ESA_BOTTOMUP_spmsk;
      lastinterval->rb = idx;
      if (processlcpinterval_spmsk != NULL &&
          processlcpinterval_spmsk(lastinterval->lcp,
                             lastinterval->lb,
                             lastinterval->rb,
                             &lastinterval->info,
                             bustate,
                             err) != 0)
      {
        haserr = true;
        break;
      }
      if (lcpvalue <= TOP_ESA_BOTTOMUP_spmsk.lcp)
      {
        if (TOP_ESA_BOTTOMUP_spmsk.lcp > 0 || !firstedgefromroot)
        {
          firstedge = false;
        } else
        {
          firstedge = true;
          firstedgefromroot = false;
        }
#ifndef GT_ESA_BOTTOM_UP_IGNORE_PROCESSBRANCHING_EDGE
        if (processbranchingedge_spmsk != NULL &&
            processbranchingedge_spmsk(firstedge,
                                 TOP_ESA_BOTTOMUP_spmsk.lcp,
                                 TOP_ESA_BOTTOMUP_spmsk.lb,
                                 &TOP_ESA_BOTTOMUP_spmsk.info,
                                 lastinterval->lcp,
                                 lastinterval->lb,
                                 lastinterval->rb,
                                 &lastinterval->info,
                                 bustate,
                                 err) != 0)
        {
          haserr = true;
          break;
        }
#endif
        lastinterval = NULL;
      }
    }
    if (haserr)
    {
      break;
    }
    if (lcpvalue > TOP_ESA_BOTTOMUP_spmsk.lcp)
    {
      if (lastinterval != NULL)
      {
        unsigned long lastintervallb = lastinterval->lb;
#ifndef GT_ESA_BOTTOM_UP_IGNORE_PROCESSBRANCHING_EDGE
        unsigned long lastintervallcp = lastinterval->lcp,
                      lastintervalrb = lastinterval->rb;
#endif
        PUSH_ESA_BOTTOMUP_spmsk(lcpvalue,lastintervallb);
#ifndef GT_ESA_BOTTOM_UP_IGNORE_PROCESSBRANCHING_EDGE
        if (processbranchingedge_spmsk != NULL &&
            processbranchingedge_spmsk(true,
                                 TOP_ESA_BOTTOMUP_spmsk.lcp,
                                 TOP_ESA_BOTTOMUP_spmsk.lb,
                                 &TOP_ESA_BOTTOMUP_spmsk.info,
                                 lastintervallcp,
                                 lastintervallb,
                                 lastintervalrb,
                                 NULL,
                                 bustate,err) != 0)
        {
          haserr = true;
          break;
        }
#endif
        lastinterval = NULL;
      } else
      {
        PUSH_ESA_BOTTOMUP_spmsk(lcpvalue,idx);
        if (processleafedge_spmsk(true,
                            TOP_ESA_BOTTOMUP_spmsk.lcp,
                            TOP_ESA_BOTTOMUP_spmsk.lb,
                            &TOP_ESA_BOTTOMUP_spmsk.info,
                            previoussuffix,bustate,err) != 0)
        {
          haserr = true;
          break;
        }
      }
    }
  }
  gt_assert(stack->nextfreeGtBUItvinfo > 0);
  if (!haserr && TOP_ESA_BOTTOMUP_spmsk.lcp > 0)
  {
    if (processleafedge_spmsk(false,
                        TOP_ESA_BOTTOMUP_spmsk.lcp,
                        TOP_ESA_BOTTOMUP_spmsk.lb,
                        &TOP_ESA_BOTTOMUP_spmsk.info,
                        lastsuftabvalue,bustate,err) != 0)
    {
      haserr = true;
    } else
    {
      TOP_ESA_BOTTOMUP_spmsk.rb = idx;
      if (processlcpinterval_spmsk(TOP_ESA_BOTTOMUP_spmsk.lcp,
                             TOP_ESA_BOTTOMUP_spmsk.lb,
                             TOP_ESA_BOTTOMUP_spmsk.rb,
                             &TOP_ESA_BOTTOMUP_spmsk.info,
                             bustate,
                             err) != 0)
      {
        haserr = true;
      }
    }
  }
  gt_GtArrayGtBUItvinfo_delete_spmsk(stack,bustate);
  return haserr ? -1 : 0;
}

#else

int gt_esa_bottomup_RAM_spmsk(const unsigned long *suftab,
                        const uint16_t *lcptab_bucket,
                        unsigned long nonspecials,
                        GtArrayGtBUItvinfo_spmsk *stack,
                        GtBUstate_spmsk *bustate,
                        GtError *err)
{
  const unsigned long incrementstacksize = 32UL;
  unsigned long lcpvalue,
                previoussuffix = 0,
                idx;
  GtBUItvinfo_spmsk *lastinterval = NULL;
  bool haserr = false, firstedge, firstedgefromroot = true;

  gt_assert(nonspecials > 0);
  PUSH_ESA_BOTTOMUP_spmsk(0,0);
  for (idx = 0; idx < nonspecials-1; idx++)
  {
    lcpvalue = (unsigned long) lcptab_bucket[idx+1];
    previoussuffix = suftab[idx];
    if (lcpvalue <= TOP_ESA_BOTTOMUP_spmsk.lcp)
    {
      if (TOP_ESA_BOTTOMUP_spmsk.lcp > 0 || !firstedgefromroot)
      {
        firstedge = false;
      } else
      {
        firstedge = true;
        firstedgefromroot = false;
      }
      if (processleafedge_spmsk(firstedge,
                          TOP_ESA_BOTTOMUP_spmsk.lcp,
                          TOP_ESA_BOTTOMUP_spmsk.lb,
                          &TOP_ESA_BOTTOMUP_spmsk.info,
                          previoussuffix,bustate,err) != 0)
      {
        haserr = true;
        break;
      }
    }
    gt_assert(lastinterval == NULL);
    while (lcpvalue < TOP_ESA_BOTTOMUP_spmsk.lcp)
    {
      lastinterval = POP_ESA_BOTTOMUP_spmsk;
      lastinterval->rb = idx;
      if (processlcpinterval_spmsk != NULL &&
          processlcpinterval_spmsk(lastinterval->lcp,
                             lastinterval->lb,
                             lastinterval->rb,
                             &lastinterval->info,
                             bustate,
                             err) != 0)
      {
        haserr = true;
        break;
      }
      if (lcpvalue <= TOP_ESA_BOTTOMUP_spmsk.lcp)
      {
        if (TOP_ESA_BOTTOMUP_spmsk.lcp > 0 || !firstedgefromroot)
        {
          firstedge = false;
        } else
        {
          firstedge = true;
          firstedgefromroot = false;
        }
#ifndef GT_ESA_BOTTOM_UP_IGNORE_PROCESSBRANCHING_EDGE
        if (processbranchingedge_spmsk != NULL &&
            processbranchingedge_spmsk(firstedge,
                                 TOP_ESA_BOTTOMUP_spmsk.lcp,
                                 TOP_ESA_BOTTOMUP_spmsk.lb,
                                 &TOP_ESA_BOTTOMUP_spmsk.info,
                                 lastinterval->lcp,
                                 lastinterval->lb,
                                 lastinterval->rb,
                                 &lastinterval->info,
                                 bustate,
                                 err) != 0)
        {
          haserr = true;
          break;
        }
#endif
        lastinterval = NULL;
      }
    }
    if (haserr)
    {
      break;
    }
    if (lcpvalue > TOP_ESA_BOTTOMUP_spmsk.lcp)
    {
      if (lastinterval != NULL)
      {
        unsigned long lastintervallb = lastinterval->lb;
#ifndef GT_ESA_BOTTOM_UP_IGNORE_PROCESSBRANCHING_EDGE
        unsigned long lastintervallcp = lastinterval->lcp,
                      lastintervalrb = lastinterval->rb;
#endif
        PUSH_ESA_BOTTOMUP_spmsk(lcpvalue,lastintervallb);
#ifndef GT_ESA_BOTTOM_UP_IGNORE_PROCESSBRANCHING_EDGE
        if (processbranchingedge_spmsk != NULL &&
            processbranchingedge_spmsk(true,
                                 TOP_ESA_BOTTOMUP_spmsk.lcp,
                                 TOP_ESA_BOTTOMUP_spmsk.lb,
                                 &TOP_ESA_BOTTOMUP_spmsk.info,
                                 lastintervallcp,
                                 lastintervallb,
                                 lastintervalrb,
                                 NULL,
                                 bustate,err) != 0)
        {
          haserr = true;
          break;
        }
#endif
        lastinterval = NULL;
      } else
      {
        PUSH_ESA_BOTTOMUP_spmsk(lcpvalue,idx);
        if (processleafedge_spmsk(true,
                            TOP_ESA_BOTTOMUP_spmsk.lcp,
                            TOP_ESA_BOTTOMUP_spmsk.lb,
                            &TOP_ESA_BOTTOMUP_spmsk.info,
                            previoussuffix,bustate,err) != 0)
        {
          haserr = true;
          break;
        }
      }
    }
  }
  gt_assert(stack->nextfreeGtBUItvinfo > 0);
  if (!haserr && TOP_ESA_BOTTOMUP_spmsk.lcp > 0)
  {
    unsigned long lastsuftabvalue = suftab[nonspecials-1];
    if (processleafedge_spmsk(false,
                        TOP_ESA_BOTTOMUP_spmsk.lcp,
                        TOP_ESA_BOTTOMUP_spmsk.lb,
                        &TOP_ESA_BOTTOMUP_spmsk.info,
                        lastsuftabvalue,bustate,err) != 0)
    {
      haserr = true;
    } else
    {
      TOP_ESA_BOTTOMUP_spmsk.rb = idx;
      if (processlcpinterval_spmsk(TOP_ESA_BOTTOMUP_spmsk.lcp,
                             TOP_ESA_BOTTOMUP_spmsk.lb,
                             TOP_ESA_BOTTOMUP_spmsk.rb,
                             &TOP_ESA_BOTTOMUP_spmsk.info,
                             bustate,
                             err) != 0)
      {
        haserr = true;
      }
    }
  }
  stack->nextfreeGtBUItvinfo = 0; /* empty the stack */
  return haserr ? -1 : 0;
}

#endif