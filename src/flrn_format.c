/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *      flrn_format.c : formatage de lignes, date (vieux code)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
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
#include "enc/enc_strings.h"

static UNUSED char rcsid[]="$Id$";


time_t parse_date (flrn_char *s) {
    time_t bla=1;
    /* c'est une date absolue, donc on se fiche du tmeps courant */
    /* la date n'est pas, a priori, avec des caractères 8bits */
    return get_date(fl_static_rev(s),&bla);
}

/* Détermination du real name a partir d'une chaine type From */
flrn_char *vrai_nom (flrn_char *nom) {
    flrn_char *result, *buf1, *buf2;
    
    result=safe_malloc((fl_strlen(nom)+1)*sizeof(flrn_char));
    memset(result, 0, fl_strlen(nom)+1);
    *result='\0';
    if (nom==NULL) return result;
    buf1=fl_strchr(nom,fl_static('('));
    if (buf1) {
       buf1++;
       buf2=fl_strrchr(buf1, fl_static(')'));
       if (buf2==NULL) buf2=fl_strchr(buf1, fl_static('\0'));
       fl_strncpy(result, buf1, buf2-buf1);
    } else {
      buf1=fl_strchr(nom, fl_static('<'));
      if (buf1==NULL) { fl_strcpy(result,nom); return result; }
      fl_strncpy(result, nom, buf1-nom);
      buf2=fl_strrchr(nom, fl_static('>'));
      if (buf2!=NULL) fl_strcat(result, buf2+1);
    }
    return result;
}

/* Écriture de la date a partir d'une chaine de date */
/* obsolete */
#if 0
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
#endif



/* Fonctions pour estimer la taille d'une chaine a afficher (avec les tab) */
/* on suppose avoir déjà transcrit la chaîne en format "terminal" */
int str_estime_width (char *la_chaine, int tmp_col, size_t tai_chaine) {
  char *buf = la_chaine;
  int w,icol=tmp_col;
  size_t i=0,l=1;

  if (tai_chaine==(size_t)-1) tai_chaine=strlen(la_chaine);
  while (i<tai_chaine) {
    if (*buf=='\t') {
      tmp_col=(tmp_col/Screen_Tab_Width+1)*Screen_Tab_Width; 
      buf++;
      i++;
    } else {
      width_termchar(buf,&w,&l);
      buf+=l;
      i+=l;
      tmp_col+=w;
    }
  }
  return tmp_col-icol;
}

/* renvoie le nombre de caractères pour la taille totale */
/* flag = 0 : au plus près ; = 1 : au plus près blanc */
size_t to_make_width (char *la_chaine, int col_total, int *col, int flag) {
  char *last_s=NULL, *buf=la_chaine;
  int save_col=*col,act_col=*col,w;
  size_t l=1;

  if (*buf=='\0') return 0;
  while (act_col<col_total) {
    if (isblank(*buf)) { last_s=buf; save_col=act_col; }
    if (*buf=='\t') {
	if ((act_col/Screen_Tab_Width+1)*Screen_Tab_Width>col_total) break;
	act_col=(act_col/Screen_Tab_Width+1)*Screen_Tab_Width;
	buf++;
    } else {
      width_termchar(buf,&w,&l);
      if (w+act_col>col_total) break;
      act_col+=w;
      buf+=l;
    }
    if (*buf=='\0') {
	*col=act_col;
	return strlen(la_chaine);
    }
  }
  if (isblank(*buf)) {
     *col=act_col;
     return buf-la_chaine;
  } else 
     if ((last_s) && (flag)) {
	*col=save_col;
        return last_s-la_chaine;
     } else
       if ((*col) && (flag)) return 0; else {
	  *col=act_col;
          return buf-la_chaine;
       }
}

/* version de conversion */
size_t to_make_width_convert(flrn_char *la_chaine, int col_total,
	int *col, int flag) {
    flrn_char *last_s=NULL, *buf=la_chaine;
    int save_col=*col,act_col=*col,w;
    char conver[15],*cv;
    size_t l=1;

    if (*buf==fl_static('\0')) return 0;
    while (act_col<col_total) {
	if ((*buf==fl_static(' ')) || (*buf==fl_static('\t')))
	   { last_s=buf; save_col=act_col; }
        if (*buf==fl_static('\t')) {
            if ((act_col/Screen_Tab_Width+1)*Screen_Tab_Width>col_total) break;
	    act_col=(act_col/Screen_Tab_Width+1)*Screen_Tab_Width;
	    buf++;
	} else {
	    size_t ll;
	    int scol;
	    ll=next_flch(buf,0);
	    if ((int)ll<=0) ll=1;
	    cv=conver;
	    w = conversion_to_terminal(buf,&cv,14,ll);
	    scol=act_col;
	    while (*cv) {
               width_termchar(cv,&w,&l);
	       if (scol+w>col_total) break; 
	       scol+=w;
	       cv+=l;
	    }
            if (*cv) break; 
	    buf+=ll;
	    act_col=scol;
	}
	if (*buf==fl_static('\0')) {
	    *col=act_col;
	    return fl_strlen(la_chaine);
	}
    }
    if ((*buf==fl_static(' ')) || (*buf==fl_static('\t'))) {
	*col=act_col;
	return buf-la_chaine;
    } else
	if ((last_s) && (flag)) {
	    *col=save_col;
	    return last_s-la_chaine;
	} else
	    if ((*col) && (flag)) return 0; else {
		*col=act_col;
		return buf-la_chaine;
            }
}

/* format_flstring convertit une chaîne en chaîne affichable pour le
 * terminal, en la mettant dans un morceau de menu. Renvoi la largeur
 * de la chaîne. Les tabulations deviennent des espaces .
 * justify =0 : left   1 : right  2 : center */
int format_flstring (char *dest, flrn_char *src, int mwidth, size_t sdest,
	int justify) {
    int width=0;
    char *buf=dest;

    *buf='\0';
    while ((*src!=fl_static('\0')) && (width<mwidth) && (sdest>1)) {
	if ((*src==fl_static(' ')) || (*src==fl_static('\t'))) {
	    width++;
	    *(buf++)=' '; sdest--;
	    src++;
	    continue;
	}
	{
	    size_t ll;
	    int w;
	    ll=next_flch(src,0);
	    if ((int)ll<=0) ll=1;
	    conversion_to_terminal(src,&buf,sdest-1,ll);
	    src+=ll;
	    if (*buf=='\0') return width;
	    while (*buf) {
	       width_termchar(buf,&w,&ll);
	       if (w+width>mwidth) {
		   *buf='\0';
		   return width;
	       }
	       buf+=ll; sdest-=ll;
	       width+=w;
	    }
	}
   }
   *buf='\0';
   if ((width<mwidth) && (sdest>1) && (justify>0)) {
       int decalage;
       if (justify==1) 
	   decalage=mwidth-width;
       else 
	   decalage=(mwidth-width)/2;
       if (decalage>=sdest) decalage=sdest-1;
       memmove(dest+decalage,dest,buf-dest+1);
       memset(dest,(int)(' '),decalage);
   }
   return width;
}

int format_flstring_from_right (char *dest, flrn_char *src, int mwidth,
	size_t sdest, int justify) {
    int width=0;
    size_t idest=sdest+1;
    char *buf=dest+sdest;
    char trad1[15];
    flrn_char *prc=src+fl_strlen(src);
    char *dummy=&(trad1[0]);

    memset(dummy,0,15);
    *(buf--)='\0';
    while ((prc!=src) && (width<mwidth) && (sdest>1)) {
	if ((*prc==fl_static(' ')) || (*prc==fl_static('\t'))) {
	    width++;
	    *(buf--)=' '; sdest--;
	    prc--;
	    continue;
	}
	{
	    size_t ll;
	    int w;
	    ll=previous_flch(prc,0,prc-src);
	    if ((int)ll<=0) ll=1;
	    prc-=ll;
	    conversion_to_terminal(prc,&dummy,14,ll);
	    if (*dummy=='\0') break;
	    ll=strlen(dummy);
	    w=str_estime_width(dummy,0,ll);
	    /* FIXME : en char arrière */
	    if ((w+width>mwidth) || (ll>sdest-1)) {
		memmove(dest,buf,idest-sdest);
		return width;
	    }
	    buf-=ll; sdest-=ll;
	    memcpy(buf,dummy,ll);
	    width+=w;
	    dummy=&(trad1[0]);
	}
   }
   memmove(dest,buf,idest-sdest);
   if ((width<mwidth) && (sdest>1) && (justify>0)) {
       int decalage;
       if (justify==1) 
	   decalage=mwidth-width;
       else 
	   decalage=(mwidth-width)/2;
       if (decalage>=sdest) decalage=sdest-1;
       memmove(dest+decalage,dest,buf-dest+1);
       memset(dest,(int)(' '),decalage);
   }
   return width;
}


/* Copie d'une chaine dans un fichier pour l'éditeur, limitée... */
/* tmp_file=NULL : initialise... chaine=NULL : flush */
void copy_bout (FILE *tmp_file, flrn_char *flchaine) {
  static char ligne[100];
  char *chaine,*buf,*ptr;
  int resconv;
  static int col;
  size_t len1;

  if (tmp_file==NULL) {
    ligne[0]='\0';
    col=0;
    return;
  }
  if (flchaine==NULL) {
     fprintf(tmp_file, "%s\n", ligne);
     ligne[0]='\0';
     col=0;
     return;
  }
  resconv=conversion_to_editor(flchaine,&chaine,0,(size_t)(-1));
  ptr=chaine;
  while (*ptr) {
     buf=ptr;
     if ((ptr=strchr(ptr,'\n'))) {
        *(ptr++)='\0';
     }
     len1=to_make_width(buf,72,&col,1);
     if (len1==0) { /* On peut vouloir couper avant dans ce cas */
        char *tmp_string, *buf2;
        tmp_string=strrchr(buf,' ');
	if (tmp_string==NULL) tmp_string=strrchr(buf,'\t');
	else { buf2=strrchr(tmp_string,'\t');
	       if (buf2) tmp_string=buf2; }
        if (tmp_string) {
	   *(tmp_string++)='\0';
	   fprintf(tmp_file, "%s\n", ligne);
	   ligne[0]='\0';
	   col=str_estime_width(tmp_string,0,(size_t)-1);
	   memmove(ligne,tmp_string,strlen(tmp_string)+1);
	   if (ptr) *(--ptr)='\0'; 
	   ptr=buf; continue;
	} else {
	   fprintf(tmp_file, "%s\n", ligne);
	   ligne[0]='\0';
	   col=0;
	   len1=to_make_width(buf,72,&col,1);
	   if (len1==0) { /* impossible ! */
	       if (ptr) *(--ptr)='\n';
	       fprintf(tmp_file, "%s\n", buf);
	       if (resconv==0) free(chaine);
	       return;
	   }
	}
     }
     strncat(ligne,buf,len1);
     if ((ptr) || (len1!=strlen(buf))) {
        fprintf(tmp_file, "%s\n", ligne);
        ligne[0]='\0';
        col=0;
	if (len1!=strlen(buf)) {
           if (ptr) *(--ptr)='\n';
	   ptr=buf+len1;
	}
     } else break;
  }
  if (resconv==0) free(chaine);
}

/* Translation des sequences d'echappement dans Copy_format. On peut
   reecrire sur la même chaîne */
static int translate_escape_seq(flrn_char *dst, flrn_char *src) {
   if ((src==NULL) || (dst==NULL)) return -1;
   while (*src) {
     if ((*dst=*(src++))!=fl_static('\\')) {
        dst++;
     } else 
     switch (*(src++)) {
         case fl_static('a') : *(dst++)=fl_static('\a'); break;
         case fl_static('b') : *(dst++)=fl_static('\b'); break;
         case fl_static('f') : *(dst++)=fl_static('\f'); break;
         case fl_static('n') : *(dst++)=fl_static('\n'); break;
         case fl_static('r') : *(dst++)=fl_static('\r'); break;
         case fl_static('t') : *(dst++)=fl_static('\t'); break;
         case fl_static('v') : *(dst++)=fl_static('\v'); break;
         case fl_static('\\') : *(dst++)=fl_static('\\'); break;
	 default : dst++; *(dst++)=fl_static('?'); break;
     }
   }
   *dst=fl_static('\0');
   return 0;
}

/* Copie d'une chaine formatée de headers dans un fichier... */
/* Et ca va marcher, parce qu'on y croit...  */
/* Mais si on veut copier dans une chaine ? */
/* Je passe aussi une chaine avec la taille limite... */
/* On suppose chaine non NULL */
void Copy_format (FILE *tmp_file, flrn_char *chaine, Article_List *article,
                  flrn_char *result, size_t len) {
   flrn_char *att, *ptr_att;
   flrn_char *buf;
   size_t len2=0;

   att=safe_flstrdup(chaine);
   if (article) if (article->headers==NULL) return; /* Beurk ! */
   if (tmp_file) copy_bout(NULL,NULL); else result[0]=fl_static('\0');
   ptr_att=att;
   while (ptr_att && (*ptr_att)) {
     buf=fl_strchr(ptr_att,fl_static('%'));
     if (buf) *buf=fl_static('\0');
     /* on copie pour faire marcher les sequences d'echappement */
     if (translate_escape_seq(ptr_att, ptr_att)<0) {
         /* ah, y'a eu un bug, pas logique, normalement il n'y a pas
	      de '%' */
        if (debug) fprintf (stderr,"Bug dans Copy_format : ne peut faire la traduction de %s\n", fl_static_rev(chaine));
     }
     if (buf==NULL) {
       if (tmp_file) {
          copy_bout(tmp_file,ptr_att);
       }
       else { 
          fl_strncat(result,ptr_att,len2); 
	  len2+=fl_strlen(ptr_att);
	  if (len2>=len) { result[len]=fl_static('\0');
	                   free(att); return; } 
	  result[len2]=fl_static('\0');
       }
       break;
     } else {
       if (tmp_file) {
          copy_bout(tmp_file,ptr_att);
       }
       else { 
          fl_strncat(result,ptr_att,len2); 
	  len2+=strlen(ptr_att); 
	  if (len2>=len) { result[len]=fl_static('\0'); free(att) ;return; } 
	  result[len2]=fl_static('\0');
       }
       ptr_att=(++buf);
       switch (*buf) {
         case fl_static('%') :
                    if (tmp_file) copy_bout(tmp_file,fl_static("%")); else
                    { strcat(result,fl_static("%")); len2++; 
		      if (len2>=len) { free(att); return; }
		      else result[len2]=fl_static('\0'); }
                    ptr_att++;
                    break;
         case fl_static('n') : { flrn_char *vrai_n;
                      ptr_att++;
		      if (article==NULL) break;
                      vrai_n=vrai_nom(article->headers->k_headers
                                                [FROM_HEADER]);
                      if (tmp_file) copy_bout(tmp_file,vrai_n); else
                      { fl_strncat(result,vrai_n,len2); 
			len2+=strlen(vrai_n); 
		        if (len2>=len) { result[len]=fl_static('\0');
			                 free(att); 
			                 free(vrai_n); return; } else 
			result[len2]=fl_static('\0'); }
                        free(vrai_n);
                        break;
                      }
         case fl_static('i') : { flrn_char *msgid;
                      ptr_att++;
		      if (article==NULL) break;
	              msgid=safe_flstrdup(fl_static_tran(article->msgid));
                      if (tmp_file) copy_bout(tmp_file,msgid); else
                      { fl_strncat(result,msgid,len2); len2+=fl_strlen(msgid); 
		        if (len2>=len) { result[len]=fl_static('\0');
			                 free(att); free(msgid); 
			                 return; } else 
			result[len2]=fl_static('\0'); }
		      free(msgid);
                      break;
		    }
         case fl_static('C') : { 
		      flrn_char *num=safe_malloc((sizeof(int)*3+2)*
			                         sizeof(flrn_char));
		      fl_snprintf(num,sizeof(int)*3+1,fl_static("%d"),
			      article ? article->numero : 0);
                      if (tmp_file) copy_bout(tmp_file,num); else
                      { fl_strncat(result,num,len2); len2+=fl_strlen(num); 
		        if (len2>=len) { result[len]=fl_static('\0');
			               free(att); free(num); 
			               return; } else 
			result[len2]=fl_static('\0'); }
		      free(num);
                      ptr_att++;
                      break;
		    }
         case fl_static('g') : { flrn_char *bla;
	              flrn_char *str;
		      bla=truncate_group(Newsgroup_courant->name,1);
		      str=safe_flstrdup(bla);
                      if (tmp_file) copy_bout(tmp_file,str); else
                      { fl_strncat(result,str,len2); len2+=fl_strlen(str); 
		        if (len2>=len) { result[len]=fl_static('\0');
			               free(att); free(str); return; }
			else result[len2]=fl_static('\0'); }
		      free(str);
                      ptr_att++;
                      break;
		    }
         case fl_static('G') : 
		  { flrn_char *str=safe_flstrdup(Newsgroup_courant->name);
                      if (tmp_file) copy_bout(tmp_file,str); else
                      { fl_strncat(result,str,len2); len2+=strlen(str); 
		        if (len2>=len) { result[len]=fl_static('\0');
			               free(att); free(str); return; } 
			else result[len2]=fl_static('\0'); }
		      free(str);
                      ptr_att++;
                      break;
		    }
	 case fl_static('{') : /* un header du message */
	 case fl_static('`') : /* une commande à insérer */
	 case fl_static('[') : /* une commande de script */
	    /* ces trois cas devraient être unifiés */
	            if (*buf==fl_static('{'))
	            { buf++;
		      ptr_att=fl_strchr(buf,fl_static('}')); 
		      if (ptr_att) {
		         int len3,tofree;
			 flrn_char *str;
		         *ptr_att=fl_static('\0');
			 len3=fl_strlen(buf);
			 ptr_att++;
			 if (article==NULL) break;
			 str = get_one_header(article, Newsgroup_courant,
				                buf,&tofree);
			 if (str) {
			       if (tofree==0) str=safe_flstrdup(str);
                               if (tmp_file) copy_bout(tmp_file,str); else
                               { strncat(result,str,len2); len2+=strlen(str);
				 if (len2>=len) { result[len]=fl_static('\0');
				             free(str); free(att); return; }
				 else result[len2]=fl_static('\0'); }
			       free(str);
			 }
		         break;
	  	      } else ptr_att=buf; /* -> default */
		   } else if (*buf==fl_static('`'))
	 	   {  buf++;
		      ptr_att=fl_strchr(buf,fl_static('`'));
		      if (ptr_att) {
		        /* TODO : deplacer ca et unifier */
			/* avec display_filter_file */
			FILE *file;
			int fd;
			char name[MAX_PATH_LEN];
			int resconv;
			flrn_char *trad;
		        *ptr_att=fl_static('\0');
			fd=Pipe_Msg_Start(0,1,buf,name);
			*ptr_att=fl_static('`');
			ptr_att++;
			if (fd<0) break;
			Pipe_Msg_Stop(fd);
			file=fopen(name,"r");
			if (file == NULL) break;
			while (fgets(tcp_line_read, MAX_READ_SIZE-1, file)) {
			   resconv=conversion_from_file(tcp_line_read,&trad,0,(size_t)(-1));
			   if (tmp_file) copy_bout(tmp_file,trad);
			    else {
			      fl_strncat(result,trad,len2);
			      len2+=strlen(trad);
			      if (len2>=len) { result[len]=fl_static('\0'); 
				             if (resconv==0) free(trad);
					     free(att);
#ifdef USE_MKSTEMP
				             unlink(name);
#endif
				             return; }
			      else result[len2]=fl_static('\0');
			    }
			    if (resconv==0) free(trad);
			}
#ifdef USE_MKSTEMP
			unlink (name);
#endif
			break;
		      } 
		      ptr_att=buf; /* -> default */
		   } else {
		      buf++;
		      ptr_att=fl_strchr(buf,fl_static(']'));
		      if (ptr_att) {
#ifdef USE_SLANG_LANGUAGE
		         flrn_char *str=NULL;
			 *ptr_att=fl_static('\0');
			 source_SLang_string(buf, &str);
			 *ptr_att=fl_static(']');
			 ptr_att++;
			 if (str!=NULL) {
                            if (tmp_file) copy_bout(tmp_file,str); else
                            { fl_strncat(result,str,len2);
				len2+=strlen(str); if
	                       (len2>=len) { result[len]=fl_static('\0');
				     free(str);
			             free(att); return; } else 
			      result[len2]=fl_static('\0'); }
			    free(str);
			 }
			 break;
#endif
	              }
		      ptr_att=buf; /* -> default */
		   }
         default : { flrn_char save=*(buf+1);
		     if (debug) fprintf(stderr, "Mauvaise formatage : %%%s\n",
			     fl_static_rev(buf));
		     *(buf+1)=fl_static('\0');
		     *(--buf)=fl_static('%');
                      if (tmp_file) copy_bout(tmp_file,buf); else
                      { strncat(result,buf,len2); len2+=strlen(buf); if
	                (len2>=len) { result[len]=fl_static('\0');
			            free(att); return; } else 
			result[len2]=fl_static('\0'); }
                     *(++ptr_att)=save;
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
flrn_char * Prepare_summary_line(Article_List *article, 
	flrn_char *previous_subject, int level, flrn_char *out, size_t outlen,
	int out_width, int ini_col, int by_msgid, int with_flag) {
    int deb_num, deb_nom, tai_nom, deb_sub, tai_sub, deb_dat, deb_rep;
    /* tout ces trucs sont de la taille des colonnes */
    flrn_char *flbuf;
    char buf2[15], *buf;
    char *subject;
    flrn_char *out_ptr=out;
    size_t len_width,tmplen;
   
    memset(out,0,outlen);
    if ((article->headers==NULL) ||
	(article->headers->k_headers_checked[FROM_HEADER] == 0) ||
	(article->headers->k_headers_checked[SUBJECT_HEADER] == 0) ||
	((article->headers->k_headers_checked[DATE_HEADER] == 0)  &&
	 Options.date_in_summary))
      cree_header(article,0,0,by_msgid);
    if (article->headers==NULL) return NULL;
    if (article->headers->k_headers[FROM_HEADER]==NULL) return out;
    if (article->headers->k_headers[SUBJECT_HEADER]==NULL) return out;
    deb_num=ini_col; deb_nom=6+with_flag+ini_col;
    deb_rep=out_width-8;
    if(Options.date_in_summary)
      deb_dat=deb_rep-13; else deb_dat=deb_rep;
    tai_nom=(deb_dat-deb_nom)/3;
    deb_sub=deb_nom+tai_nom+2;
    tai_sub=deb_dat-deb_sub-1;
    if ((tai_nom<=0) || (tai_sub<=0)) return out; /* TROP PETIT */
    
    if (with_flag)
        sprintf(buf2,"%c%5d",
	      (article->flag&FLAG_READ)?' ':'*', article->numero);
    else
	sprintf(buf2,"%5d",article->numero);
    tmplen=(size_t)fl_snprintf(out_ptr,8,fl_static("%-*.*s "),6,
	    6,buf2);
    if (tmplen==(size_t)(-1)) return out;
    out_ptr+=tmplen;
    flbuf=vrai_nom(article->headers->k_headers[FROM_HEADER]);
    len_width=to_make_width_convert(flbuf,deb_nom+tai_nom,&deb_nom,0);
    if (tmplen+len_width>=outlen) return out;   /* TROP PETIT */
    fl_strncpy(out_ptr,flbuf,len_width); out_ptr+=len_width; 
    *out_ptr=fl_static('\0'); free(flbuf);
    tmplen+=len_width;
    if (tmplen+(deb_sub-deb_nom)>=outlen) return out;   /* TROP PETIT */
    fl_memset(out_ptr,fl_static(' '),deb_sub-deb_nom);
    out_ptr+=deb_sub-deb_nom; tmplen+=deb_sub-deb_nom;
    *out_ptr=fl_static('\0');
    subject=article->headers->k_headers[SUBJECT_HEADER];
    if (!fl_strncasecmp(subject,fl_static("Re: "),4)) subject +=4;
    if (previous_subject && 
	    !fl_strncasecmp(previous_subject,fl_static("Re: "),4))
      previous_subject +=4;
    if(previous_subject && !fl_strcmp(subject,previous_subject))
    {
	if (tmplen+tai_sub+1>=outlen) return out;  /* T.P */
	if (level && (level < tai_sub-1)) {
	  sprintf(out_ptr,"%*s%-*s",level,"",tai_sub-level+1,".");
	  out_ptr += tai_sub+1; tmplen+=tai_sub+1;
	}
    } else {
	len_width=to_make_width_convert(subject,deb_sub+tai_sub,&deb_sub,0);
	if (tmplen+len_width>=outlen) return out;   /* TROP PETIT */
	fl_strncpy(out_ptr,subject,len_width); out_ptr+=len_width;
	*out_ptr=fl_static('\0'); tmplen+=len_width;
	if (tmplen+(deb_dat-deb_sub)>=outlen) return out;   /* TROP PETIT */
        fl_memset(out_ptr,fl_static(' '),deb_dat-deb_sub);
	out_ptr+=deb_dat-deb_sub; tmplen+=deb_dat-deb_sub;
        *out_ptr=fl_static('\0');
    }
    if(Options.date_in_summary) {
      if (tmplen+13>=outlen) return out;   /* TROP PETIT */
      if (article->headers->date_gmt)  {
        buf=safe_strdup(ctime(&article->headers->date_gmt)+4);
	fl_snprintf(out_ptr,14,fl_static("%-*.*s"),13,12,buf);
	free(buf);
      } else
        fl_snprintf(out_ptr,14,fl_static("%-*.*s"),13,12,"");
      out_ptr += 13;
      tmplen+=13;
    }
    if (tmplen+8>=outlen) return out;
    if (article->parent!=0) {
       if (article->parent>0) {
	   fl_snprintf(out_ptr,8,fl_static("[%5d]"), article->parent);
	 }
         else fl_snprintf(out_ptr,8,fl_static("%-7s"),
		 fl_static("[  ?  ]"));
    }
         else fl_snprintf(out_ptr,outlen,fl_static("%7.7s"),fl_static(""));
    return out;
}

/* parse la ligne de From:... dans un format pour l'instant unique et */
/* forum-like...						      */
/* ajoute le resultat à str */
/* on suppose qu'il y a de la place (2*strlen(from_line))+3 */
/* (plus strlen(sender) ) */

void ajoute_parsed_from(flrn_char *str, flrn_char *from_line, 
	flrn_char *sender_line) {
   flrn_char *buf,*buf2,*bufs;
   size_t siz_login;
   buf=vrai_nom(from_line);
   fl_strcat(str,buf);
   fl_strcat(str,fl_static(" ("));
   free(buf);
   bufs=str+fl_strlen(str); /* sauvegarde du nom entre parentheses */
   buf=fl_strchr(from_line,fl_static('<'));
   if (buf) {
      buf2=fl_strchr(buf,fl_static('@'));
      if (buf2) 
        fl_strncat(str,buf+1,buf2-buf-1);
      else fl_strcat(str,buf+1);
   } else {
     buf2=fl_strchr(from_line,fl_static('@'));
     if (buf2)
       fl_strncat(str,from_line,buf2-from_line);
     else fl_strcat(str,from_line);
   }
   siz_login=fl_strlen(bufs);
   fl_strcat(str,fl_static(")"));
   if (sender_line) {
      /* on ajoute [sender]  si le login est différent */
      buf=fl_strchr(sender_line,fl_static('@'));
      if ((buf) && ((siz_login!=(buf-sender_line)) ||
                    (fl_strncmp(bufs,sender_line,siz_login)))) {
	 fl_strcat(str,fl_static(" ["));
	 fl_strncat(str,sender_line,buf-sender_line);
	 fl_strcat(str,fl_static("]"));
      }
   }
}
