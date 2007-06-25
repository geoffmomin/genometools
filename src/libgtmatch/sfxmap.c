/*
  Copyright (c) 2007 Stefan Kurtz <kurtz@zbh.uni-hamburg.de>
  Copyright (c) 2007 Center for Bioinformatics, University of Hamburg
  See LICENSE file or http://genometools.org/license.html for license details.
*/

#include <stdio.h>
#include <errno.h>
#include <stdbool.h>
#include "libgtcore/env.h"
#include "types.h"
#include "arraydef.h"
#include "sarr-def.h"
#include "encseq-def.h"

#include "readnextline.pr"
#include "endianess.pr"
#include "alphabet.pr"

#define DBFILEKEY "dbfile="

typedef struct
{
  const char *keystring;
  union 
  {
    unsigned long long *bigvalue;
    unsigned int *smallvalue;
  } value;
  size_t sizeval;
  bool found;
} Readintkeys;

DECLAREARRAYSTRUCT(Readintkeys);

static void setreadintkeys(ArrayReadintkeys *riktab,
                           const char *keystring,
                           void *valueptr,
                           size_t sizeval,
                           Env *env)
{
  Readintkeys *riktabptr;

  GETNEXTFREEINARRAY(riktabptr,riktab,Readintkeys,1);
  riktabptr->keystring = keystring;
  riktabptr->sizeval = sizeval;
  if(sizeval == (size_t) 4)
  {
    assert(sizeof(unsigned int) == 4);
    riktabptr->value.smallvalue = (unsigned int *) valueptr;
  } else
  {
    if(sizeval == (size_t) 8)
    {
      assert(sizeof(unsigned long long) == 8);
      riktabptr->value.bigvalue = (unsigned long long *) valueptr;
    } else
    {
      fprintf(stderr,"sizeval must be 4 or 8\n");
      exit(EXIT_FAILURE);
    }
  }
  riktabptr->found = false;
}

static int scanuintintline(unsigned int *lengthofkey,
                           void *value,
                           const ArrayUchar *linebuffer,
                           Env *env)
{
  Scaninteger readint;
  unsigned int i;
  bool found = false;

  env_error_check(env);
  for (i=0; i<(unsigned int) linebuffer->nextfreeUchar; i++)
  {
    if (linebuffer->spaceUchar[i] == '=')
    {
      *lengthofkey = i;
      found = true;
      if (sscanf((char *) (linebuffer->spaceUchar + i + 1),"%ld",
                &readint) != 1 ||
         readint < (Scaninteger) 0)
      {
        env_error_set(env,"cannot find non-negative integer in \"%*.*s\"",
                           (Fieldwidthtype) (linebuffer->nextfreeUchar - (i+1)),
                           (Fieldwidthtype) (linebuffer->nextfreeUchar - (i+1)),
                           (const char *) (linebuffer->spaceUchar + i + 1));
        return -1;
      }
      *value = (Uint) readint;
      break;
    }
  }
  if (!found)
  {
    env_error_set(env,"missing equality symbol in \"%*.*s\"",
                       (Fieldwidthtype) linebuffer->nextfreeUchar,
                       (Fieldwidthtype) linebuffer->nextfreeUchar,
                       (const char *) linebuffer->spaceUchar);
    return -2;
  }
  return 0;
}

static int allkeysdefined(const Str *prjfile,const ArrayReadintkeys *rik,
                          Env *env)
{
  Uint i;

  env_error_check(env);
  for (i=0; i<rik->nextfreeReadintkeys; i++)
  {
    if (!rik->spaceReadintkeys[i].found)
    {
      env_error_set(env,"file %s: missing line beginning with \"%s=\"",
                         str_get(prjfile),
                         rik->spaceReadintkeys[i].keystring);
      return -1;
    }
    if (rik->spaceReadintkeys[i].valueptr == NULL)
    {
      printf("%s=0\n",rik->spaceReadintkeys[i].keystring);
    } else
    {
      printf("%s=%lu\n",rik->spaceReadintkeys[i].keystring,
                        (Showuint) *(rik->spaceReadintkeys[i].valueptr));
    }
  }
  return 0;
}

#define SETREADINTKEYS(VALNAME,VAL)
        if((VAL) == NULL)\
        {\
          setreadintkeys(&rik,VALNAME,VAL,(size_t) 4,env);\
        } else\
        {\
          setreadintkeys(&rik,VALNAME,VAL,sizeof(*(VAL)),env);\
        }

static int scanprjfileviafileptr(Suffixarray *suffixarray,
                                 Seqpos *totallength,
                                 const Str *prjfile,FILE *fpin,Env *env)
{
  ArrayUchar linebuffer;
  Uint uintvalue, numofsequences, integersize, littleendian;
  unsigned int linenum, lengthofkey, i;
  unsigned long numoffiles = 0, numofallocatedfiles = 0;
  size_t dbfilelen = strlen(DBFILEKEY);
  int retval;
  bool found, haserr = false;
  ArrayReadintkeys rik;

  env_error_check(env);
  INITARRAY(&rik,Readintkeys);
  SETREADINTKEYS("totallength",totallength);
  SETREADINTKEYS("specialcharacters",
                 &suffixarray->specialcharinfo.specialcharacters);
  SETREADINTKEYS("specialranges",
                 &suffixarray->specialcharinfo.specialranges);
  SETREADINTKEYS("lengthofspecialprefix",
                 &suffixarray->specialcharinfo.lengthofspecialprefix);
  SETREADINTKEYS("lengthofspecialsuffix",
                 &suffixarray->specialcharinfo.lengthofspecialsuffix);
  SETREADINTKEYS("numofsequences",
                 &numofsequences);
  SETREADINTKEYS("numofdbsequences",
                 &suffixarray->numofdbsequences);
  SETREADINTKEYS("numofquerysequences",
                 NULL);
  SETREADINTKEYS("prefixlength",
                 &suffixarray->prefixlength);
  SETREADINTKEYS("integersize",
                 &integersize);
  SETREADINTKEYS("littleendian",
                 &littleendian);
  assert(rik.spaceReadintkeys != NULL);
  INITARRAY(&linebuffer,Uchar);
  suffixarray->filenametab = strarray_new(env);
  suffixarray->filelengthtab = NULL;
  for (linenum = 0; /* Nothing */; linenum++)
  {
    linebuffer.nextfreeUchar = 0;
    if (readnextline(fpin,&linebuffer,env) == EOF)
    {
      break;
    }
    if (dbfilelen <= (size_t) linebuffer.nextfreeUchar &&
       memcmp(DBFILEKEY,linebuffer.spaceUchar,dbfilelen) == 0)
    {
      char *tmpfilename;
      Scaninteger readint1, readint2;

      if (numoffiles >= numofallocatedfiles)
      {
        numofallocatedfiles += 2;
        ALLOCASSIGNSPACE(suffixarray->filelengthtab,suffixarray->filelengthtab,
                         PairUint,numofallocatedfiles);
      }
      assert(suffixarray->filelengthtab != NULL);
      ALLOCASSIGNSPACE(tmpfilename,NULL,char,linebuffer.nextfreeUchar);
      if (sscanf((const char *) linebuffer.spaceUchar,"dbfile=%s %ld %ld\n",
                   tmpfilename,
                   &readint1,
                   &readint2) != 3)
      {
        env_error_set(env,"cannot parse line %*.*s",
                          (int) linebuffer.nextfreeUchar,
                          (int) linebuffer.nextfreeUchar,
                          (const char *) linebuffer.spaceUchar);
        FREESPACE(tmpfilename);
        haserr = true;
        break;
      }
      if (readint1 < (Scaninteger) 1 || readint2 < (Scaninteger) 1)
      {
        env_error_set(env,"need positive integers in line %*.*s",
                          (int) linebuffer.nextfreeUchar,
                          (int) linebuffer.nextfreeUchar,
                          (const char *) linebuffer.spaceUchar);
        haserr = true;
      }
      if (!haserr)
      {
        strarray_add_cstr(suffixarray->filenametab,tmpfilename,env);
        FREESPACE(tmpfilename);
        suffixarray->filelengthtab[numoffiles].uint0 = (Uint) readint1;
        suffixarray->filelengthtab[numoffiles].uint1 = (Uint) readint2;
        printf("%s%s %lu %lu\n",
                DBFILEKEY,
                strarray_get(suffixarray->filenametab,numoffiles),
                (Showuint)
                suffixarray->filelengthtab[numoffiles].uint0,
                (Showuint)
                suffixarray->filelengthtab[numoffiles].uint1);
        numoffiles++;
      }
    } else
    {
      retval = scanuintintline(&lengthofkey,
                               &uintvalue,
                               &linebuffer,
                               env);
      if (retval < 0)
      {
        haserr = true;
        break;
      }
      found = false;
      for (i=0; i<rik.nextfreeReadintkeys; i++)
      {
        if (memcmp(linebuffer.spaceUchar,
                  rik.spaceReadintkeys[i].keystring,
                  (size_t) lengthofkey) == 0)
        {
          rik.spaceReadintkeys[i].found = true;
          if (rik.spaceReadintkeys[i].valueptr != NULL)
          {
            *(rik.spaceReadintkeys[i].valueptr) = uintvalue;
          }
          found = true;
          break;
        }
      }
      if (!found)
      {
        env_error_set(env,"file %s, line %u: cannot find key for \"%*.*s\"",
                           str_get(prjfile),
                           linenum,
                           (Fieldwidthtype) lengthofkey,
                           (Fieldwidthtype) lengthofkey,
                           (char  *) linebuffer.spaceUchar);
        haserr = true;
        break;
      }
    }
  }
  if (!haserr && allkeysdefined(prjfile,&rik,env) != 0)
  {
    haserr = true;
  }
  CHECKUintCast(prefixlength);
  suffixarray->prefixlength = (unsigned int) prefixlength;
  if (!haserr && integersize != UintConst(32) && integersize != UintConst(64))
  {
    env_error_set(env,"%s contains illegal line defining the integer size",
                  str_get(prjfile));
    haserr = true;
  }
  if (!haserr && integersize != (Uint) (sizeof (Uint) * CHAR_BIT))
  {
    env_error_set(env,"index was generated for %lu-bit integers while "
                      "this program uses %lu-bit integers",
                      (Showuint) integersize,
                      (Showuint) (sizeof (Uint) * CHAR_BIT));
    haserr = true;
  }
  if (!haserr)
  {
    if (islittleendian())
    {
      if (littleendian != UintConst(1))
      {
        env_error_set(env,"computer has little endian byte order, while index "
                          "was build on computer with big endian byte order");
        haserr = true;
      }
    } else
    {
      if (littleendian == UintConst(1))
      {
        env_error_set(env,"computer has big endian byte order, while index "
                          "was build on computer with little endian byte "
                          "order");
        haserr = true;
      }
    }
  }
  FREEARRAY(&linebuffer,Uchar);
  FREEARRAY(&rik,Readintkeys);
  return haserr ? -1 : 0;
}

int mapsuffixarray(Suffixarray *suffixarray,
                   bool withsuftab,
                   bool withencseq,
                   const Str *indexname,
                   Env *env)
{
  Str *tmpfilename;
  FILE *fp;
  bool haserr = false;
  Seqpos totallength;

  env_error_check(env);
  suffixarray->suftab = NULL;
  suffixarray->alpha = NULL;
  tmpfilename = str_clone(indexname,env);
  str_append_cstr(tmpfilename,".prj",env);
  fp = env_fa_fopen(env,str_get(tmpfilename),"rb");
  if (fp == NULL)
  {
    env_error_set(env,"cannot open file \"%s\": %s",str_get(tmpfilename),
                                                    strerror(errno));
    haserr = true;
  }
  if (!haserr && scanprjfileviafileptr(suffixarray,&totallength,
                                      tmpfilename,fp,env) != 0)
  {
    haserr = true;
  }
  env_fa_xfclose(fp,env);
  str_delete(tmpfilename,env);
  if (!haserr)
  {
    tmpfilename = str_clone(indexname,env);
    str_append_cstr(tmpfilename,".al1",env);
    suffixarray->alpha = assigninputalphabet(false,
                                             false,
                                             tmpfilename,
                                             NULL,
                                             env);
    if (suffixarray->alpha == NULL)
    {
      haserr = true;
    }
    str_delete(tmpfilename,env);
  }
  if (!haserr && withencseq)
  {
    suffixarray->encseq = initencodedseq(true,
					 NULL,
					 indexname,
					 totallength,
					 &suffixarray->specialcharinfo,
					 suffixarray->alpha,
                                         NULL,
					 env);
    if (suffixarray->encseq == NULL)
    {
      haserr = true;
    }
  }
  if (!haserr && withsuftab)
  {
    size_t numofbytes;

    tmpfilename = str_clone(indexname,env);
    str_append_cstr(tmpfilename,".suf",env);
    suffixarray->suftab = env_fa_mmap_read(env,str_get(tmpfilename),
                                           &numofbytes);
    if (suffixarray->suftab == NULL)
    {
      env_error_set(env,"cannot map file \"%s\": %s",str_get(tmpfilename),
                    strerror(errno));
      haserr = true;
    }
    if (!haserr && totallength + 1 != (Seqpos) (numofbytes/sizeof (Uint)))
    {
      env_error_set(env,"number of mapped integers = %lu != %lu = "
                        "expected number of integers",
                         (Showuint) (numofbytes/sizeof (Uint)),
                         (Showuint) (totallength + 1));
      env_fa_xmunmap((void *) suffixarray->suftab,env);
      haserr = true;
    }
    str_delete(tmpfilename,env);
  }
  return haserr ? -1 : 0;
}

void freesuffixarray(Suffixarray *suffixarray,Env *env)
{
  env_fa_xmunmap((void *) suffixarray->suftab,env);
  freeAlphabet(&suffixarray->alpha,env);
  freeEncodedsequence(&suffixarray->encseq,env);
  strarray_delete(suffixarray->filenametab,env);
  FREESPACE(suffixarray->filelengthtab);
}
