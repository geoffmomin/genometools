/*
  Copyright (c) 2007 Stefan Kurtz <kurtz@zbh.uni-hamburg.de>
  Copyright (c) 2007 Center for Bioinformatics, University of Hamburg
  See LICENSE file or http://genometools.org/license.html for license details.
*/

#include <ctype.h>
#include <stdbool.h>
#include "libgtcore/env.h"
#include "libgtcore/str.h"
#include "types.h"
#include "fbs-def.h"
#include "stamp.h"

#include "fbsadv.pr"

#include "readnextUchar.gen"

int guessifproteinsequencestream(const StrArray *filenametab,Env *env)
{
  Seqpos countnonbases = 0,
         currentposition;
  Uchar currentchar;
  Fastabufferstate fbs;
  int retval;

  initformatbufferstate(&fbs,
                        filenametab,
                        NULL,
                        false,
                        NULL,
                        env);
  for (currentposition = 0; currentposition < (Seqpos) 1000; currentposition++)
  {
    retval = readnextUchar(&currentchar,&fbs,env);
    if (retval < 0)
    {
      return -1;
    }
    if (retval == 0)
    {
      break;
    }
    switch (currentchar)
    {
      case 'L':
      case 'I':
      case 'F':
      case 'E':
      case 'Q':
      case 'P':
      case 'X':
      case 'Z': countnonbases++;
                break;
      default:  break;
    }
  }
  if (countnonbases > 0 && countnonbases >= currentposition/10)
  {
    return 1;
  }
  return 0;
}
