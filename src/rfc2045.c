/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *        rfc2045.c : décodage du contenu d'un message
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

/* Actuellement, ce fichier sait :
   _ lire un header Content-Type, renvoyer -1, 0 ou 1 selon qu'il est
     non compris, sans décodage nécessaire, ou décodable
     (on se limite à text-plain avec iso-8859-X et à utf8)
   _ lire un header Content-Transfer-Encoding, renvoyer -1, 0 ou 1 selon
     les cas (actuellement -1 ou 0).
     (on se limite à 7bit / 8bit)
   _ transcrire une ligne 8bit d'un code à un autre
     (malheureusement, le code d'arrivée ne peut être l'unicode, à
      cause de slang. Du moins c'est à vérifier)
   _ parser une option de transcription
*/

#include "flrn.h"

static UNUSED char rcsid[]="$Id$";

#ifdef WITH_CHARACTER_SETS

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>

/* Le define qui suit sert pour UCkd.h . Je préfère ne pas modifier
   ledit fichier d'entêtes */
#define CONST const
#include "chrtrans/UCkd.h"
#include "rfc2045.h"

#include "art_group.h"
#include "group.h"
#include "options.h"
#include "flrn_slang.h"

/* Pages incluses : cf init_charsets */
#include "chrtrans/iso01_uni.h"          /* ISO 8859-1 Latin-1   */
#include "chrtrans/iso02_uni.h"
#include "chrtrans/iso03_uni.h"
#include "chrtrans/iso04_uni.h"
#include "chrtrans/iso05_uni.h"          /* ISO 8859-5 Cyrillic  */
#include "chrtrans/iso06_uni.h"
#include "chrtrans/iso07_uni.h"
#include "chrtrans/iso08_uni.h"
#include "chrtrans/iso09_uni.h"
#include "chrtrans/iso10_uni.h"
#include "chrtrans/iso15_uni.h"
#include "chrtrans/def7_uni.h"           /* 7 bit approximations */
#include "chrtrans/utf8_uni.h"           /* utf-8 unicode */

/* Ici, on reprend le code de Lynx (légerement réécrit), lui-même dérivé
 * du code du noyau Linux. Tout ceci est soumis à la GNU Public Licence
 */

#define MAXCHARSETS 60               /* max character sets supported */

struct UC_charset {
        const char *MIMEname;
        const char *FLRNname;
        const u8* unicount;
        const u16* unitable;
        int num_uni;
        struct unimapdesc_str replacedesc;
        int lowest_eight;
        int enc;
        int codepage;   /* codepage number, used by OS/2 font-switching code */
};
static struct UC_charset UCInfo[MAXCHARSETS];

static int UCNumCharsets=0;
static int default_charset=0;
int terminal_charset=-1;
static int message_charset=-1;

#define UCT_ENC_7BIT 0
#define UCT_ENC_8BIT 1
#define UCT_ENC_8859 2
#define UCT_ENC_8BIT_C0 3
#define UCT_ENC_MAYBE2022 4
#define UCT_ENC_CJK 5
#define UCT_ENC_16BIT 6
#define UCT_ENC_UTF8 7

void UC_Charset_Setup(
    const char *UC_MIMEcharset,   /* non MIME */
    const char *UC_FLRNcharset,   /* nom pour flrn (comme celui de Lynx) */
    const u8 *unicount,           /* codage vers unicode : combien
                                     de traductions possibles (taille 256) */
    const u16 *unitable,          /* la table de traduction codage -> unicode
    					(taille nnuni) */
    int nnuni,                    /* taille de la table */
    struct unimapdesc_str replacedesc, /* unicode -> codage,
                                            surtout pour def7 */
    int lowest_eight,             /* plus petit 8bit avec correspondance */
    int UC_rawuni,                /* type d'encodage */
    int codepage			  /* page de code : directive C */
)
{
   int s;
   int i, found;

    /*
     *  Get (new?) slot.
     */
    found = -1;
    for (i = 0; i < UCNumCharsets && found < 0; i++) {
        if (!strcmp(UCInfo[i].MIMEname, UC_MIMEcharset)) {
            found = i;
        }
    }
    if (found >= 0) {
        s = found;
    } else {
        if (UCNumCharsets >= MAXCHARSETS) {
            fprintf(stderr,"Trop de jeux de caractères : %s/%s\n",
                          UC_MIMEcharset, UC_FLRNcharset);
            return;
        }
        s = UCNumCharsets;
        UCInfo[s].MIMEname = UC_MIMEcharset;
    }
    UCInfo[s].FLRNname = UC_FLRNcharset;
    UCInfo[s].unicount = unicount;
    UCInfo[s].unitable = unitable;
    UCInfo[s].num_uni = nnuni;
    UCInfo[s].replacedesc = replacedesc;
    if (replacedesc.isdefault) {
        default_charset = s;
    }
    if (UC_rawuni == UCT_ENC_UTF8)
        lowest_eight = 128;  /* cheat here */
    UCInfo[s].lowest_eight = lowest_eight;
    UCInfo[s].enc = UC_rawuni;
    UCInfo[s].codepage = codepage;
    if (found < 0)
        UCNumCharsets++;
    return;
}

/* parse juste une chaine charset... -1 -> pas réussi */
int Parse_charset (const char *buf) {
    int i;
    for (i = 0; i < UCNumCharsets; i++) {
        if ((strcasecmp(UCInfo[i].MIMEname, buf)==0) ||
	     (strcasecmp(UCInfo[i].FLRNname, buf)==0)) {
	   return i;
	}
    }
    return -1;
}

/* Parse une chaine d'option de CharSet et remvoie -1 si échec, 0 si
   vide, 1 sinon */
int Parse_charset_line (const char *charset_line) {
    const char *buf=charset_line;

    terminal_charset=-1;
    if (charset_line==NULL) return 0;
    while ((*buf) && (isblank(*buf))) buf++;
    if (*buf=='\0') return 0;
    if ((terminal_charset=Parse_charset(buf))>=0) {
        set_Display_Eight_Bit(UCInfo[terminal_charset].lowest_eight);
	return 1;
    }
    /* Échec */
    return -1;
}

/* Parse une ligne Content-Type, renvoie -1, 0 ou 1, selon... */
int Parse_ContentType_header (char *contenttype_line) {
    char *buf=contenttype_line, *buf2, sc='\0';
    int found;

    message_charset=-1;
    if (terminal_charset==-1) return 0;
    if (contenttype_line==NULL) return 0; /* on s'en moque */
    buf=strchr(buf,';');
    if (buf==NULL) return -1;
    buf++;
    while ((*buf) && (isblank(*buf))) buf++;
    if (*buf=='\0') return -1;
    if (strncasecmp(buf,"charset",7)!=0) return -1;
    buf=strchr(buf,'=');
    if (buf==NULL) return -1;
    buf++;
    while ((*buf) && (isblank(*buf))) buf++;
    if (*buf=='\0') return -1;
    if (*buf=='"') buf++;
    buf2=buf;
    while ((*buf2) && (!isblank(*buf2)) && (*buf2!='"')) buf2++;
    sc=*buf2;
    *buf2='\0';
    found=((message_charset=Parse_charset(buf))>=0);
    *buf2=sc;
    if (found) {
	if (message_charset==terminal_charset) return 0;
	if (UCInfo[message_charset].enc==0) return 0;
	return 1;
    }
    return -1;
}

int Length_one_character(char *chaine) {
    if (terminal_charset==-1) return 1;
    if (UCInfo[terminal_charset].enc!=UCT_ENC_UTF8) return 1;
    if (!(*chaine&0x80))
        return 1; /* ASCII range character */
    if (!(*chaine&0x40))
        return 1;            /* not a valid UTF-8 start */
    if ((*chaine & 0xe0) == 0xc0) return 2;
    if ((*chaine & 0xf0) == 0xe0) return 3;
    if ((*chaine & 0xf8) == 0xf0) return 4;
    if ((*chaine & 0xfc) == 0xf8) return 5;
    if ((*chaine & 0xfe) == 0xfc) return 6;
    return 1;
}

/* The following function is extracted from lynx */
/*
 * ** Get UCS character code for one character from UTF-8 encoded string.
 * **
 * ** On entry:
 * **      *ppuni should point to beginning of UTF-8 encoding character
 * ** On exit:
 * **      *ppuni is advanced to point to the next character
 * **              if there was a valid one; otherwise unchanged.
 * ** returns the UCS value
 * ** returns negative value on error (invalid UTF-8 sequence)
 * */
long UCGetUniFromUtf8String (u8 **ppuni)
{
    long uc_out = 0;
    char * p = *ppuni;
    int utf_count, i;
    if (!(**ppuni&0x80))
        return (long) *((*ppuni)++); /* ASCII range character */
    else if (!(**ppuni&0x40))
        return (-1);            /* not a valid UTF-8 start */
    if ((*p & 0xe0) == 0xc0) {
        utf_count = 1;
    } else if ((*p & 0xf0) == 0xe0) {
        utf_count = 2;
    } else if ((*p & 0xf8) == 0xf0) {
        utf_count = 3;
    } else if ((*p & 0xfc) == 0xf8) {
        utf_count = 4;
    } else if ((*p & 0xfe) == 0xfc) {
        utf_count = 5;
    } else { /* garbage */
        return (-1);
    }
    for (p = *ppuni, i = 0; i < utf_count ; i++) {
        if ((*(++p) & 0xc0) != 0x80)
            return (-1);
    }
    p = *ppuni;
    switch (utf_count) {
    case 1:
        uc_out = (((*p&0x1f) << 6) | (*(p+1)&0x3f));
        break;
    case 2:
        uc_out = (((((*p&0x0f) << 6) | (*(p+1)&0x3f)) << 6) | (*(p+2)&0x3f));
        break;
    case 3:
        uc_out = (((((((*p&0x07) << 6) | (*(p+1)&0x3f)) << 6) | (*(p+2)&0x3f)) << 6)
            | (*(p+3)&0x3f));
        break;
    case 4:
        uc_out = (((((((((*p&0x03) << 6) | (*(p+1)&0x3f)) << 6) | (*(p+2)&0x3f)) << 6)
                  | (*(p+3)&0x3f)) << 6) | (*(p+4)&0x3f));
        break;
    case 5:
        uc_out = (((((((((((*p&0x01) << 6) | (*(p+1)&0x3f)) << 6) | (*(p+2)&0x3f)) << 6)
                  | (*(p+3)&0x3f)) << 6) | (*(p+4)&0x3f)) << 6) | (*(p+5)&0x3f));
        break;
    }
    *ppuni = p + utf_count + 1;
    return uc_out;
}

int UCLenUniToUtf8 (long code)
{
    if (code <= 0 || code > 0x7fffffffL) return 0;
    if (code < 0x80L) return 1;
    if (code < 0x800L) return 2;
    if (code < 0x10000L) return 3;
    if (code < 0x200000L) return 4;
    if (code < 0x4000000L) return 5;
    return 6;
}
    

/*
 * **  This function converts a Unicode (UCode_t) value
 * **  to a multibyte UTF-8 character, which is loaded
 * **  into the buffer received as an argument.  The
 * **  buffer should be large enough to hold at least
 * **  seven characters (but should be declared as 8
 * **  to minimize byte alignment problems with some
 * **  compilers). - FM
 * */
int UCConvertUniToUtf8 (long code, unsigned char **buffer)
{
    char *ch = *buffer;

    if (!ch)
        return -1;

    if (code <= 0 || code > 0x7fffffffL) {
        *ch = '\0';
        return -1;
    }

    if (code < 0x800L) {
        *ch++ = (char)(0xc0 | (code>>6));
        *ch++ = (char)(0x80 | (0x3f & (code)));
        *ch = '\0';
    } else if (code < 0x10000L) {
        *ch++ = (char)(0xe0 | (code>>12));
        *ch++ = (char)(0x80 | (0x3f & (code>>6)));
        *ch++ = (char)(0x80 | (0x3f & (code)));
        *ch = '\0';
    } else if (code < 0x200000L) {
        *ch++ = (char)(0xf0 | (code>>18));
        *ch++ = (char)(0x80 | (0x3f & (code>>12)));
        *ch++ = (char)(0x80 | (0x3f & (code>>6)));
        *ch++ = (char)(0x80 | (0x3f & (code)));
        *ch = '\0';
    } else if (code < 0x4000000L) {
        *ch++ = (char)(0xf8 | (code>>24));
        *ch++ = (char)(0x80 | (0x3f & (code>>18)));
        *ch++ = (char)(0x80 | (0x3f & (code>>12)));
        *ch++ = (char)(0x80 | (0x3f & (code>>6)));
        *ch++ = (char)(0x80 | (0x3f & (code)));
        *ch = '\0';
    } else {
        *ch++ = (char)(0xfc | (code>>30));
        *ch++ = (char)(0x80 | (0x3f & (code>>24)));
        *ch++ = (char)(0x80 | (0x3f & (code>>18)));
        *ch++ = (char)(0x80 | (0x3f & (code>>12)));
        *ch++ = (char)(0x80 | (0x3f & (code>>6)));
        *ch++ = (char)(0x80 | (0x3f & (code)));
        *ch = '\0';
    }
    *buffer=ch;
    return 0;
}



/* décodage d'un bout de texte avec charset. retour=1 -> ligne_lue=output */
int Decode_ligne_with_charset (char *ligne_lue, char **output, int chrset) {
    int size_needed=0, can_replace=1, count, i;
    u8 *bufread=(u8 *) ligne_lue;
    long uni_carac;
    unsigned char *bufwrite;
    int le_charset;

    if (UCInfo[chrset].enc==UCT_ENC_7BIT) return 1;
    if ((UCInfo[chrset].enc>UCT_ENC_8859) &&
        (UCInfo[chrset].enc!=UCT_ENC_UTF8)) {
        *output=ligne_lue;
	return -1;
    }
    while (*bufread!='\0') {
        if ((*bufread<UCInfo[chrset].lowest_eight) ||
	    ((UCInfo[chrset].unicount[*bufread]==0) &&
	     (UCInfo[chrset].enc!=UCT_ENC_UTF8))) {
	    bufread++; /* on va le copier bêtement */
	    size_needed++;
	    continue;
	}
	if (UCInfo[chrset].enc!=UCT_ENC_UTF8) {
	  count=0; i=0;
	  while (i<*bufread) {
	     count+=UCInfo[chrset].unicount[i];
	     i++;
	  }
	  uni_carac=UCInfo[chrset].unitable[count];
	  bufread++;
	} else {    /* UTF8 */
	  uni_carac=UCGetUniFromUtf8String (&bufread);
	  if (uni_carac<0)   /* Erreur dans le code UTF8 */
	      bufread++;
	}
	/* bon, on a le caractère unicode */
	/* transcrivons-le en le bon truc */
	/* on regarde s'il existe pas déjà */
        if (UCInfo[terminal_charset].enc!=UCT_ENC_UTF8) {
	  i=0;
	  while (i<UCInfo[terminal_charset].num_uni) {
	      if (uni_carac==UCInfo[terminal_charset].unitable[i]) {
		  size_needed++;
		  break;
	      }
	      i++;
	  }
	  if (i==UCInfo[terminal_charset].num_uni) {
	     le_charset=default_charset;
	     if (le_charset!=terminal_charset) {
	       i=0;
	       while (i<UCInfo[default_charset].num_uni) {
	         if (uni_carac==UCInfo[default_charset].unitable[i]) {
		   size_needed++;
	           break;
	         }
	         i++;
	       }
	     }
	  } else le_charset=terminal_charset;
	  if (i<UCInfo[le_charset].num_uni) continue;
	  /* on cherche un remplaçant */
	  i=0;
	  while (i<UCInfo[terminal_charset].replacedesc.entry_ct) {
	      if (uni_carac==UCInfo[terminal_charset].replacedesc.entries[i].unicode)
		  break;
	      i++;
	  }
	  if ((i==UCInfo[terminal_charset].replacedesc.entry_ct) &&
	      (terminal_charset!=default_charset)) {
	      le_charset=default_charset;
	      i=0;
	      while (i<UCInfo[default_charset].replacedesc.entry_ct) {
		  if (uni_carac==UCInfo[default_charset].replacedesc.entries[i].unicode)
		      break;
	          i++;
              }
	  } else le_charset=terminal_charset;
	  if (i!=UCInfo[le_charset].replacedesc.entry_ct) {
	      size_needed+=strlen(UCInfo[le_charset].replacedesc.entries[i].replace_str);
	      can_replace=0;
	  }
	} else {
          size_needed+=UCLenUniToUtf8(uni_carac);
	  can_replace=0;
	}
    }
    if (can_replace) *output=ligne_lue; else
	*output=safe_malloc((size_needed+1));
    bufwrite=*output;
    bufread=(u8 *) ligne_lue;
    while (*bufread!='\0') {
        if ((*bufread<UCInfo[chrset].lowest_eight) ||
	    ((UCInfo[chrset].unicount[*bufread]==0) &&
	     (UCInfo[chrset].enc!=UCT_ENC_UTF8))) {
	    if ((u8 *)bufwrite!=bufread) *bufwrite=*bufread;
	    bufread++;
	    bufwrite++;
	    continue;
	}
	if (UCInfo[chrset].enc!=UCT_ENC_UTF8) {
	  count=0; i=0;
	  while (i<*bufread) {
	     count+=UCInfo[chrset].unicount[i];
	     i++;
	  }
	  uni_carac=UCInfo[chrset].unitable[count];
	  bufread++;
	} else {    /* UTF8 */
	  uni_carac=UCGetUniFromUtf8String (&bufread);
	  if (uni_carac<0)   /* Erreur dans le code UTF8 */
	      bufread++;
	}
	/* bon, on a le caractère unicode */
	/* transcrivons-le en le bon truc */
	/* on regarde s'il existe pas déjà */
	if (UCInfo[terminal_charset].enc!=UCT_ENC_UTF8) 
	{
	  i=0;
	  while (i<UCInfo[terminal_charset].num_uni) {
	      if (uni_carac==UCInfo[terminal_charset].unitable[i]) 
		  break;
	      i++;
	  }
	  if (i==UCInfo[terminal_charset].num_uni) {
	     le_charset=default_charset;
	     if (le_charset!=terminal_charset) {
	       i=0;
	       while (i<UCInfo[default_charset].num_uni) {
	         if (uni_carac==UCInfo[default_charset].unitable[i])
	             break;
	         i++;
	       }
	     }
	  } else le_charset=terminal_charset;
	  if (i<UCInfo[le_charset].num_uni) {
	     int a;
	     *bufwrite=0;
	     a=0;
	     while (a<=i) {
	        a+=UCInfo[le_charset].unicount[(int)*bufwrite];
	        if (a<=i) (*bufwrite)++;
	     }
	     bufwrite++;
	     continue;
	  }
	  /* on cherche un remplaçant */
	  i=0;
	  while (i<UCInfo[terminal_charset].replacedesc.entry_ct) {
	      if (uni_carac==UCInfo[terminal_charset].replacedesc.entries[i].unicode)
		  break;
	      i++;
	  }
	  if ((i==UCInfo[terminal_charset].replacedesc.entry_ct) &&
	      (terminal_charset!=default_charset)) {
	      le_charset=default_charset;
	      i=0;
	      while (i<UCInfo[default_charset].replacedesc.entry_ct) {
		  if (uni_carac==UCInfo[default_charset].replacedesc.entries[i].unicode)
		      break;
	          i++;
              }
	  } else le_charset=terminal_charset;
	  if (i!=UCInfo[le_charset].replacedesc.entry_ct) {
	      strcpy(bufwrite,
		    UCInfo[le_charset].replacedesc.entries[i].replace_str);
	      bufwrite+=strlen(
		    UCInfo[le_charset].replacedesc.entries[i].replace_str);
	  }
	} else {
          UCConvertUniToUtf8(uni_carac, &bufwrite);
	}
    }
    *bufwrite='\0';
    return (can_replace);
}

/* deoodage d'une ligne... renvoie une nouvelle ligne allouee et sa taille */
/* si retour=1, ligne_lue=output */
int Decode_ligne_message (char *ligne_lue, char **output) {
    return Decode_ligne_with_charset (ligne_lue, output, message_charset);
}

const char *get_name_charset(int num) {
    return UCInfo[num].MIMEname;
}

void init_charsets() {
   UC_CHARSET_SETUP;
   UC_CHARSET_SETUP_iso_8859_1;
   UC_CHARSET_SETUP_iso_8859_2;
   UC_CHARSET_SETUP_iso_8859_3;
   UC_CHARSET_SETUP_iso_8859_4;
   UC_CHARSET_SETUP_iso_8859_5;
   UC_CHARSET_SETUP_iso_8859_6;
   UC_CHARSET_SETUP_iso_8859_7;
   UC_CHARSET_SETUP_iso_8859_8;
   UC_CHARSET_SETUP_iso_8859_9;
   UC_CHARSET_SETUP_iso_8859_10;
   UC_CHARSET_SETUP_iso_8859_15;
   UC_CHARSET_SETUP_utf_8;
}

#endif
