/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *      flrn_format.c : formatage de lignes, date (vieux code)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/times.h>

#include "flrn.h"
#include "art_group.h"
#include "options.h"
#include "group.h"
#include "flrn_format.h"
#include "flrn_slang.h"
#include "flrn_shell.h"
#include "getdate.h"
/* langage SLang */
#include "slang_flrn.h"
#ifdef USE_SLANG_LANGUAGE
#include <slang.h>
#endif


/* On ne parse plus la date ici, on utilise getdate.y (cf GNU date) */
#if 0
/* Parsing de la date */
/* ce code est tiré de celui de mutt, placé aussi sous GPL */
/* cf ftp://ftp.lip6.fr/pub/unix/mail/mutt/ */
const char *Months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug",
 "Sep", "Oct", "Nov", "Dec", "ERR" };

static struct tz_t
{
  char *tzname;
  unsigned char zhours;
  unsigned char zminutes;
  unsigned char zoccident; /* west of UTC? */
  unsigned char xxx;       /* unused */
}
TimeZones[] =
{
  { "sst",  11,  0, 1, 0 }, /* Samoa */
  { "pst",   8,  0, 1, 0 },
  { "mst",   7,  0, 1, 0 },
  { "pdt",   7,  0, 1, 0 },
  { "cst",   6,  0, 1, 0 },
  { "mdt",   6,  0, 1, 0 },
  { "cdt",   5,  0, 1, 0 },
  { "est",   5,  0, 1, 0 },
  { "ast",   4,  0, 1, 0 }, /* Atlantic */
  { "edt",   4,  0, 1, 0 },
  { "wgt",   3,  0, 1, 0 }, /* Western Greenland */
  { "wgst",  2,  0, 1, 0 }, /* Western Greenland DST */
  { "aat",   1,  0, 1, 0 }, /* Atlantic Africa Time */
  { "egt",   1,  0, 1, 0 }, /* Eastern Greenland */
  { "egst",  0,  0, 0, 0 }, /* Eastern Greenland DST */
  { "gmt",   0,  0, 0, 0 },
  { "utc",   0,  0, 0, 0 },
  { "wat",   0,  0, 0, 0 }, /* West Africa */
  { "wet",   0,  0, 0, 0 }, /* Western Europe */
  { "bst",   1,  0, 0, 0 }, /* British DST */
  { "cat",   1,  0, 0, 0 }, /* Central Africa */
  { "cet",   1,  0, 0, 0 }, /* Central Europe */
  { "met",   1,  0, 0, 0 }, /* this is now officially CET */
  { "west",  1,  0, 0, 0 }, /* Western Europe DST */
  { "cest",  2,  0, 0, 0 }, /* Central Europe DST */
  { "eet",   2,  0, 0, 0 }, /* Eastern Europe */
  { "ist",   2,  0, 0, 0 }, /* Israel */
  { "sat",   2,  0, 0, 0 }, /* South Africa */
  { "ast",   3,  0, 0, 0 }, /* Arabia */
  { "eat",   3,  0, 0, 0 }, /* East Africa */
  { "eest",  3,  0, 0, 0 }, /* Eastern Europe DST */
  { "idt",   3,  0, 0, 0 }, /* Israel DST */
  { "msk",   3,  0, 0, 0 }, /* Moscow */
  { "adt",   4,  0, 0, 0 }, /* Arabia DST */
  { "msd",   4,  0, 0, 0 }, /* Moscow DST */
  { "gst",   4,  0, 0, 0 }, /* Presian Gulf */
  { "smt",   4,  0, 0, 0 }, /* Seychelles */
  { "ist",   5, 30, 0, 0 }, /* India */
  { "ict",   7,  0, 0, 0 }, /* Indochina */
/*{ "cst",   8,  0, 0, 0 },*/ /* China */
  { "hkt",   8,  0, 0, 0 }, /* Hong Kong */
/*{ "sst",   8,  0, 0, 0 },*/ /* Singapore */
  { "wst",   8,  0, 0, 0 }, /* Western Australia */
  { "jst",   9,  0, 0, 0 }, /* Japan */
/*{ "cst",   9, 30, 0, 0 },*/ /* Australian Central Standard Time */
  { "kst",  10,  0, 0, 0 }, /* Korea */
  { "nzst", 12,  0, 0, 0 }, /* New Zealand */
  { "nzdt", 13,  0, 0, 0 }, /* New Zealand DST */
  { NULL,    0,  0, 0, 0 }
};

static int mutt_check_month (const char *s)
{
  int i;

  for (i = 0; i < 12; i++)
    if (strncasecmp (s, Months[i], 3) == 0)
     return (i);
  return (-1); /* error */
}

static const char *uncomment_timezone (char *buf, size_t buflen, const char *tz)
{
  char *p;
  size_t len;

  if (*tz != '(')
    return tz; /* no need to do anything */
  tz++;
  while (isblank(*tz)) tz++;
  if ((p = strpbrk (tz, " )")) == NULL)
    return tz;
  len = p - tz;
  if (len > buflen - 1)
    len = buflen - 1;
  memcpy (buf, tz, len);
  buf[len] = 0;
  return buf;
}

/* converts struct tm to time_t, but does not take the local timezone into
 *    account unless ``local'' is nonzero */
time_t mutt_mktime (struct tm *t, int local)
{
  time_t g;

  static int AccumDaysPerMonth[12] = {
    0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
  };
 /* Compute the number of days since January 1 in the same year */
  g = AccumDaysPerMonth [t->tm_mon % 12];

 /* The leap years are 1972 and every 4. year until 2096,
  * but this algoritm will fail after year 2099 */
  g += t->tm_mday;
  if ((t->tm_year % 4) || t->tm_mon < 2)
    g--;
  t->tm_yday = g;

  /* Compute the number of days since January 1, 1970 */
  g += (t->tm_year - 70) * 365;
  g += (t->tm_year - 69) / 4;
  /* Compute the number of hours */
  g *= 24;
  g += t->tm_hour;

 /* Compute the number of minutes */
  g *= 60;
  g += t->tm_min;

  /* Compute the number of seconds */
  g *= 60;
  g += t->tm_sec;

/*  if (local)
    g += mutt_compute_tz (g, t);  */

  return (g);
}

/* Cette fonction modifiant la chaine de caractère date, on recopie date d'abord
 */
time_t parse_date (char *s)
{
  char *t, *ns;
  struct tm tm;
  int hour, min, sec;
  int i;
  int tz_offset = 0;
  int zhours = 0;
  int zminutes = 0;
  int zoccident = 0;
  const char *ptz;
  char tzstr[128];
  int count=0;

  /* kill the day of the week, if it exists. */
  ns=safe_strdup(s);
  if ((t = strchr (ns, ',')))
        t++;
    else
        t = ns;
  while (isblank(*t)) t++;
  memset (&tm, 0, sizeof(tm));
  while ((t = strtok (t, " \t")) != NULL)
  {
    switch (count)
    {
      case 0: /* day of the month */
         if (!isdigit ((int) *t)) {
           free(ns);
           return (0);
         }
         tm.tm_mday = atoi (t);
         if (tm.tm_mday > 31) {
           free(ns);
           return (0);
         }
         break;

      case 1: /* month of the year */
         if ((i = mutt_check_month (t)) < 0) {
           free(ns);
           return (0);
         }
         tm.tm_mon = i;
         break;

      case 2: /* year */
         tm.tm_year = atoi (t);
         if (tm.tm_year >= 1900)
             tm.tm_year -= 1900;
         break;

      case 3: /* time of day */
         if (sscanf (t, "%d:%d:%d", &hour, &min, &sec) == 3)
           ;
        else if (sscanf (t, "%d:%d", &hour, &min) == 2)
           sec = 0;
        else
        {
          if (debug) fprintf(stderr, "parse_date: beurk : %s\n",t);
          free(ns);
          return 0;
        }
        tm.tm_hour = hour;
        tm.tm_min = min;
        tm.tm_sec = sec;
        break;

      case 4: /* timezone */
       /* sometimes we see things like (MST) or (-0700) so attempt to 
        * compensate by uncommenting the string if non-RFC822 compliant
        */
        ptz = uncomment_timezone (tzstr, sizeof (tzstr), t);
        if (*ptz == '+' || *ptz == '-')
        {
          if (ptz[1] && ptz[2] && ptz[3] && ptz[4])
          {
            zhours = (ptz[1] - '0') * 10 + (ptz[2] - '0');
            zminutes = (ptz[3] - '0') * 10 + (ptz[4] - '0');
            if (ptz[0] == '-')
            zoccident = 1;
          }
        }
        else
        {
          for (i = 0; TimeZones[i].tzname; i++)
          if (!strcasecmp (TimeZones[i].tzname, ptz))
          {
            zhours = TimeZones[i].zhours;
            zminutes = TimeZones[i].zminutes;
            zoccident = TimeZones[i].zoccident;
            break;
          }
        /* ad hoc support for the European MET (now officially CET) TZ */
        if (strcasecmp (t, "MET") == 0)
        {
          if ((t = strtok (NULL, " \t")) != NULL)
          {
            if (!strcasecmp (t, "DST"))
            zhours++;
          }
        }
      }
      tz_offset = zhours * 3600 + zminutes * 60;
      if (!zoccident)
         tz_offset = -tz_offset;
         break;
    }
    count++;
    t = 0;
  }
  free(ns);
  if (count < 4) /* don't check for missing timezone */
    return (0);
  return (mutt_mktime (&tm,0) + tz_offset);
}

/* fin de la partie prise sur mutt */
#endif


time_t parse_date (char *s) {
    time_t bla=1;
    /* c'est une date absolue, donc on se fiche du tmeps courant */
    return get_date(s,&bla);
}

/* Détermination du real name a partir d'une chaine type From */
char *vrai_nom (char *nom) {
    char *result, *buf1, *buf2;
    
    result=safe_malloc((strlen(nom)+1)*sizeof(char));
    memset(result, 0, strlen(nom)+1);
    *result='\0';
    if (nom==NULL) return result;
    buf1=strchr(nom,'(');
    if (buf1) {
       buf1++;
       buf2=strrchr(buf1, ')');
       if (buf2==NULL) buf2=index(buf1, '\0');
       strncpy(result, buf1, buf2-buf1);
    } else {
      buf1=strchr(nom, '<');
      if (buf1==NULL) { strcpy(result,nom); return result; }
      strncpy(result, nom, buf1-nom);
      buf2=strrchr(nom, '>');
      if (buf2!=NULL) strcat(result, buf2+1);
    }
    return result;
}

/* Écriture de la date a partir d'une chaine de date */
char *local_date (char *date) {
    char *result;
    char *mydate=date;
    char mon[3];
    struct tm tim;

    result=safe_malloc(13*sizeof(char));
    if (date==NULL) {
       strcpy(result,"???");
       return result;
    }
    while(*mydate && !isdigit((int) *mydate)) mydate++;

    if(sscanf(mydate,"%d %s %d %d:%d:%d", &(tim.tm_mday), mon, &(tim.tm_year),
                                     &(tim.tm_hour), &(tim.tm_min),
                                     &(tim.tm_sec)) >= 5)
    {
    sprintf(result, "%3s %2d %2d:%2d", mon, tim.tm_mday, tim.tm_hour,
                                        tim.tm_min);
    } else
      strcpy(result,"???");
    return result;
}



/* Fonctions pour estimer la taille d'une chaine a afficher (avec les tab) */
int str_estime_len (char *la_chaine, int tmp_col, int tai_chaine) {
  int i;

  if (tai_chaine==-1) tai_chaine=strlen(la_chaine);
  for (i=0;i<tai_chaine;i++) 
    if (la_chaine[i]=='\t') 
      tmp_col=(tmp_col/Screen_Tab_Width+1)*Screen_Tab_Width; else tmp_col++;
  return tmp_col;
}
int to_make_len (char *la_chaine, int col_total, int tmp_col) {
  char *last_s=NULL, *buf=la_chaine;
  int tmp_col_ini=tmp_col;

  if (*buf=='\0') return 0;
  while (tmp_col<col_total) {
    if (isblank(*buf)) last_s=buf;
    if (*buf=='\t') tmp_col=(tmp_col/Screen_Tab_Width+1)*Screen_Tab_Width; else
      tmp_col++;
    buf++;
    if (*buf=='\0') return strlen(la_chaine);
  }
  if (isblank(*buf)) return buf-la_chaine+1; else 
     if (last_s) return last_s-la_chaine+1; else
       if (tmp_col_ini) return 0; else 
         return buf-la_chaine;
}
   

/* Copie d'une chaine dans un fichier, limitée... */
/* tmp_file=NULL : initialise... chaine=NULL : flush */
void copy_bout (FILE *tmp_file, char *chaine) {
  static char ligne[80];
  char *buf;
  int len1;
  static int len;

  if (tmp_file==NULL) {
    ligne[0]='\0';
    len=0;
    return;
  }
  if (chaine==NULL) {
     fprintf(tmp_file, "%s\n", ligne);
     ligne[0]='\0';
     len=0;
     return;
  }
  if ((buf=strchr(chaine,'\n'))) {
     *buf='\0';
     copy_bout(tmp_file,chaine);
     copy_bout(tmp_file,NULL);
     copy_bout(tmp_file,buf+1);
     return;
  }
  len1=to_make_len(chaine,72,len);
  if (len1==0) { /* On peut vouloir couper avant dans ce cas */
     char *tmp_string, *buf2;
     buf=strrchr(ligne,' ');
     if (buf) {
        buf2=strrchr(buf,'\t');
	if (buf2) buf=buf2;
	*buf='\0';
	fprintf(tmp_file, "%s\n", ligne);
	tmp_string=safe_strdup(buf+1);
	strcpy(ligne,tmp_string);
	len=str_estime_len(ligne, 0, -1);
	free(tmp_string);
	copy_bout (tmp_file, chaine);
	return;
     }
  }
  strncat(ligne,chaine,len1);
  len=str_estime_len (chaine, len, len1);
  if (len1!=strlen(chaine)) {
     fprintf(tmp_file, "%s\n", ligne);
     ligne[0]='\0';
     len=0;
     buf=chaine+len1;
     copy_bout(tmp_file,buf);
  }
}

/* Translation des sequences d'echappement dans Copy_format. On peut
   reecrire sur la même chaîne */
static int translate_escape_seq(char *dst, char *src) {
   if ((src==NULL) || (dst==NULL)) return -1;
   while (*src) {
     if ((*dst=*(src++))!='\\') {
        dst++;
     } else 
     switch (*(src++)) {
         case 'a' : *(dst++)='\a'; break;
         case 'b' : *(dst++)='\b'; break;
         case 'f' : *(dst++)='\f'; break;
         case 'n' : *(dst++)='\n'; break;
         case 'r' : *(dst++)='\r'; break;
         case 't' : *(dst++)='\t'; break;
         case 'v' : *(dst++)='\v'; break;
	 default : dst++; *(dst++)='?'; break;
     }
   }
   *dst='\0';
   return 0;
}

/* Copie d'une chaine formatée de headers dans un fichier... */
/* Et ca va marcher, parce qu'on y croit...  */
/* Mais si on veut copier dans une chaine ? */
/* Je passe aussi une chaine avec la taille limite... */
void Copy_format (FILE *tmp_file, char *chaine, Article_List *article,
                  char *result, int len) {
   char *att=safe_strdup(chaine), *ptr_att;
   char *buf;
   int len2=len;

   if (article) if (article->headers==NULL) return; /* Beurk ! */
   if (tmp_file) copy_bout(NULL,NULL); else result[0]=0;
   ptr_att=att;
   while (ptr_att && (*ptr_att)) {
     buf=strchr(ptr_att,'%');
     if (buf) *buf='\0';
     /* on copie pour faire marcher les sequences d'echappement */
     if (translate_escape_seq(ptr_att, ptr_att)<0) {
         /* ah, y'a eu un bug, pas logique, normalement il n'y a pas
	      de '%' */
        if (debug) fprintf (stderr,"Bug dans Copy_format : ne peut faire la traduction de %s\n", chaine);
     }
     if (buf==NULL) {
       if (tmp_file) {
          copy_bout(tmp_file,ptr_att);
       }
       else { 
          strncat(result,ptr_att,len2); len2-=strlen(ptr_att); 
	  if (len2<=0) { result[len]=0; free(att) ;return; } 
	  else result[len-len2]=0;
       }
       break;
     } else {
       if (tmp_file) {
          copy_bout(tmp_file,ptr_att);
       }
       else { 
          strncat(result,ptr_att,len2); len2-=strlen(ptr_att); 
	  if (len2<=0) { result[len]=0; free(att) ;return; } 
	  else result[len-len2]=0;
       }
       ptr_att=(++buf);
       switch (*buf) {
         case '%' : copy_bout(tmp_file,"%");
                    if (tmp_file) copy_bout(tmp_file,"%"); else
                    { strcat(result,"%"); len2--; 
		      if (len2<=0) { free(att); return; }
		      else result[len-len2]=0; }
                    ptr_att++;
                    break;
         case 'n' : { char *vrai_n;
                      ptr_att++;
		      if (article==NULL) break;
                      vrai_n=vrai_nom(article->headers->k_headers
                                                [FROM_HEADER]);
                      if (tmp_file) copy_bout(tmp_file,vrai_n); else
                      { strncat(result,vrai_n,len2); len2-=strlen(vrai_n); 
		        if (len2<=0) { result[len]=0; free(att); 
			               free(vrai_n); return; } else 
			result[len-len2]=0; }
                      free(vrai_n);
                      break;
                    }
         case 'i' : { char *msgid;
                      ptr_att++;
		      if (article==NULL) break;
	              msgid=safe_strdup(article->msgid);
                      if (tmp_file) copy_bout(tmp_file,msgid); else
                      { strncat(result,msgid,len2); len2-=strlen(msgid); 
		        if (len2<=0) { result[len]=0; free(att); free(msgid); 
			               return; } else 
			result[len-len2]=0; }
                      ptr_att++;
		      free(msgid);
                      break;
		    }
         case 'C' : { char *num=safe_malloc(sizeof(int)*3+2);
		      snprintf(num,sizeof(int)*3+1,"%d",article ? article->numero : 0);
                      if (tmp_file) copy_bout(tmp_file,num); else
                      { strncat(result,num,len2); len2-=strlen(num); 
		        if (len2<=0) { result[len]=0; free(att); free(num); 
			               return; } else 
			result[len-len2]=0; }
		      free(num);
                      ptr_att++;
                      break;
		    }
         case 'g' : { char *bla;
	              char *str=safe_strdup(Newsgroup_courant->name);
		      bla=truncate_group(str,1);
                      if (tmp_file) copy_bout(tmp_file,bla); else
                      { strncat(result,bla,len2); len2-=strlen(bla); 
		        if (len2<=0) { result[len]=0; free(att); 
			               free(str); return; } else 
			          result[len-len2]=0; }
		      free(str);
                      ptr_att++;
                      break;
		    }
         case 'G' : { char *str=safe_strdup(Newsgroup_courant->name);
                      if (tmp_file) copy_bout(tmp_file,str); else
                      { strncat(result,str,len2); len2-=strlen(str); 
		        if (len2<=0) { result[len]=0; free(att); 
			               free(str); return; } else 
			result[len-len2]=0; }
		      free(str);
                      ptr_att++;
                      break;
		    }
	 case '{' : /* un header du message */
	 case '`' : /* une commande à insérer */
	 case '[' : /* une commande de script */
	    /* ces trois cas devraient être unifiés */
	            if (*buf=='{')
	            { buf++;
		      ptr_att=strchr(buf,'}'); 
		      if (ptr_att) {
		         /* on suppose le header connu */
		         int n,len3;
			 char *str;
		         *ptr_att='\0';
			 len3=strlen(buf);
			 ptr_att++;
			 if (article==NULL) break;
			 for (n=0;n<NB_KNOWN_HEADERS;n++) {
			    if ((len3!=Headers[n].header_len) &&
			        (len3!=Headers[n].header_len-1)) continue;
			    if (strncasecmp(buf,Headers[n].header,len3)!=0)
			      continue;
		            str=article->headers->k_headers[n];	
			    if (str) {
			       str=safe_strdup(str);
                               if (tmp_file) copy_bout(tmp_file,str); else
                               { strncat(result,str,len2); len2-=strlen(str); if
	                         (len2<=0) { result[len]=0; free(str);
				             free(att); return; } else 
			         result[len-len2]=0; }
			       free(str);
			    }
			    break;
			 }
		         break;
	  	      } else ptr_att=buf; /* -> default */
		   } else if (*buf=='`')
	 	   {  buf++;
		      ptr_att=strchr(buf,'`');
		      if (ptr_att) {
		        /* TODO : deplacer ca et unifier */
			/* avec display_filter_file */
			FILE *file;
			int fd;
			char name[MAX_PATH_LEN];
			char *home;
		        *ptr_att='\0';
			fd=Pipe_Msg_Start(0,1,buf);
			*ptr_att='`';
			ptr_att++;
			if (fd<0) break;
			Pipe_Msg_Stop(fd);
			if (NULL == (home = getenv ("FLRNHOME")))
			    home = getenv ("HOME");
			if (home==NULL) break;
			strncpy(name,home,MAX_PATH_LEN-2-strlen(TMP_PIPE_FILE));
			strcat(name,"/"); strcat(name,TMP_PIPE_FILE);
			file=fopen(name,"r");
			if (file == NULL) break;
			while (fgets(tcp_line_read, MAX_READ_SIZE-1, file)) {
			   if (tmp_file) copy_bout(tmp_file,tcp_line_read);
			    else {
			      strncat(result,tcp_line_read,len2);
			      len2-=strlen(tcp_line_read);
			      if (len2<=0) { result[len]=0; return; }
			      else result[len-len2]=0; }
			}
			break;
		      } 
		      ptr_att=buf; /* -> default */
		   } else {
		      buf++;
		      ptr_att=strchr(buf,']');
		      if (ptr_att) {
#ifdef USE_SLANG_LANGUAGE
		         char *str=NULL, *str1;
			 *ptr_att='\0';
			 source_SLang_string(buf, &str);
			 *ptr_att=']';
			 ptr_att++;
			 if (str!=NULL) {
			    str1=safe_strdup(str);
			    SLang_free_slstring(str);
			    str=str1;
                            if (tmp_file) copy_bout(tmp_file,str); else
                            { strncat(result,str,len2); len2-=strlen(str); if
	                       (len2<=0) { result[len]=0; free(str);
			             free(att); return; } else 
			      result[len-len2]=0; }
			    free(str);
			 }
			 break;
#endif
	              }
		      ptr_att=buf; /* -> default */
		   }
         default : { char *str=safe_malloc(3);
		     sprintf(str,"%%%c",*buf);
		     if (debug) fprintf(stderr, "Mauvaise formatage : %%%s\n",
                                        buf);
                      if (tmp_file) copy_bout(tmp_file,str); else
                      { strncat(result,str,len2); len2-=strlen(str); if
	                (len2<=0) { result[len]=0; free(str); 
			            free(att); return; } else 
			result[len-len2]=0; }
		     free(str);
                     ptr_att++;
                     break;
		   }
       }
     }
   }
   if (tmp_file) copy_bout(tmp_file,NULL);
   free(att);
}

/* prépare une ligne de résumé */
/* by_msgid DOIT valoir 1 si l'article peut être extérieur au groupe */
char * Prepare_summary_line(Article_List *article, char *previous_subject,
    int level, char *out, int outlen, int by_msgid) {
    int deb_num, deb_nom, tai_nom, deb_sub, tai_sub, deb_dat, deb_rep;
    char *buf, buf2[15];
    char *subject;
    char *out_ptr=out;
   
    out[0]='\0';
    if ((article->headers==NULL) ||
	(article->headers->k_headers_checked[FROM_HEADER] == 0) ||
	(article->headers->k_headers_checked[SUBJECT_HEADER] == 0) ||
	((article->headers->k_headers_checked[DATE_HEADER] == 0)  &&
	 Options.date_in_summary))
      cree_header(article,0,0,by_msgid);
    if (article->headers==NULL) return NULL;
    if (article->headers->k_headers[FROM_HEADER]==NULL) return NULL;
    if (article->headers->k_headers[SUBJECT_HEADER]==NULL) return NULL;
    deb_num=0; deb_nom=7;
    deb_rep=outlen-8;
    if(Options.date_in_summary)
      deb_dat=deb_rep-13; else deb_dat=deb_rep;
    tai_nom=(deb_dat-deb_nom)/3;
    deb_sub=deb_nom+tai_nom+2;
    tai_sub=deb_dat-deb_sub-1;
    if ((tai_nom<=0) || (tai_sub<=0)) return NULL; /* TROP PETIT */
    
    sprintf(buf2,"%c%5d", (article->flag&FLAG_READ)?' ':'*',article->numero);
    sprintf(out_ptr,"%-*.*s ",deb_nom,deb_nom,buf2);
    out_ptr+=deb_nom;
    buf=vrai_nom(article->headers->k_headers[FROM_HEADER]);
    sprintf(out_ptr,"%-*.*s",tai_nom+2,tai_nom,buf);
    out_ptr+=tai_nom+2;
    free(buf);
    subject=article->headers->k_headers[SUBJECT_HEADER];
    if (!strncasecmp(subject,"Re: ",4)) subject +=4;
    if (previous_subject && !strncasecmp(previous_subject,"Re: ",4))
      previous_subject +=4;
    if (article->headers->k_headers[SUBJECT_HEADER])  {
      if(previous_subject && !strcmp(subject,previous_subject))
      {
	if (level && (level < tai_sub-1)) {
	  sprintf(out_ptr,"%*s%-*s",level,"",tai_sub-level+1,".");
	}
      } else
	sprintf(out_ptr,"%-*.*s",tai_sub+1,tai_sub,subject);
    } else sprintf(out_ptr,"%*s",tai_sub,"");
    out_ptr += tai_sub+1;
    if(Options.date_in_summary) {
      if (article->headers->date_gmt) 
        buf=safe_strdup(ctime(&article->headers->date_gmt)+4);
      else
        buf=local_date(article->headers->k_headers[DATE_HEADER]);
      sprintf(out_ptr,"%-*.*s",13,12,buf);
      out_ptr += 13;
      free(buf);
    }
    if (article->parent!=0) {
       if (article->parent>0) {
	   sprintf(out_ptr,"[%5d]", article->parent);
	 }
         else sprintf(out_ptr,"%-7s","[ ? ]");
    }
         else sprintf(out_ptr,"%7.7s","");
    return out;
}

/* parse la ligne de From:... dans un format pour l'instant unique et */
/* forum-like...						      */
/* ajoute le resultat à str */
/* on suppose qu'il y a de la place (2*strlen(from_line))+3 */
/* (plus strlen(sender) ) */

void ajoute_parsed_from(char *str, char *from_line, char *sender_line) {
   char *buf,*buf2,*bufs;
   int siz_login;
   buf=vrai_nom(from_line);
   strcat(str,buf);
   strcat(str," (");
   free(buf);
   bufs=str+strlen(str); /* sauvegarde du nom entre parentheses */
   buf=strchr(from_line,'<');
   if (buf) {
      buf2=strchr(buf,'@');
      if (buf2) 
        strncat(str,buf+1,buf2-buf-1);
      else strcat(str,buf+1);
   } else {
     buf2=strchr(from_line,'@');
     if (buf2)
       strncat(str,from_line,buf2-from_line);
     else strcat(str,from_line);
   }
   siz_login=strlen(bufs);
   strcat(str,")");
   if (sender_line) {
      /* on ajoute [sender]  si le login est différent */
      buf=strchr(sender_line,'@');
      if ((buf) && ((siz_login!=(buf-sender_line)) ||
                    (strncmp(bufs,sender_line,siz_login)))) {
	 strcat(str," [");
	 strncat(str,sender_line,buf-sender_line);
	 strcat(str,"]");
      }
   }
}
