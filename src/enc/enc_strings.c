/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-2003  Damien Massé et Joël-Yann Fourré
 *
 *        enc_strings.c : gestion des multi-byte strings, locales, etc...
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

/* Une partie de ce code (sur l'approximation des caractères) est inspiré
 * de Lynx, lui-même inspiré du noyau Linux. L'ensemble est soumis à
 * la GNU Public License */

#include <stdlib.h>
#include <wchar.h>
/* Les locales : locale.h et langinfo.h (pour le CODESET) */
/* puis iconv */
#include <locale.h>
#include <langinfo.h>
#include <iconv.h>
#include <ctype.h>

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include "../flrn.h"
#include "../compatibility.h"
#include "../flrn_slang.h"  /* pour set_Display_Eight_Bit */
#include "enc_strings.h"

static UNUSED char rcsid[]="$Id$";

/**************************************************************************/
/**************         Encodages variés               ********************/
/**************************************************************************/

/* includes pour chrtrans */
#define CONST const
#include "../chrtrans/UCkd.h"
/* Pages incluses : cf init_charsets */
#include "../chrtrans/iso01_uni.h"          /* ISO 8859-1 Latin-1   */
#include "../chrtrans/iso02_uni.h"
#include "../chrtrans/iso03_uni.h"
#include "../chrtrans/iso04_uni.h"
#include "../chrtrans/iso05_uni.h"          /* ISO 8859-5 Cyrillic  */
#include "../chrtrans/iso06_uni.h"
#include "../chrtrans/iso07_uni.h"
#include "../chrtrans/iso08_uni.h"
#include "../chrtrans/iso09_uni.h"
#include "../chrtrans/iso10_uni.h"
#include "../chrtrans/iso15_uni.h"
#include "../chrtrans/def7_uni.h"           /* 7 bit approximations */
#include "../chrtrans/utf8_uni.h"           /* utf-8 unicode */

#define MAXCHARSETS 20               /* max character sets supported */

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

/* encodages acceptables */
/* default charset is the default charset we can always used. It is
 * supposed to be constant, 7 bits us-ascii */
static int default_charset=0;
static int terminal_charset=1; /* this one must be 'iso-8859-1' */
static char *terminal_charset_string=NULL;
#define DEFAULT_MESSAGE_CHARSET "ISO-8859-1"
#define DEFAULT_LCCTYPE_VALUE "fr_FR"
static int message_charset;
static char *message_charset_string=NULL; 
static int default_message_charset=1;
static int editor_charset=-2; /* -2 : use terminal_charset,
				 -1 : editor_charset_string */
static char *editor_charset_string=NULL;
static int file_charset=-2; /* -2 : use terminal_charset,
			       -1 : file_charset_string */
static char *file_charset_string=NULL;
static char *post_charset_list=NULL; /* NULL : utiliser terminal_charset */

/* default_utf8 is the constant string used to put the local codeset
 * in utf8 */
const char default_utf8[]="en_US.utf8";
/* in fact, we will always be in utf8 for the locale (as iconv is
 * used to convert the charset. Nevertheless, we need to be able to
 * read the codeset from the locale */

/* parsing d'une chaîne codeset */
int Parse_charset (const char *buf) {
    int i;
    /* cas particulier : "ANSI_X3.4-1968" = "us-ascii" */
    if (strcasecmp(buf,"ANSI_X3.4-1968")==0) 
	return 0;
    for (i = 0; i < UCNumCharsets; i++) {
        if ((strcasecmp(UCInfo[i].MIMEname, buf)==0) ||
             (strcasecmp(UCInfo[i].FLRNname, buf)==0)) {
           return i;
        }
    }
    return -1;
}

int parse_terminal_charset (char *codeset) {
   int i;
   i = Parse_charset(codeset);
   if (i>=0) {
       if (terminal_charset_string) free(terminal_charset_string);
       terminal_charset_string=NULL;
       terminal_charset=i;
       set_Display_Eight_Bit(UCInfo[terminal_charset].lowest_eight);
       return 0;
   }
   terminal_charset=-1;
   if (terminal_charset_string) free(terminal_charset_string);
   terminal_charset_string=safe_strdup(codeset);
   return -1;
}


/* read the codeset from the environment */
/* 0 : ok. 1 : read 'C' or 'POSIX', choose 'iso-8859-1' 
 * -1 : charset not recognized.    -2 : error */
int read_terminal_charset () {
    char *locale;
    char *codeset;
    if ((locale=setlocale(LC_ALL,""))==NULL) return -2;
#ifdef I18N_GETTEXT
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain (PACKAGE);
    bind_textdomain_codeset(PACKAGE,"utf-8");
#endif
    if ((strcmp(locale,"C")==0) || (strcmp(locale,"POSIX")==0)) {
	/* on change LC_CTYPE pour slang-utf8 :( */
 	setlocale(LC_CTYPE,DEFAULT_LCCTYPE_VALUE); 
	return 1;
    }
    codeset=nl_langinfo(CODESET);
    if (codeset==NULL) return -2;
    return parse_terminal_charset (codeset);
}

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
    int codepage                          /* page de code : directive C */
)
{
   int s;
   int i, found;

    /*
     *      *  Get (new?) slot.
     *           */
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
size_t UCGetUniFromUtf8String (long *uch,const u8 *ppuni, size_t sleft)
{
    long uc_out = 0;
    const char * p = ppuni;
    int utf_count, i;
    if (!(*p&0x80)) {
        if (uch) *uch=(long) (*ppuni);
	return 1; /* ASCII range character */
    }
    else if (!(*p&0x40))
        return (size_t)(-1);            /* not a valid UTF-8 start */
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
    } else {
	return (size_t)(-1);
    }
    if (sleft<=utf_count) return (size_t)(-2);
    for (p = ppuni, i = 0; i < utf_count ; i++) {
         if ((*(++p) & 0xc0) != 0x80)
	     return (size_t)(-1);
    }
    p = ppuni;
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
    if (uch) *uch=uc_out;
    return utf_count+1;
}


int init_charsets() {
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
   read_terminal_charset();
/*   set_localeUTF8();   */ /* on garde la locale courante */
   return 0;
}

/**************************************************************************/
/**************         Gestion des chaînes flrn       ********************/
/**************************************************************************/

/* initialisation de la locale pour l'utf8 */
/* le retour n'est pas forcément nécessaire */
int is_in_utf8=0;
char *set_localeUTF8 () {
     return setlocale(LC_CTYPE,default_utf8);
}     
/* char *restore_locale () {
     return setlocale(LC_CTYPE,saved_locale); */
     /* TODO : à modifier. Ne pas oublier que la locale *peut* être utf8 */
/* } */

/* Conversion de wide characters vers chaîne courante */
/* on recopie dans *dest. Si *dest=NULL, on alloue une chaîne de taille
 * suffisante et len est ignoré. Sinon, on recopie au maximum et on renvoie
 * la chaîne restante à recopier */
/* renvoie -3 si *dest vaut *src */
size_t flrn_wcstofls (flrn_char **dest, wflrn_char **src,
	               size_t len)
{
#if 0
    if (*dest==NULL) {
       *dest=*src;
       *src=NULL;
       return (size_t)-3;
    }
    else
    {
       if (wcslen(*src)<len) {
	   wcscpy(*dest,*src);
	   *src=NULL;
	   return wcslen(*dest);
       } else {
           wcsncpy(*dest,*src,len-1);
	   *src+=len-1;
	   (*dest)[len-1]=L'\0';
	   return len;
       }
    }
#else
    size_t len2;
    flrn_mbstate ps;
    memset(&ps,0,sizeof(flrn_mbstate));
/*    if (!is_in_utf8) {
	set_localeUTF8();
    } */

    if (*dest==NULL) {
	/* il semble qu'un appel à wcsrtombs avec NULL pour dest
	 * détruit *src. C'est complètement stupide, mais on sait jamais */
	const wflrn_char *cpsrc=*src;
	len2 = wcsrtombs(NULL,&cpsrc,0,&ps)+1;
	*dest=safe_malloc(len2);
	memset(&ps,0,sizeof(flrn_mbstate));
	len2=wcsrtombs(*dest,(const wflrn_char **) src,len2,&ps);
    } else {
	len2=wcsrtombs(*dest,(const wflrn_char **) src,len,&ps);
    }
/*    if (!is_in_utf8) {
        restore_locale();
    }  */
    return len2;
#endif
}

/* Conversion de multi-bytes characters vers chaîne courante */
/* Il ne s'agit *pas* d'une conversion d'une locale vers chaîne courante
 * type iconv. Ici, c'est juste une fonction inverse de la précédente,
 * qui servira relativement peu. */
/* -1 : incorrect séquence. -2 : longueur bizarre */
/* -3 : copie directe de la source : on ne peut pas la libérer */
size_t flrn_mbstofls (flrn_char **dest, const nflrn_char **src,
	              size_t len, flrn_mbstate *ps) {
#if 0
    int ret;
    if (!is_in_utf8) {
	set_localeUTF8();
    }
    if (*dest==NULL) {
	flrn_mbstate ps2;
	memcpy(&ps2,ps,sizeof(flrn_mbstate));
	size_t ll=1;
	size_t lo;
	nflrn_char *place=*src;
        while (*place!='\0') {
            lo=mbrlen(place,MB_CUR_MAX,ps2);
	    if (((int)lo)<0) {
/*		if (!is_in_utf8) {
		    restore_locale();
		} */
		return lo;
	    }
	    ll++;
	    place+=lo;
	}
	*dest=safe_malloc((ll+1)*sizeof(flrn_char));
	len=ll;
    }
/*    if (!is_in_utf8) {
	restore_locale();
    }  */
    ret=mbsrtowcs(*dest, src, len, ps);
    (*dest)[len-1]=L'\0';
    return ret;
#else
    if (*dest==NULL) {
	*dest=(flrn_char *) *src;
	*src=NULL;
	return (size_t)(-3);
    } else {
	strncpy(*dest,*src,len-1);
	(*dest)[len-1]='\0';
	*src = (strlen(*src)<len ? NULL : ((*src)+len-1));
	return strlen(*dest);
    }
#endif
}


/*****************************************************************/
/* Conversions à base de iconv ***********************************/
/*****************************************************************/

struct conversion_etat {
    int status; /* -1 : erreur 0 : fermé, 1 : ouvert, 2 : direct */
                /* 3 : approximé (cas spécifique du terminal) */
    iconv_t cd_in;
    iconv_t cd_out;
};
#define NB_CONV_BASE 6
#define EDIT_CONV 0
#define FILE_CONV 1
#define TERM_CONV 2
#define MESS_CONV 3
#define UTF8_CONV 4
#define LOCA_CONV 5
static struct conversion_etat conv_base[NB_CONV_BASE];

int change_conversion_etat (struct conversion_etat *ce,
	const char *strset, int appterm) {
    if (ce==NULL) return -1;
    if (ce->status==1) {
	iconv_close(ce->cd_in);
	iconv_close(ce->cd_out);
    }
    if (strset==NULL) 
	strset=DEFAULT_MESSAGE_CHARSET;
    if (strcasecmp(strset,"UTF-8")==0) {
	ce->status=2;
	return 0;
    }
    ce->cd_in = iconv_open ("UTF-8",strset);
    if ((int)(ce->cd_in) == -1) {
	ce->cd_in = iconv_open ("UTF-8","US-ASCII");
	if ((int)(ce->cd_in) == -1) {
	    ce->status=2;
	    return -1;
	}
    }
    ce->cd_out = iconv_open (strset,"UTF-8");
    if ((int)(ce->cd_out) == -1) {
	ce->cd_out = iconv_open ("US-ASCII","UTF-8");
	if ((int)(ce->cd_out) == -1) {
	    iconv_close(ce->cd_in);
	    ce->status=2;
	    return -1;
	}
    }
    if (appterm==-1) ce->status=1; else ce->status=3;
    return 0;
}


int init_conv_base () {
    int i;
    for (i=0;i<UTF8_CONV;i++) {
	conv_base[i].status=0;
    }
    conv_base[UTF8_CONV].status=2; /* FIXME : cas wchar_t */
    /* EDIT_CONV */
    change_conversion_etat(&(conv_base[EDIT_CONV]),
	    (editor_charset == -1 ? editor_charset_string :
	     (editor_charset == -2 ? 
	      (terminal_charset==-1 ? 
	       terminal_charset_string : UCInfo[terminal_charset].MIMEname) :
	      UCInfo[editor_charset].MIMEname)),-1);
    /* FILE_CONV */
    change_conversion_etat(&(conv_base[FILE_CONV]),
	    (file_charset == -1 ? file_charset_string :
	     (file_charset == -2 ? 
	      (terminal_charset==-1 ? 
	       terminal_charset_string : UCInfo[terminal_charset].MIMEname) :
	      UCInfo[file_charset].MIMEname)),-1);
    /* TERM_CONV */
    change_conversion_etat(&(conv_base[TERM_CONV]),
	    (terminal_charset==-1 ? 
	       terminal_charset_string : UCInfo[terminal_charset].MIMEname),
	    terminal_charset);
    /* MESS_CONV */
    change_conversion_etat(&(conv_base[MESS_CONV]),
	    "US-ASCII",-1);
    return 0;
}



/* ouverture du canal de conversion "standard" (encodage local vers
 * chaîne courante). N'existe qu'avec la version "multi-byte".  */
/* retour : 0 : OK . -1 : erreur */
int fl_stdconv_open (const char *fromcode) {
    return change_conversion_etat(&(conv_base[LOCA_CONV]),fromcode,-1);
}
/* fermeture du canal standard */
int fl_stdconv_close () {
    change_conversion_etat(&(conv_base[LOCA_CONV]),NULL,-1);
    return 0;
}

/* Conversion d'une chaîne en encodage locale vers une chaîne courante */
/* La version ``wide characters'' est plus simple, mais on va faire les
 * deux, en utilisant iconv */
/* (-2) : le descripteur n'est pas ouvert. (-1) : erreur */
/* (-3) : *outbuf est devenu *inbuf */
size_t fl_conv_to_flstring_ce (struct conversion_etat *ce,
 	                    const char **inbuf, size_t *inbl,
	                    flrn_char **outbuf, flrn_char **outptr,
			    size_t *outbl) {
#if 0
    size_t len,res;
    char *sinbuf=*inbuf;
    if (stdconv_cd_opened==0) return (size_t)(-2);
    if (*outbuf==NULL) {
	flrn_mbstate ps2;
	memcpy(&ps2,ps,sizeof(flrn_mbstate));
	size_t ll=1;
	size_t lo,lcum=0;
	nflrn_char *place=*inbuf;
        while ((*place!='\0') && (*inbl>lcum)) {
            lo=mbrlen(place,MB_CUR_MAX,ps2);
	    if (((int)lo)<0) return lo;
	    ll++;
	    place+=lo;
	    lcum+=lo;
	}
	*outbuf=safe_malloc((ll+1)*sizeof(flrn_char));
	len=ll;
    } else {
	len=*outbl;
    }
    res=mbsrtowcs(*outbuf, inbuf, len, ps);
    if ((int)res!=-1) {
	*outptr=*outbuf+res;
        if (outbl) *outbl-=len;
	*inbl-=(*inbuf-sinbuf);
    }
    return res;
#else
    if (ce==NULL) return (size_t)(-1);
    if (ce->status==0) return (size_t)(-2);
    if (ce->status==2) {
       size_t maxl;
       if (*outbuf==NULL) {
	   if (strlen(*inbuf)<=*inbl) {
             *outbuf = (flrn_char *)*inbuf;
	     *outptr = *outbuf;
	     *inbuf = NULL;
	     return (size_t)(-3);
	   } else {
	      *outbuf=safe_malloc((*inbl)+1);
	      *outbl=*inbl;
	   }
       }
       maxl = *inbl;
       if (maxl > *outbl) maxl=*outbl;
       strncpy(*outbuf,*inbuf,maxl);
       *inbl-=maxl; *outbl-=maxl;
       *outptr=*outbuf+maxl;
       (*inbuf)+=maxl;
       return 0;
    }
    if (*outbuf==NULL) {
       /* on alloue 4*la taille standard. C'est bourrin mais bon */
       *outbl=4*strlen(*inbuf);
       *outbuf=safe_malloc(*outbl+1);
       *outptr=*outbuf;
    }
    if (*outptr==NULL) *outptr=*outbuf;
    /* problème : outbuf est modifié par iconv, donc on a plus
     * le début de la chaîne */
    iconv(ce->cd_in,NULL,NULL,NULL,NULL);
    return iconv(ce->cd_in,(char **)inbuf,inbl,outptr,outbl);
#endif
}

size_t fl_conv_to_flstring (char **inbuf, size_t *inbl,
	                    flrn_char **outbuf, flrn_char **outptr,
			    size_t *outbl) {
    return fl_conv_to_flstring_ce (&(conv_base[LOCA_CONV]),
	    (const char **)inbuf, inbl, outbuf, outptr, outbl);
}

/* conversion avec '?' pour les caractères incorrects */
size_t fl_appconv_to_flstring_ce (struct conversion_etat *ce,
	const char **inbuf, size_t *inbl, 
	flrn_char **outbuf, flrn_char **outptr, size_t *outbl) {
    size_t res;
    flrn_mbstate ps;
    memset(&ps,0,sizeof(flrn_mbstate));
    while (1) {
	res=fl_conv_to_flstring_ce(ce,inbuf,inbl,outbuf,outptr,outbl);
	if (((int)res==-1) && (errno==EILSEQ)) {
#if 0
	    (*inbl)--; (*outbl)--; (*inbuf)++; **outptr='?';
	    (*outptr)++;
#else
	    /* on suppose qu'on est en locale utf8, ce qui est la norme */
	    **outptr=fl_static('?'); (*outptr)++; (*outbl)--;
	    (*inbl)--; (*inbuf)++;
	    if (*inbl==0) return 0;
#endif
	} else return res;
    }
}

size_t fl_appconv_to_flstring (char **inbuf, size_t *inbl, 
        flrn_char **outbuf, flrn_char **outptr, size_t *outbl) {
    return fl_appconv_to_flstring_ce(&(conv_base[LOCA_CONV]),
	    (const char **)inbuf, inbl, outbuf, outptr, outbl);
}

/* ouverture du canal de conversion "reverse" (chaîne courante vers
 * encodage locale). N'existe qu'avec la version "multi-byte".  */
/* retour : 0 : OK . -1 : erreur */
int fl_revconv_open (const char *fromcode) {
    return change_conversion_etat(&(conv_base[LOCA_CONV]),fromcode,-1);
}
/* fermeture du canal standard */
int fl_revconv_close () {
    change_conversion_etat(&(conv_base[LOCA_CONV]),NULL,-1);
    return 0;
}

/* Conversion d'une chaîne en encodage locale vers une chaîne courante */
/* La version ``wide characters'' est plus simple (sauf qu'elle
 * utilise les locales et que ça facilite pas les choses), mais on va
 * faire les deux, en utilisant iconv */
/* (-2) : le descripteur n'est pas ouvert. (-1) : erreur */
/* (-3) : *outbuf est devenu *inbuf */
size_t fl_conv_from_flstring_ce (struct conversion_etat *ce,
	const flrn_char **inbuf, size_t *inbl,
        char **outbuf, char **outptr, size_t *outbl) {
#if 0
    size_t len2;
    flrn_char *sinbuf=*inbuf;
    flrn_mbstate ps;
    memset(&ps,0,sizeof(flrn_mbstate));

    if (revconv_cd_opened==0) return (size_t)(-2);
    if (*outbuf==NULL) {
	/* il semble qu'un appel à wcsrtombs avec NULL pour dest
	 * détruit *src. C'est complètement stupide, mais on sait jamais */
	const wflrn_char *cpsrc=*inbl;
	len2 = wcsrtombs(NULL,&cpsrc,0,&ps)+1;
	*outbuf=safe_malloc(len2);
	memset(&ps,0,sizeof(flrn_mbstate));
	len2=wcsrtombs(*outbuf,inbuf,len2,&ps);
    } else {
	len2=wcsrtombs(*outbuf,inbuf,len,&ps);
    }
    if ((int)len2>=0) {
       if (outbl) *outbl-=len2;
       *inbl-=(*inbuf-sinbuf);
       *outptr=*outbuf+len2;
    }
    return len2;
#else
    if (ce==NULL) return (size_t)(-1);
    if (ce->status==0) return (size_t)(-2);
    if (ce->status==2) {
       size_t maxl;
       if (*outbuf==NULL) {
	   if (strlen(*inbuf)==*inbl) {
             *outbuf = (char *)*inbuf;
	     *outptr = *outbuf;
	     *inbuf = NULL;
	     return (size_t)(-3);
	   } else {
	     *outbuf=safe_malloc((*inbl)+1);
	     *outbl=*inbl;
	   }
       }
       maxl = *inbl;
       if (maxl > *outbl) maxl=*outbl;
       strncpy(*outbuf,*inbuf,maxl);
       *inbl-=maxl; *outbl-=maxl;
       *outptr=*outbuf+maxl;
       *inbuf+=*inbl;
       return 0;
    }
    if (*outbuf==NULL) {
       /* on alloue la taille standard *2. */
       *outbl=strlen(*inbuf)*2;
       *outbuf=safe_malloc(*outbl+1);
       *outptr=*outbuf;
    }
    if (*outptr==NULL) *outptr=*outbuf;
    iconv(ce->cd_out,NULL,NULL,NULL,NULL);
    return iconv(ce->cd_out,(char **)inbuf,inbl,outptr,outbl);
#endif
}

size_t fl_conv_from_flstring (flrn_char **inbuf, size_t *inbl,
        char **outbuf, char **outptr, size_t *outbl) {
    return fl_conv_from_flstring_ce (&(conv_base[LOCA_CONV]),
	    (const flrn_char **)inbuf, inbl, outbuf, outptr, outbl);
}

/* conversion avec '?' pour les caractères incorrects */
size_t fl_appconv_from_flstring_ce (struct conversion_etat *ce,
	const flrn_char **inbuf, size_t *inbl, 
	char **outbuf, char **outptr, size_t *outbl) {
    size_t res;
    while (1) {
	res=fl_conv_from_flstring_ce(ce,inbuf,inbl,outbuf,outptr,outbl);
	if (((int)res==-1) && (errno==EILSEQ)) {
#if 0
	    /* l'erreur n'a pu être qu'à la transcription */
	    (*inbl)--; (*outbl)--; (*inbuf)++; **outptr='?';
	    (*outptr)++;
#else
	    /* on suppose qu'on est en locale utf8, ce qui est la norme */
	    **outptr='?'; (*outptr)++; (*outbl)--;
	    res=UCGetUniFromUtf8String(NULL, *inbuf, *inbl);
	    if (res==0) res=1;
	    if ((int)res>0) {
		(*inbuf)+=res;
		(*inbl)-=res;
		continue;
	    }
	    while ((int)res==-1) {
	      (*inbuf)++; (*inbl)--; if (*inbl==0) break;
	      res=UCGetUniFromUtf8String(NULL, *inbuf, *inbl);
	    }
	    if ((int)res==-2) return -1;
	    if (*inbl==0) return 0;
#endif
	} else return res;
    }
}

size_t fl_appconv_from_flstring (flrn_char **inbuf, size_t *inbl, 
        char **outbuf, char **outptr, size_t *outbl) {
    return fl_appconv_from_flstring_ce(&(conv_base[LOCA_CONV]),
	    (const flrn_char **)inbuf, inbl, outbuf, outptr, outbl);
}


/* conversion d'un caractère à l'aide des tables de conversion */
/* peut renvoyer la taille voulue, ou le caractère, selon les cas */
/* Il *FAUT* qu'il y ait assez de place dans conv, si conv!=NULL */
size_t approximate_wchar (long uni, char *conv, int le_charset, int *rs) {
    int i,a;
    i=(rs ? *rs : 0);
    while (i<UCInfo[le_charset].num_uni) {
	if (uni==UCInfo[le_charset].unitable[i]) {
	    if (conv) { 
		a=0;
		*conv=0;
		while (a<=i) {
		    a+=UCInfo[le_charset].unicount[(int)
			((unsigned char)*conv)];
		    if (a<=i) (*conv)++;
		}
	    }
	    if (rs) *rs=i;
   	    return (size_t)1;
	}
	i++;
    }
    if (rs) *rs=i;
    i=0;
    while (i<UCInfo[le_charset].replacedesc.entry_ct) {
	if (uni==UCInfo[le_charset].replacedesc.entries[i].unicode)
	{
	    size_t len=strlen(UCInfo[le_charset].
		    replacedesc.entries[i].replace_str);
	    if (conv) 
		strncpy(conv,
                      UCInfo[le_charset].replacedesc.entries[i].replace_str,
		      len);
            return len;
	}
	i++;
    }
    return -1;
}

int appr_conv_one (long unichar, 
	                 char **outptr, size_t *outbl) {
    int tmpi=0;
    int res;
    res=approximate_wchar(unichar,NULL,terminal_charset,&tmpi);
    if (res>=0) {
	 if (res==1) {
	     approximate_wchar(unichar,*outptr,terminal_charset,&tmpi);
	 } else if (res>1) {
	     if (res>*outbl) return 0;
	     approximate_wchar(unichar,*outptr,terminal_charset,&tmpi);
	 }
	 (*outbl)-=res;
	 (*outptr)+=res;
    } else {
       tmpi=0;
       res=approximate_wchar(unichar, NULL, default_charset,&tmpi);
       if (res>=0) {
	   if (res==1) {
	       approximate_wchar(unichar,*outptr,default_charset,&tmpi);
	   } else if (res>1) {
	       if (res>*outbl) return 0;
	       approximate_wchar(unichar,*outptr,default_charset,&tmpi);
	   }
	   (*outbl)-=res;
	   (*outptr)+=res;
       } else { 
	   *((*outptr)++)='?'; (*outbl)--; 
       }
    }
    return 1;
}

/* Conversion partielle "d'affichage", à partir des chaînes internes
 * de flrn, pour l'affichage à l'écran */
/* -1 : erreur de iconv   -2 : non initialisé.    -3 : inbuf = outbuf. */
size_t fl_approximate_conv (flrn_char **inbuf, size_t *inbl,
	                    char **outbuf, char **outptr, size_t *outbl) {
     if (conv_base[TERM_CONV].status<=0) return (size_t)(-2);
     if (conv_base[TERM_CONV].status<=2) 
	 return fl_appconv_from_flstring_ce (&(conv_base[TERM_CONV]),
		 (const flrn_char **)inbuf, inbl,outbuf,outptr,outbl);
#if 0
     /* app_conv==0 est impossible , et on refuse aussi app_conv = 2 
      * pour cause de traduction wchar_t -> UCS */
     return (size_t)(-2);
#else
     {
	 long uch;
	 flrn_char *sinbuf=*inbuf;
	 int can_replace=0;
	 int res,res2;

/*         if (!is_in_utf8) {
             set_localeUTF8 ();
	 }  */
	 if (*outbuf==NULL) {
	     int size_needed=0;
	     size_t size_left=*inbl;
	     can_replace=1;
	     while ((*sinbuf!='\0') && (size_left>0)) {
                if ((((unsigned char) (*sinbuf)) & 0x80)==0) {
		    /* on suppose que les caractères
				    ASCII sont bons partout */
		    /* ce qui est faux, mais je laisse tomber */
		    size_needed++;
		    sinbuf++;
		    size_left--;
		    continue;
		}
		/* TODO : ici ce sont les points unicode utilisés */
		/* res=mbrtowc(&uch,sinbuf,size_left,&ps); */
		res=UCGetUniFromUtf8String(&uch,sinbuf,size_left);
		if (res==-2) break;
		if (res==-1) {
		    sinbuf++;
		    size_needed++; size_left--;
		    continue;
		}
		if (res==0) break;
		sinbuf+=res; size_left-=res;
	        res2=approximate_wchar(uch, NULL, terminal_charset,NULL);
	        if (res2<0) 
		    res2=approximate_wchar(uch, NULL, default_charset,NULL);
	        if (res2<0) res2=1;
	        size_needed+=res2;
		if (res2>res) can_replace=0;
	     }
	     if (can_replace) *outbuf=*inbuf;
	        else *outbuf=safe_malloc(size_needed+1);
	     *outptr=*outbuf;
	     *outbl=size_needed;
	}
	 if (*outptr==NULL) *outptr=*outbuf;
	while (((**inbuf)!='\0') && (*inbl>0) && (*outbl>0)) {
             if ((((unsigned char) (**inbuf)) & 0x80)==0) {
		 (*outbl)--; (*inbl)--; 
		 *((*outptr)++)=*((*inbuf)++);
		 continue;
	     }
	     res=UCGetUniFromUtf8String(&uch,*inbuf,*inbl);
	     /* res=mbrtowc(&uch,*inbuf,*inbl,&ps); */
	     if (res==-2) break;
	     if (res==-1) {
		 (*outbl)--; (*inbl)--;
		 *((*outptr)++)='?'; (*inbuf)++;
		 continue;
	     }
	     if (appr_conv_one(uch,outptr,outbl)) 
	         { (*inbuf)+=res; (*inbl)-=res; }
	     else { **outptr='\0'; return (can_replace ? -3 : 0); }
	}
	if ((**inbuf)=='\0') *inbuf=NULL;
	**outptr='\0';
	return (can_replace ? -3 : 0);
     }
#endif
}


/* isolate_non_critical_element */
int isolate_non_critical_element (flrn_char *s, flrn_char *f,
	                          flrn_char *l, flrn_char **rf,
				  flrn_char **rl) {
    flrn_char *tl=f;
#if 0
    while ((f!=s) && (iswalnum(*f)) f--;
    while ((tl!=l) && (*(tl+1)!=L'<') && (*(tl+1)!=L')')) tl++;
    while ((tl!=f) && (*tl==L' ')) tl--;
    if (tl=l) 
	while ((*(tl+1)!=L'\0') && (iswalnum(*tl))) tl++;
#else
#define fl_isalnum2(X)	((((X)>='A') && ((X)<='Z')) || (((X)>='a') && ((X)<='z')) || (((X)>='0') && ((X)<='9')))
    while ((f!=s) && (fl_isalnum2(*(f-1)))) f--;
    while ((tl!=l) && (*(tl+1)!='<') && (*(tl+1)!=')')) tl++;
    while ((tl!=f) && (*tl==' ')) tl--;
    if (tl==l) 
	while ((*(tl+1)!='\0') && (fl_isalnum2(*tl))) tl++;
#endif
    *rf=f;
    *rl=tl;
    return 0;
}

static const char **list_best_conversion=NULL;
static int num_best_conversion=-1;
/* find best conversion uses a _static_ approach. Use with care */
/* called with NULL : reinitialise */
int find_best_conversion (flrn_char *str, size_t len, const char **result,
	const char *potential) {
    int count;
    int res;
    size_t pipo;
    char *outbuf=NULL, *outptr;
    size_t dummy2=0;
    const char *ch;
    const char **parcours, *fst=NULL;
    const char *pcl=(potential ? potential : post_charset_list);
    char *bla;
    if (str==NULL) {
	if (list_best_conversion) free(list_best_conversion);
	list_best_conversion=NULL;
	num_best_conversion=-1;
	return 0;
    }
    if (list_best_conversion==NULL) {
	if (pcl==NULL) 
	    count=1;
	else {
          count=1;
	  ch = pcl;
	  while ((ch=strchr(ch,':'))) { count++; ch++; }
	}
	list_best_conversion=safe_malloc(count*sizeof(char *));
	num_best_conversion=count;
	if (post_charset_list==NULL)
	    *list_best_conversion=(terminal_charset_string ?
		      terminal_charset_string : 
		      (terminal_charset>=0 ? UCInfo[terminal_charset].MIMEname
		                           : NULL));
	else {
	    parcours=list_best_conversion;
	    *parcours=pcl;
	    ch=pcl;
	    while ((ch=strchr(pcl,':'))) { 
		ch++;
	        *(++parcours)=ch;
	    }
	}
    }
    parcours=list_best_conversion;
    count=0;
    while (count<num_best_conversion) {
	if (*parcours==NULL) { count++; parcours++; continue; }
	ch=strchr(*parcours,':');
	pipo=(ch ? ch-(*parcours) : strlen(*parcours));
	bla=malloc(pipo+1);
	strncpy(bla,*parcours,pipo);
	bla[pipo]='\0';
        res=fl_revconv_open(bla);
	free(bla);
	if (res<0) { *parcours=NULL; count++; parcours++; continue; }
	res=(int) fl_conv_from_flstring(&str,&len,&outbuf,&outptr,&dummy2);
	if (res>-2) free(outbuf);
	fl_revconv_close();
	outbuf=NULL;
	if (res==-1) {
	    if (errno==EINVAL) break; 
	    *parcours=NULL; count++; parcours++; continue; 
        }
	if (fst==NULL) fst=*parcours;
	count++;
	parcours++;
	break;
    }
    *result=fst;
    return 0;
}

int check_7bits_string (const char *str) {
    while (*str) {
	if ((*str) & 0x80) return 0;
	str++;
    }
    return 1;
}


/* conversion rapide */
int conversion_to_generic(struct conversion_etat *ce,
	const flrn_char *flstr, char **resbuf, size_t len,
	size_t fllen) {
    size_t res2, resl, lenflstr;
    char *resptr;
    if ((len==0) && (fllen==(size_t)(-1)) && 
	    ((ce->status==2) || (check_7bits_string(flstr)))) {
	*resbuf=(char *)flstr;
	return 1;
    }
    lenflstr=(fllen==(size_t)(-1) ? fl_strlen(flstr) : fllen);
    if (len==0) {
       *resbuf=resptr=NULL;
    } else {
	resptr=*resbuf;
	resl=len;
    }
    res2 = fl_appconv_from_flstring_ce(ce,
		&flstr, &lenflstr, resbuf,&resptr,&resl);
    if ((res2!=(size_t)(-3)) && (resptr)) *resptr='\0';
    if ((res2==(size_t)(-1)) || (res2==(size_t)(-2))) return -1;
    return ((int)res2==-3 ? 1 : 0);
}
/* conversion d'une chaîne de l'éditeur/slang */
/* retour : 0 -> chaîne allouée.   1 : chaîne non allouée 
 * (c'est la chaîne initiale, ou un peu modifiée) */
int conversion_from_generic(struct conversion_etat *ce,
	const char *str, flrn_char **resbuf, size_t fllen,
	size_t len) {
    size_t res2, resl, lenstr;
    flrn_char *resptr;
    if ((fllen==0) && (len==(size_t)(-1)) && 
	    ((ce->status==2) || (check_7bits_string(str)))) {
	*resbuf=(flrn_char *)str;
	return 1;
    }
    lenstr=(len==(size_t)(-1) ? strlen(str) : len);
    if (fllen==0) {
       *resbuf=resptr=NULL;
    } else {
	resptr=*resbuf;
	resl=len;
    }
    res2 = fl_appconv_to_flstring_ce(ce,
		&str, &lenstr, resbuf,&resptr,&resl);
    if ((res2!=(size_t)(-3)) && (resptr)) *resptr=fl_static('\0');
    if ((res2==(size_t)(-1)) || (res2==(size_t)(-2))) return -1;
    return ((int)res2==-3 ? 1 : 0);
}
/* conversion d'une chaîne vers l'éditeur/slang */
/* retour : 0 -> chaîne allouée.   1 : chaîne non allouée 
 * (c'est la chaîne initiale, ou un peu modifiée) */
int conversion_to_editor(const flrn_char *flstr, char **resbuf, size_t len,
	size_t fllen) {
    return conversion_to_generic(&(conv_base[EDIT_CONV]),
	    flstr,resbuf,len,fllen);
}
/* conversion d'une chaîne de l'éditeur/slang */
/* retour : 0 -> chaîne allouée.   1 : chaîne non allouée 
 * (c'est la chaîne initiale, ou un peu modifiée) */
int conversion_from_editor(const char *str, flrn_char **resbuf, size_t fllen,
	size_t len) {
    return conversion_from_generic(&(conv_base[EDIT_CONV]),
	    str,resbuf,fllen,len);
}
int conversion_to_file(const flrn_char *flstr, char **resbuf, size_t len,
	size_t fllen) {
    return conversion_to_generic(&(conv_base[FILE_CONV]),
	    flstr,resbuf,len,fllen);
}
int conversion_from_file(const char *str, flrn_char **resbuf, size_t fllen,
	size_t len) {
    return conversion_from_generic(&(conv_base[FILE_CONV]),
	    str,resbuf,fllen,len);
}
int conversion_to_message(const flrn_char *flstr, char **resbuf, size_t len,
	size_t fllen) {
    return conversion_to_generic(&(conv_base[MESS_CONV]),
	    flstr,resbuf,len,fllen);
}
#ifdef USE_CONTENT_ENCODING
static int QPmode=0;
extern int Index_hex[128]; /* defini dans rfc2047.c */
void change_QP_mode(int nm) {
    QPmode=nm;
}
#endif
int conversion_from_message(const char *str, flrn_char **resbuf, size_t fllen,
	size_t len) {
    int res;
#ifdef USE_CONTENT_ENCODING
    char *strbis=NULL;
    if (QPmode) {  /* assert fllen = 0, len = (-1) */
	char *p,*q;
	if (strchr(str,'=')) {
	  strbis = safe_strdup(str);
	  p=q=strbis;
	  while (*p) {
	      if (*p == '=')
	      {
		  if ((*(p+1) == '\0') || *(p+2) == '\0')
		     { *(++p)='\0'; *(q++)='?'; }
		  else 
		     { *(q++) = Index_hex[(*(p+1))%128] << 4 | 
		              Index_hex[*(p+2)%128] ; 
	               p+=3;
		     }
	     } else { *(q++)=*(p++); }
	  }
	  *q='\0';
          str=strbis;
        }
    }
#endif
    res = conversion_from_generic(&(conv_base[MESS_CONV]),
	    str,resbuf,fllen,len);
#ifdef USE_CONTENT_ENCODING
    if (strbis) {
	if (res!=1) free(strbis); else res=0;
    }
#endif
    return res;
}
int conversion_to_utf8(const flrn_char *flstr, char **resbuf, size_t len,
	size_t fllen) {
    return conversion_to_generic(&(conv_base[UTF8_CONV]),
	    flstr,resbuf,len,fllen);
}
int conversion_from_utf8(const char *str, flrn_char **resbuf, size_t fllen,
	size_t len) {
    return conversion_from_generic(&(conv_base[UTF8_CONV]),
	    str,resbuf,fllen,len);
}
#if 0
int conversion_to_terminal(const flrn_char *flstr, char **resbuf, size_t len,
	size_t fllen) {
    return conversion_to_generic(&(conv_base[TERM_CONV]),
	    flstr,resbuf,len,fllen);
}
#endif
int conversion_to_terminal(const flrn_char *flstr, char **resbuf, size_t len,
	size_t fllen) {
    size_t res2, resl, lenflstr;
    char *resptr;
    flrn_char *sflstr,*ssflstr;
    int tof=0;
    if ((len==0) && (fllen==(size_t)(-1)) && 
	    ((conv_base[TERM_CONV].status==2) || 
	     (check_7bits_string(flstr)))) {
	*resbuf=(char *)flstr;
	return 1;
    }
    lenflstr=(fllen==(size_t)(-1) ? fl_strlen(flstr) : fllen);
    if (len==0) {
       *resbuf=resptr=NULL;
    } else {
	resptr=*resbuf;
	resl=len;
    }
    if (*resbuf==NULL) /* la chaîne peut être écrasée... */ {
	sflstr=safe_flstrdup(flstr);
	tof=1;
    } else sflstr=(flrn_char *)flstr;
    ssflstr=sflstr;
    res2 = fl_approximate_conv(&sflstr,
	    &lenflstr, resbuf,&resptr,&resl);
    if ((res2!=(size_t)(-3)) && (tof)) free(ssflstr);
    if ((res2!=(size_t)(-3)) && (resptr)) *resptr='\0';
    if ((res2==(size_t)(-1)) || (res2==(size_t)(-2))) return -1;
    /* return ((int)res2==-3 ? 1 : 0); */ return 0;
}

int conversion_from_terminal(const char *str, flrn_char **resbuf, size_t fllen,
	size_t len) {
    return conversion_from_generic(&(conv_base[TERM_CONV]),
	    str,resbuf,fllen,len);
}


/****** PARSINGS ********/
/* parse d'un content-type header */
const char *parse_ContentType_header (flrn_char *contenttype_line) {
    flrn_char *buf=contenttype_line, *buf2, sc=fl_static('\0');

    if (message_charset_string) free(message_charset_string);
    message_charset_string=NULL;
    message_charset=default_message_charset;
    if (contenttype_line) {
       buf=fl_strchr(buf,fl_static(';'));
       if (buf) {
	   buf++;
	   while ((*buf) && (fl_isspace(*buf))) buf++;
	   if (fl_strncasecmp(buf,fl_static("charset"),7)==0) {
	      buf=fl_strchr(buf,fl_static('='));
	      if (buf) {
		  buf++;
		  while ((*buf) && (fl_isspace(*buf))) buf++;
		  if (*buf) {
		      if (*buf==fl_static('"')) buf++;
		      buf2=buf;
		      while ((*buf2) && ((fl_isalnum(*buf2)) || 
			      (fl_strchr(fl_static("._-/()"),*buf2)))) buf2++;
		      sc=*buf2;
		      *buf2=fl_static('\0');
		      message_charset=Parse_charset(fl_static_rev(buf));
		      if (message_charset==-1) {
			  message_charset_string=safe_strdup
			      (fl_static_rev(buf));
		      }
		      *buf2=sc;
		  }
	      }
	   }
       }
    }
    change_conversion_etat(&(conv_base[MESS_CONV]),
	    message_charset == -1 ? message_charset_string :
	    UCInfo[message_charset].MIMEname,-1);
    return (message_charset == -1 ? message_charset_string :
	    UCInfo[message_charset].MIMEname);
}

int Change_message_conversion(char *str) {
    message_charset=Parse_charset(str);
    if (message_charset==-1) {
	message_charset_string=safe_strdup(str);
    }
    change_conversion_etat(&(conv_base[MESS_CONV]),
	    message_charset == -1 ? message_charset_string :
	    UCInfo[message_charset].MIMEname,-1);
    return 0;
}

int Parse_termcharset_line(flrn_char *str) {
    if (str==NULL) {
	terminal_charset=default_charset;
    } else {
        while ((*str) && (fl_isblank(*str))) str++;
	return parse_terminal_charset(fl_static_rev(str));
    }
    change_conversion_etat(&(conv_base[TERM_CONV]),
	    terminal_charset == -1 ? terminal_charset_string :
	    UCInfo[terminal_charset].MIMEname,terminal_charset);
    if (file_charset==-2)
	change_conversion_etat(&(conv_base[FILE_CONV]),
	    terminal_charset == -1 ? terminal_charset_string :
	    UCInfo[terminal_charset].MIMEname,-1);
    if (editor_charset==-2)
	change_conversion_etat(&(conv_base[EDIT_CONV]),
	    terminal_charset == -1 ? terminal_charset_string :
	    UCInfo[terminal_charset].MIMEname,-1);
    return 0;
}


int Parse_filescharset_line(flrn_char *str) {
    if (str==NULL) {
	file_charset=-2;
	change_conversion_etat(&(conv_base[FILE_CONV]),
		terminal_charset == -1 ? terminal_charset_string :
		UCInfo[terminal_charset].MIMEname,-1);
        return 0;
    } else {
        while ((*str) && (fl_isblank(*str))) str++;
	if (*str==fl_static('\0')) {
	    file_charset=-2;
	    change_conversion_etat(&(conv_base[FILE_CONV]),
		    terminal_charset == -1 ? terminal_charset_string :
		    UCInfo[terminal_charset].MIMEname,-1);
	    return 0;
	}
        file_charset=Parse_charset(fl_static_rev(str));
        if (file_charset==-1) {
	    file_charset_string=safe_strdup (fl_static_rev(str));
        }
    }
    change_conversion_etat(&(conv_base[FILE_CONV]),
	    file_charset == -1 ? file_charset_string :
	    UCInfo[file_charset].MIMEname,-1);
    return 0;
}

int Parse_editorcharset_line(flrn_char *str) {
    if (str==NULL) {
	editor_charset=-2;
	change_conversion_etat(&(conv_base[EDIT_CONV]),
		terminal_charset == -1 ? terminal_charset_string :
		UCInfo[terminal_charset].MIMEname,-1);
        return 0;
    } else {
        while ((*str) && (fl_isblank(*str))) str++;
	if (*str==fl_static('\0')) {
	    editor_charset=-2;
	    change_conversion_etat(&(conv_base[EDIT_CONV]),
		    terminal_charset == -1 ? terminal_charset_string :
		    UCInfo[terminal_charset].MIMEname,-1);
	    return 0;
	}
        editor_charset=Parse_charset(fl_static_rev(str));
        if (editor_charset==-1) {
	    editor_charset_string=safe_strdup (fl_static_rev(str));
        }
    }
    change_conversion_etat(&(conv_base[EDIT_CONV]),
	    editor_charset == -1 ? editor_charset_string :
	    UCInfo[editor_charset].MIMEname,-1);
    return 0;
}

int convert_termchar (const char *str, size_t len, flrn_char **res, size_t lb) {
    size_t rc, slb=0;
    flrn_char *ptr=NULL, *ptr2;
    ptr2=ptr;
    rc=fl_conv_to_flstring_ce(&(conv_base[TERM_CONV]),
	    &str,&len,&ptr,&ptr2,&slb);
    if (rc==0) {
	/* on a converti un caractère */
	*ptr2='\0';
	if (res) {
	    if (fl_strlen(ptr)>lb) {
		*res=ptr;
	    } else {
		fl_strncpy(*res,ptr,lb);
		free(ptr);
	    }
	    return 1;
	} else return 1;
    } else if (rc==(size_t)(-3)) {
	rc=UCGetUniFromUtf8String(NULL,ptr,strlen(ptr));
	if ((int)rc==(-1)) return -1;
	if ((int)rc==(-2)) return 0;
	if (rc>lb) {
	    *res=safe_strdup(ptr);
	} else fl_strncpy(*res,ptr,lb);
	return lb;
    } else if (rc==(size_t)(-1)) {
	free(ptr);
	if (errno==EINVAL) return 0;
	return -1;
    }
    return -1;
}

int Parse_postcharsets_line(flrn_char *str) { 
    if (post_charset_list) free(post_charset_list);
    if (str==NULL) post_charset_list=NULL; 
    else post_charset_list=safe_flstrdup(fl_static_tran(str));
    return 0; 
}

size_t previous_flch(flrn_char *str,size_t len, size_t mlen) {
    size_t a=len+mlen,b=0,c;
    flrn_char *p=str+len;
    if (a==0) return 0;
#if 0
    return 1;
#else
    while ((a>0) && (*(p-1)&0x80)) { p--; a--; b++; }
    /* on se trouve au début d'un caractère 8bit avec un avant,
     * ou alors on n'a pas bougé */
    if (b==0) return 1;
    while (b>0) {
       c=UCGetUniFromUtf8String(NULL,p,b);
       if ((int)c<0) {
	   b--; p++; 
	   if (b==0) return 1;
       } else {
	   if (b<c) return b;
	   b-=c; p+=c;
	   if (b==0) return c;
       }
    }
    return c;
#endif
}

size_t next_flch(flrn_char *str,size_t len) {
    size_t a;
    if (*(str+len)==fl_static('\0')) return 0;
#if 0
    return 1;
#else
    a=UCGetUniFromUtf8String(NULL,str+len,strlen(str+len));
    return a;
#endif
}

void width_termchar(char *str,int *wdt, size_t *len) {

    if (terminal_charset!=-1) {
	if (UCInfo[terminal_charset].enc != UCT_ENC_UTF8) {
	    *wdt=1;
	    *len=1;
	    return;
	} else {
#ifndef HAVE_WCHAR_H
	    long uch;
	    size_t a;
	    a=UCGetUniFromUtf8String(&ucg,str,strlen(str));
	    *len = ((int)a<=0? 1 : a);
	    *wdt=1;
	    return;
#endif
	}
    }
#ifdef HAVE_WCHAR_H
    {
	    size_t a;
	    /* en fait, ce serait plutôt HAVE_WCWIDTH */
	    mbstate_t ps;
	    wchar_t ch;
	    /* mbrtowc(NULL,NULL,0,&ps); */
	    memset(&ps,0,sizeof(mbstate_t));
	    a = mbrtowc(&ch,str,strlen(str),&ps);
	    if ((int)a<=0) { *wdt=1; *len=1; }
	    else {
		*len=a;
		*wdt=wcwidth((wint_t)ch);
		if (*wdt<=0) *wdt=1;
	    }
	    return;
    }
#else
    *wdt=1; *len=1; return; /* FIXME : a corriger */
#endif
}

#if 0
/* traduction d'une chaîne statique en flrn_char */
flrn_char *fl_static_tran(const char *chaine) {
    static flrn_char result[200];
    flrn_char *bla=result;
    int t=199;
    while ((*chaine) && (t>0)) { *(bla++)=(flrn_char) (*(chaine++)); t--; }
    *bla='\0';
    return result;
}
char *fl_static_rev(const flrn_char *chaine) {
    static char result[200];
    char *bla=result;
    int t=199;
    while ((*chaine) && (t>0)) { *(bla++)=(char) (*(chaine++)); t--; }
    *bla='\0';
    return result;
}
flrn_char *fl_dynamic_tran(const char *chaine, int *tofree) {
    flrn_char *result=safe_malloc((strlen(chaine)+1)*sizeof(flrn_char));
    flrn_char *bla=result;
    *tofree=1;
    while (*chaine) { *(bla++)=(flrn_char) (*(chaine++)); }
    *bla='\0';
    return result;
}
char *fl_dynamic_rev(const flrn_char *chaine, int *tofree) {
    char *result=safe_malloc(wcslen(chaine)+1);
    char *bla=result;
    *tofree=1;
    while (*chaine) { *(bla++)=(char) (*(chaine++)); }
    *bla='\0';
    return result;
}
#endif
