/* flrn v 0.1                                                           */
/*              post.c              28/11/97                            */
/*                                                                      */
/* Gestion des posts...                                                 */
/* Ce fichier ne fait pas appel à slang...				*/
/*                                                                      */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <ctype.h>
#include <sys/stat.h>

#include "flrn.h"
#include "flrn_slang.h"
#include "art_group.h"
#include "group.h"
#include "flrn_string.h"
#include "post.h"
#include "options.h"
#include "flrn_tcp.h"
#include "flrn_files.h"
#include "flrn_shell.h"
#include "flrn_format.h"
#include "tty_display.h"
#include "tty_keyboard.h"
#include "flrn_menus.h"
#include "rfc2047.h"
#include "flrn_regexp.h"

#define DEFAULT_SIZE_LINE 72

extern char short_version_string[];

Lecture_List *Deb_article;
Lecture_List *Deb_body;
Article_List *Pere_post;
int par_mail, supersedes;

Post_Headers *Header_post;

/* Obtention du sujet d'un post. Pas de formatage pour l'instant */
/* retour : 0 en cas de succes, -1 en cas d'annulation.		 */
static int get_Sujet_post(char *str) {
   int res;
   
   Cursor_gotorc(2,0);
   Screen_write_string("Sujet : ");
   do {
     str[0]='\0';
     res=getline(str,MAX_SUJET_LEN,2,8); 
   } while (res==-2);   /* un backspace en debut de ligne ne fait rien */
   return res; 
}

/* recalcule la colonne courante, avec un charactère de moins si flag=1 */
/* Renvoie -1 si la ligne est vide et flag=1 */
static int recalc_col (char *ligne, int flag) {
   int ret=0;

   if (flag && (*ligne=='\0')) return -1;
   while (*(ligne+flag)) {
      if (*ligne==9) ret=(ret/Screen_Tab_Width+1)*Screen_Tab_Width; else
	 ret++;
      ligne++;
   }
   return ret;
}

/* Copie dans le fichier temporaire le message tel qu'on suppose qu'il   */
/* sera.								 */
void Copie_prepost (FILE *tmp_file, Lecture_List *d_l, int place, int incl) {
   Header_List *liste;
   Lecture_List *lecture_courant;
  
   if (Options.edit_all_headers) {
      int i;
      for (i=0; i<NB_KNOWN_HEADERS; i++) {
	if (i==SENDER_HEADER) continue;
	if (i==DATE_HEADER) continue;
	if (i==LINES_HEADER) continue;
	if (i==XREF_HEADER) continue;
	if (i==X_NEWSREADER_HEADER) continue;
	if (i==MESSAGE_ID_HEADER) continue;
	if ((i==TO_HEADER) && (!par_mail)) continue;
	if ((i==CC_HEADER) && (!par_mail)) continue;
	if ((i==BCC_HEADER) && (!par_mail)) continue;
	fprintf(tmp_file, "%s ", Headers[i].header);
	if (Header_post->k_header[i])
	   fputs(Header_post->k_header[i],tmp_file);
	putc('\n',tmp_file);
      }
      liste=Header_post->autres;
      while (liste) {
	 fprintf(tmp_file, "%s\n", liste->header);
	 liste=liste->next;
      }
   } else {
      fputs("Groupes: ",tmp_file);
      if (Header_post->k_header[NEWSGROUPS_HEADER]) 
         fputs(Header_post->k_header[NEWSGROUPS_HEADER],tmp_file);
      putc('\n',tmp_file);
      fputs("Sujet: ",tmp_file);
      if (Header_post->k_header[SUBJECT_HEADER]);
         fputs(Header_post->k_header[SUBJECT_HEADER],tmp_file);
      putc('\n',tmp_file);
      liste=Header_post->autres;
      while (liste) {
	 if (liste->num_af) fprintf(tmp_file, "%s\n", liste->header);
	 liste=liste->next;
      }
   }
   fprintf(tmp_file, "\n");
   if (Pere_post && ((incl==1) || ((incl==-1) && (Options.include_in_edit)))) { 
       if (!supersedes) Copy_format (tmp_file,Options.attribution,Pere_post,NULL,0);
       Copy_article(tmp_file,Pere_post, 0, (supersedes ? NULL : Options.index_string));
       fprintf(tmp_file,"\n");
   }
   lecture_courant=Deb_body;
   while (lecture_courant) {
      if (lecture_courant==d_l)  
	 lecture_courant->size=place;
      lecture_courant->lu[lecture_courant->size]='\0';  /* Par précaution */
      fprintf(tmp_file, "%s", lecture_courant->lu);
     /* J'utilise fprintf plutôt que fwrite par principe : fwrite est */
     /* a priori pour des fichiers binaires (et puis, j'ai commencé   */
     /* avec fprintf). Mais bon, on pourrait peut-être changer...     */
      if (lecture_courant==d_l) break;
      lecture_courant=lecture_courant->suivant;
   } 
}

/* Lecture du fichier une fois l'édition finie */
/* Renvoie -1 si le fichier est VRAIMENT trop moche */
int Lit_Post_Edit (FILE *tmp_file, Lecture_List **d_l, int *place) {
   char buf[513]; /* On suppose qu'un ligne ne peut pas faire plus de 512 */
   char *buf2, *buf3;
   char **header_courant=NULL;
   int header_connu;
   int taille;
   Lecture_List *lecture_courant;
   int headers=1,read_something=0, i;
   Header_List *liste, *last_liste=NULL, **unk_header_courant=NULL;

   lecture_courant=Deb_body;
   lecture_courant->size=0;
   lecture_courant->lu[0]='\0';
   while (fgets(buf,513,tmp_file)) {
      if (strlen(buf)==512) return -1;
      if (!read_something && (buf[0]=='\n')) continue; /* J'ignore les */
      		/* les lignes en debut fichier */
      read_something=1;
      if ((buf[0]=='\n') && headers) {headers=0; continue;}
      if (headers) {
	 if (isblank(buf[0])) {
	   if (header_courant) {
	     if (*header_courant) {
	      *header_courant=safe_realloc(*header_courant, (strlen(*header_courant)+3+strlen(buf))*sizeof(char));
	      strcat(*header_courant, "\n");
	      strcat(*header_courant, buf);
	      (*header_courant)[strlen(*header_courant)-1]='\0';
	     } else 
	       *header_courant=safe_strdup(buf);
	   } else headers=0; /* On recopiera après */
	 } else {
           buf2=strchr(buf,':');
	   buf3=strpbrk(buf, " \t");
	   if ((buf2==NULL) || ((buf3) && (buf3-buf2<0))) headers=0; else {
	      header_connu=1;
	      for (i=0; i<NB_KNOWN_HEADERS; i++) 
	        if (strncasecmp(buf,Headers[i].header,Headers[i].header_len)==0)
		{
		  header_courant=&(Header_post->k_header[i]); 
		  break;
		}
              if (i==NB_KNOWN_HEADERS) {
	         if ((strncasecmp(buf,"sujet:", 6)==0) && 
		     !Options.edit_all_headers)
		    header_courant=&(Header_post->k_header[SUBJECT_HEADER]); 
		 else if ((strncasecmp(buf,"groupes:", 8)==0) && 
		     !Options.edit_all_headers)
		    header_courant=&(Header_post->k_header[NEWSGROUPS_HEADER]); 
		 else {
		   header_connu=0;
		   taille=buf2-buf;
                   liste=Header_post->autres;
		   while (liste) {
		      if (strncasecmp(buf, liste->header, taille)==0) break;
		      last_liste=liste;
		      liste=liste->next;
		   }
		   if (liste) {
		      header_courant=&(liste->header); 
		      if (last_liste) unk_header_courant=&(last_liste->next);
		       else unk_header_courant=&(Header_post->autres);
		   } else
		   if (last_liste) {
		      last_liste->next=safe_malloc(sizeof(Header_List));
		      last_liste->next->next=NULL;
		      last_liste->next->header=NULL;
		      header_courant=&(last_liste->next->header);
		      unk_header_courant=&(last_liste->next);
		   } else {
		      Header_post->autres=safe_malloc(sizeof(Header_List));
		      Header_post->autres->next=NULL;
		      Header_post->autres->header=NULL;
		      header_courant=&(Header_post->autres->header);
		      unk_header_courant=&(Header_post->autres);
		   }
		   (*unk_header_courant)->num_af=1;
		 }
	      }
	      buf2++;
	      while ((*buf2) && isblank(*buf2)) buf2++;
	      if ((*buf2=='\0') || (*buf2=='\n')) {
		  if (*header_courant) { 
		     free(*header_courant);
		     *header_courant=NULL;
		     if (header_connu==0) {
		        liste=(*unk_header_courant)->next;
			free(*unk_header_courant);
			*unk_header_courant=liste;
	             }
	          }
		  continue;
	      }
	      *header_courant=safe_realloc(*header_courant,(strlen(header_connu ? buf2 : buf)+1)*sizeof(char));
	      strcpy(*header_courant, (header_connu ? buf2 : buf));
	      (*header_courant)[strlen(*header_courant)-1]='\0';
	   }
	}
      }
      if (headers==0) {
        if (header_courant) 
	    header_courant=NULL;
        str_cat(&lecture_courant, buf);
      }
   }
   *d_l=lecture_courant;
   *place=lecture_courant->size;
   return 0;
}

/* Summon_Editor : programme de préparation, d'appel, et de relecture    */
/* de l'éditeur.							 */
/* Renvoie -1 en cas de bug majeur.					 */
/* Renvoie 2 en cas de message non modifié				 */
/* Renvoie 1 en cas de message vide					 */
/* Renvoie 0 si pas (trop) de problèmes...				 */
static int Summon_Editor (Lecture_List **d_l, int *place, int incl) {
   FILE *tmp_file;
   int res;
   struct stat before, after;

   free_text_scroll();
   Cursor_gotorc(1,0);
   Screen_erase_eos();
   tmp_file=open_postfile(TMP_POST_FILE,"w");
   if (tmp_file==NULL) {
      Screen_write_string("Echec dans l'ouverture du fichier temporaire.");
      return -1;
   }
   Copie_prepost (tmp_file, *d_l, *place, incl);
   fclose(tmp_file);
   if (stat_postfile(TMP_POST_FILE,&before)<0) {
     /* ca doit jamais arriver */
     Screen_write_string("Echec dans la création du fichier temporaire.");
     return -1;
   }
   res=Launch_Editor(0);  
   if (res==-1) {
      Screen_write_string("Echec dans le lancement de l'éditeur.");
      return -1;
   }
   if (stat_postfile(TMP_POST_FILE,&after)<0) {
     /* ca doit jamais arriver */
     Screen_write_string("Et le fichier temporaire louze.");
     return -1;
   }
   if (after.st_size == 0) {
     Screen_write_string("Fichier temporaire vide.");
     return 1;
   }
   if (after.st_mtime == before.st_mtime) {
     Screen_write_string("Message non modifié.");
     return 2;
   }
   tmp_file=open_postfile(TMP_POST_FILE,"r");
   if (tmp_file==NULL) {
      Screen_write_string("Echec dans la réouverture du fichier temporaire");
      return -1;
   }
   res=Lit_Post_Edit(tmp_file, d_l, place);
   fclose (tmp_file);
   return res;
}


/* Prend le message à poster à partir d'un fichier...			 */
/* renvoie 0 en cas d'echec...						 */
int charge_postfile (char *str) {
   int res;
   FILE *post_file;
   int size=0;
   Lecture_List *lecture_courant;

   Deb_body=lecture_courant=alloue_chaine();
   post_file=fopen(str,"r");
   if (post_file==NULL) {
       Aff_error("Echec en ouverture du fichier");
       return 0;
   }
   res=Lit_Post_Edit(post_file,&lecture_courant, &size);
   if (res<0) {
       Aff_error("Fichier incompréhensible...");
       return 0;
   }
   fclose(post_file);
   return 1;
}

/* Analyse le debut de la ligne tapée (pour voir les commandes spéciales */
/* Pour l'instant, on se contente de renvoyer 1 en cas de ".\n"		 */
/* On appelle aussi Summon_Editor en cas de "~e\n", puis on renvoie 2    */
/* ou 3 si le message est vide */
static int analyse_ligne (Lecture_List **d_l, int *place) {
   Lecture_List *sd_l=*d_l;
   int splace=*place;
   char debut[3];
   int i, res;
   
   for (i=0; i<3; i++) {
       debut[i]=sd_l->lu[splace];
       if (debut[i]=='\n') break;
       splace++; if (splace>=sd_l->size) { splace=0; sd_l=sd_l->suivant; }
   }

   if (strncmp(debut,".\n",2)==0) return 1;
   if ((strncmp(debut,"~e\n",3)==0) || (strncmp(debut,"~E\n",3)==0)) {
      res=Summon_Editor(d_l, place, (debut[1]=='E'));
      if ((res>=0)) return (res==1)?3:2; else return -1;
   }
   return 0;
}

/* Obtention du corps du post.					*/
/* La gestions des points doubles ne se fait pas.		*/
static int Screen_Start;

static int get_Body_post() {
   int key, act_col=0, act_row=4, size_debligne, fin=0;
   char chr;
   File_Line_Type *last_line=NULL;
   char *ligne_courante, *ptr_ligne_cou;
   Lecture_List *lecture_courant, *debut_ligne;
   int already_init=0, incl_in_auto=-1;
   int empty=1, size_max_line;
   int screen_start=0;

   Screen_Start=0;
   Deb_body=lecture_courant=debut_ligne=alloue_chaine();
   if (Options.auto_edit) {
      int place, res;
     
      place=res=0;
      while (res>=0) {
        res=Summon_Editor(&lecture_courant,&place, incl_in_auto);
	if ((incl_in_auto==0) && (res==2)) res=0; 
	    		     /* C'est au moins la seconde édition, le */
	  		     /* message a deja ete modifié une fois...*/
	incl_in_auto=0;
	lecture_courant->size=place;
	lecture_courant->lu[place]='\0';
        if (res==0) 
	  while (res==0) {
  	    Cursor_gotorc(Screen_Rows-1,0);
	    Screen_write_string("(P)oster, (E)diter, (A)nnuler ? ");
            key=Attend_touche();
	    key=toupper(key);
	    if (KeyBoard_Quit || (key=='A')) 
               return 0;
	    if (key=='E') res=1;
	    if (key=='P' || key=='Y' || key=='\r' ) {
              if (lecture_courant->suivant) free_chaine(lecture_courant->suivant);
              return 1;
	   }
	}
        else {
	  if (res>0) return -1; /* C'est un message non valide */
	  Cursor_gotorc(2,0);
	  Screen_write_string("   ( erreur dans l'édition. option auto_edit ignorée )");
        }
      } 
   }

   Cursor_gotorc(4,0);
   size_debligne=0;
   size_max_line=(Screen_Cols > DEFAULT_SIZE_LINE ? Screen_Cols+1 : DEFAULT_SIZE_LINE+1);
   ptr_ligne_cou=ligne_courante=safe_malloc(size_max_line*sizeof(char));
   *ptr_ligne_cou='\0';

   do {
      if (act_col-Screen_Start>=Screen_Cols) {
        screen_start=Screen_Start;
        Screen_Start=act_col-Screen_Cols+1;
	if (screen_start!=Screen_Start) {
	  screen_start=Screen_Start;
	  Screen_set_screen_start(NULL,&screen_start);
	  if (last_line) {
            if (!already_init) {
              Init_Scroll_window(act_row-4,4,Screen_Rows-5);
              already_init=1;
            }
            Do_Scroll_Window(0,1);
	  }
          Cursor_gotorc(act_row,0);
          Screen_write_string(ligne_courante);
          Screen_erase_eol();
	}
      } else
      if (act_col-Screen_Start<=1) {
        screen_start=Screen_Start;
        if (act_col<Screen_Cols) Screen_Start=0; else
	   Screen_Start=act_col-Screen_Cols+1;
	if (screen_start!=Screen_Start) {
	  screen_start=Screen_Start;
	  Screen_set_screen_start(NULL,&screen_start);
	  if (last_line) {
            if (!already_init) {
               Init_Scroll_window(act_row-4,4,Screen_Rows-5);
               already_init=1;
            }
            Do_Scroll_Window(0,1);
	  }
          Cursor_gotorc(act_row,0);
          Screen_write_string(ligne_courante);
          Screen_erase_eol();
	}
      }
      key=Attend_touche();
      if (sigwinch_received || KeyBoard_Quit) {
           lecture_courant->lu[lecture_courant->size]='\0';
	   free_text_scroll();
	   sigwinch_received=0;
	   free(ligne_courante);
           return 0;
      }
      if (key=='\r') key='\n'; /* On ne VEUT pas distinguer ^J et ^M */
      if ((key==FL_KEY_BACKSPACE) || (key==21) || (key==23)) {
	int mottrouve=0;
	int lignetrouvee=0;
	int retour_enleve=0;
	do {
	  
	  if (retour_enleve) break;
	  chr=get_char(lecture_courant,1);
	  if ((key==23) && (mottrouve) && (isblank(chr))) break;
	  if ((chr=='\n') && ((last_line==NULL) || (lignetrouvee))) break;
	  mottrouve=(!isblank(chr)) || (chr=='\n');
	  lignetrouvee=1; /* on ne poursuit jamais au-dela d'une ligne */
	  retour_enleve=(chr=='\n');
	  if (chr=='\0') break;
          enleve_char(&lecture_courant);
	  if (chr==9)  
	      act_col=recalc_col(ligne_courante,1);
	    else act_col--;
	  if (act_col<0) {
	      Read_Line(ligne_courante, last_line);
	      ptr_ligne_cou=index(ligne_courante, '\0');
	      /* On enleve le dernier caractere de la ligne si on n'a pas */
	      /* enleve le saut de ligne (logique, non ?) */
	      if (chr!='\n')
	        *(--ptr_ligne_cou)='\0';
	      if (last_line->prev) 
	      {
		 last_line=last_line->prev;
	         Retire_line(last_line);
              } else
	      {
	         Retire_line(last_line);
		 last_line=NULL;
		 already_init=0;
	      }
	      Cursor_gotorc(act_row, 0);
	      Screen_erase_eol();
              act_col=recalc_col(ligne_courante, 0);
	      act_row--;
	      if (act_row<4) {
		 if (already_init) Do_Scroll_Window(-1,0);
		 act_row=4;
		 Cursor_gotorc(act_row,0);
	         Screen_write_string(ligne_courante);
	      }
	      if (chr=='\n') { 
	         /* on doit recalculer deb_line et size_deb_line */
		 debut_ligne=lecture_courant;
		 cherche_char(&debut_ligne,&size_debligne,'\n');
	      }
	  } else *(--ptr_ligne_cou)='\0';
	  Cursor_gotorc(act_row,act_col);
	  Screen_erase_eol();
	}
	while ((key!=FL_KEY_BACKSPACE));
	continue;
      }
      if (key>255) continue;
      if (key=='\n') {
	 last_line=Ajoute_line(ligne_courante);
	 ptr_ligne_cou=ligne_courante;
	 *ptr_ligne_cou='\0';
         act_row++; act_col=0;
	 ajoute_char(&lecture_courant, '\n');
	 if (act_row>=Screen_Rows) {
	    if (!already_init) {
                Init_Scroll_window(Screen_Rows-4,4,Screen_Rows-5);
	        already_init=1;
	    }
	    Do_Scroll_Window(1,1);
/* On efface la ligne devenue superflue */
	    Cursor_gotorc(Screen_Rows-1,0);
	    Screen_erase_eol();
	    act_row=Screen_Rows-1;
	 }
	 fin=analyse_ligne(&debut_ligne, &size_debligne);
	 if (fin==-1) {
	     Cursor_gotorc(2,0);
	     Screen_write_string(" Ligne trop longue !!! ");
	     sleep(1);
	     free(ligne_courante);
	     return 0;
	 }
	 if (fin==0) {  /* Si rien de special, on encaisse la ligne */
	   empty=0;
	   debut_ligne=lecture_courant;
           size_debligne=lecture_courant->size;
	 } else {
	   lecture_courant=debut_ligne;
	   lecture_courant->size=size_debligne;
	 }
	 if ((fin==2)||(fin==3)) {
	   empty = (fin==3);
	   Cursor_gotorc(2,0);
	   if (fin ==2) {
	     Screen_write_string(" (continue) ");
	   } else {
	     Screen_write_string(" (début) ");
	   }
	   fin=0;
	   act_row=4; act_col=0;
	   already_init=0;
	   last_line=NULL;
	   Screen_Start=screen_start=0;
	   Screen_set_screen_start(NULL,NULL);
	 }
         Cursor_gotorc(act_row, act_col);
      } else
      {
	 if ((key==4)&&(act_col==0)) {fin=1; continue;}
         if (key==9) act_col=(act_col/Screen_Tab_Width+1)*Screen_Tab_Width; 
	   else if (key>31) act_col++; else continue;
         Screen_write_char(key);
	 *(ptr_ligne_cou++)=key;
	 *ptr_ligne_cou='\0';
	 ajoute_char(&lecture_courant, key);
         if (act_col>=DEFAULT_SIZE_LINE) {
/* Cette configuration tordue de l'éditeur intégrée a été suggérée par APO */
	     char *beuh1;
	     int on_change=0;
	     beuh1=strrchr(ligne_courante,' ');
	     if (beuh1) {
	       char *beuh2;
	       *(beuh1++)='\0';
	       last_line=Ajoute_line(ligne_courante);
	       Cursor_gotorc(act_row,0);
	       Screen_write_string(ligne_courante);
	       Screen_erase_eol();
               beuh2=safe_strdup(beuh1);
	       strcpy(ligne_courante,beuh2);
	       ptr_ligne_cou=index(ligne_courante,'\0');
	       Cursor_gotorc(act_row+1,0);
	       Screen_write_string(ligne_courante);
	       act_col=ptr_ligne_cou-ligne_courante;
	       free(beuh2);
	       while (get_char(lecture_courant,1)!=' ') 
	           enleve_char(&lecture_courant);
	       enleve_char(&lecture_courant);
	       ajoute_char(&lecture_courant, '\n');
	       str_cat(&lecture_courant,ligne_courante);
	       on_change=1;
	    } else if (act_col>=Screen_Cols) {
	       last_line=Ajoute_line(ligne_courante);
	       ptr_ligne_cou=ligne_courante;
	       *ptr_ligne_cou='\0';
	       act_col=0;
	       on_change=1;
	    }
	    if (on_change) {
              act_row++;
	      if (act_row>=Screen_Rows) {
	         if (!already_init) {
                   Init_Scroll_window(Screen_Rows-4,4,Screen_Rows-5);
		   already_init=1;
	         }
	         Do_Scroll_Window(1,1);
/* On efface la ligne devenue superflue */
	         Cursor_gotorc(Screen_Rows-1,0);
	         Screen_write_string(ligne_courante);
	         Screen_erase_eol();
	         act_row=Screen_Rows-1;
	      }
              Cursor_gotorc(act_row,act_col);
	   }
         }
      }
   } while (!fin);
   lecture_courant->lu[lecture_courant->size]='\0';
   free_text_scroll();
   if (lecture_courant->suivant) free_chaine(lecture_courant->suivant);
   free(ligne_courante);
   return (empty)?0:1;
}

/* Regarde l'existence d'un groupe dans un header... essaie une correction */
/* sinon...								   */
static char *check_group_in_header(char *nom, int *copy_pre, char *header) {
   char *nom2=nom;
   Newsgroup_List *groupe;
   int key,alloue=0,ret,col,to_test=1;
   regex_t reg;
   Liste_Menu *lemenu;

   if (strlen(nom)>MAX_NEWSGROUP_LEN) return nom;
     /* Tant pis. Si le mec se fout de ma gueule, c'est réciproque */
   while (1) {
     if (to_test) {
       groupe=cherche_newsgroup(nom2,1,*copy_pre);
       if (groupe!=NULL) return nom2;
       if (*copy_pre) {
         groupe=cherche_newsgroup(nom2,1,0);
         if (groupe!=NULL) {
            *copy_pre=0;
	    return nom2;
         }
       }
       Cursor_gotorc(1,0);
       Screen_erase_eos();
     }
     Cursor_gotorc(Screen_Rows-2,0);
     Screen_erase_eol();
     Screen_write_string("Groupe inconnu dans ");
     Screen_write_string(header);
     Screen_write_string(" : ");
     Screen_write_string(nom2);
     Cursor_gotorc(Screen_Rows-1,0);
     Screen_erase_eol();
     Screen_write_string("(L)aisser,(S)upprimer,(R)emplacer,(M)enu ? ");
     key=Attend_touche();
     key=toupper(key);
     if (KeyBoard_Quit || (key=='S')) {
        if (alloue) free(nom2);
        return NULL;
     }
     if (key=='L') 
        return nom2;
     if ((key=='R') || (key=='M')) {
        Cursor_gotorc(Screen_Rows-1,0);
	if (!alloue) {
	  nom2=safe_malloc(MAX_NEWSGROUP_LEN);
	  alloue=1;
	}
	nom2[0]='\0';
	Screen_erase_eol();
	if (key=='R') {
	  if (*copy_pre) strcpy(nom2,Options.prefixe_groupe);
	  Screen_write_string("Nom du groupe : ");
	  Screen_write_string(nom2);
	  ret=getline(nom2,MAX_NEWSGROUP_LEN,Screen_Rows-1,16);
	  if (ret<0) {
	     strcpy(nom2,nom);
	     to_test=0;
	  } else to_test=1;
	  continue;
	} 
	/* key=='M' */
	col=(Options.use_regexp ? 9 : 14);
	Screen_write_string(Options.use_regexp ? "Regexp : " :
						 "Sous-chaîne : ");
	ret=getline(nom2,MAX_NEWSGROUP_LEN,Screen_Rows-1,col);
	if (ret<0) {
	   strcpy(nom2,nom);
	   to_test=0;
	   continue;
	}
	if (Options.use_regexp) {
	   char *mustmatch;
	   if (regcomp(&reg,nom2,REG_EXTENDED|REG_NOSUB)) {
	      strcpy(nom2,nom);
	      to_test=0;
	      continue;
	   }
	   mustmatch=reg_string(nom2,1);
	   if (mustmatch!=NULL) lemenu=menu_newsgroup_re(mustmatch, reg,
	     				1+((*copy_pre)*2));
	   else {
	     regfree(&reg);
	     strcpy(nom2,nom);
	     to_test=0;
	     continue;
	   }
	   free(mustmatch);
	   regfree(&reg);
	} else lemenu=menu_newsgroup_re(nom2,reg,((*copy_pre)*2));
	     /* On passe n'importe quoi pour reg */
	if (lemenu) {
	  if (lemenu->suiv==NULL) {
	     strcpy(nom2,((Newsgroup_List *) (lemenu->lobjet))->name);
	     return nom2;
	  }
	  else
	    groupe=Menu_simple(lemenu,NULL,Ligne_carac_du_groupe,NULL,"Quel groupe ?");
	  Libere_menu(lemenu); /* On ne libere PAS les noms */
	  	/* voir à prendre change_group ici, c'est la même fonction */
	} else groupe=NULL;
	if (groupe) {
	   strcpy(nom2,groupe->name);
	   *copy_pre=0;
	   return nom2;
	}
	strcpy(nom2,nom);
     }
     to_test=0;
   }
}
	

/* Prepare les headers pour le post */
/* Pour l'instant, on s'occupe juste de sender et de X-newsreader */
/* On pourrait aussi supprimer Control */
static void Format_headers() {
   int len1, len2, copy_pre, i,j;
   char *real_name=safe_strdup(flrn_user->pw_gecos), *buf, *buf2;
   static char *delim2=" ,;\t";

   buf=strchr(real_name,','); if (buf) *buf='\0';

   /* Sender */
   len1=257+strlen(flrn_user->pw_name);
   if (Options.post_name)
     len2=4+strlen(Options.post_name);
   else
     len2=4+strlen(real_name);
   Header_post->k_header[SENDER_HEADER]=
        safe_realloc(Header_post->k_header[SENDER_HEADER],(len1+len2)*sizeof(char));
   Get_user_address(Header_post->k_header[SENDER_HEADER]);
   strcat(Header_post->k_header[SENDER_HEADER]," (");
   if (Options.post_name)
     strcat(Header_post->k_header[SENDER_HEADER], Options.post_name);
   else
     strcat(Header_post->k_header[SENDER_HEADER], real_name);
   strcat(Header_post->k_header[SENDER_HEADER],")");
   if (strcmp(Header_post->k_header[SENDER_HEADER], 
              Header_post->k_header[FROM_HEADER])==0) {
      free(Header_post->k_header[SENDER_HEADER]);
      Header_post->k_header[SENDER_HEADER]=NULL;
   }

   /* X-NEWSREADER */
   Header_post->k_header[X_NEWSREADER_HEADER]=
        safe_realloc(Header_post->k_header[X_NEWSREADER_HEADER], (strlen(short_version_string)+1)*sizeof(char));
   strcpy(Header_post->k_header[X_NEWSREADER_HEADER], short_version_string);
   free(real_name);

   /* Newsgroups , Followup-To*/
   for (i=0; i<2; i++) {
     j=(i==0 ? NEWSGROUPS_HEADER : FOLLOWUP_TO_HEADER);
     if (Header_post->k_header[j]) {
       if ((j==FOLLOWUP_TO_HEADER) && (strcasecmp(Header_post->k_header[j],"poster")==0)) continue;
       buf=strtok(Header_post->k_header[j],delim2);
       real_name=NULL;
       len1=0;
       while (buf) {
         copy_pre=((Options.prefixe_groupe) && (strncmp(buf,Options.prefixe_groupe,strlen(Options.prefixe_groupe))!=0));
	 buf2=check_group_in_header(buf,&copy_pre,Headers[j].header);
	 if (buf2==NULL) {
	   buf=strtok(NULL,delim2);
	   continue;
	 }
         real_name=safe_realloc(real_name,len1+(copy_pre?strlen(Options.prefixe_groupe):0)+strlen(buf2)+2);
         if (len1==0) real_name[0]='\0';
         len1+=(copy_pre?strlen(Options.prefixe_groupe):0)+strlen(buf2)+1;
         if (copy_pre) strcat(real_name,Options.prefixe_groupe);
         strcat(real_name,buf2);
         strcat(real_name,",");
	 if (buf2!=buf) free(buf2);
         buf=strtok(NULL,delim2);
       }
       if (len1>0) real_name[len1-1]='\0'; /* Enlevons la dernière , */
       free(Header_post->k_header[j]);
       Header_post->k_header[j]=real_name;
    }
  }
}



/* Formatage du message à des fins de post */
/* On fait la gestion des points doubles   */
/* Si to_cancel n'est pas NULL, on rajoute le header Control */
/* ou supersedes, selon la valeur de supersedes */
static int Format_article(char *to_cancel) {
   Lecture_List *ecriture_courant, *lecture_courant;
   char *buf1;
   int place_lu=0, deb_ligne=1, i;
   Header_List *liste;

   if (debug) fprintf(stderr, "Appel a format_article\n");
   ecriture_courant=Deb_article=alloue_chaine();
  /* Ecriture des headers */
  /* Attention : ici les headers sont plutôt buggués : il faudrait */
  /* formater, et remplacer les \n par des \r\n... Mais bon...     */
   Format_headers();
   for (i=0; i<NB_KNOWN_HEADERS; i++) 
      if (Header_post->k_header[i]) {
	str_cat(&ecriture_courant, Headers[i].header);
	ajoute_char(&ecriture_courant, ' ');
	str_cat(&ecriture_courant, Header_post->k_header[i]);
	str_cat(&ecriture_courant, "\r\n");
      }
   liste=Header_post->autres;
   while (liste) {
      if ((strncasecmp(liste->header,"Control:",8)==0) ||
	  (strncasecmp(liste->header,"Also-Control:",13)==0) ||
	  (strncasecmp(liste->header,"Supersedes:",11)==0)) {
	  	liste=liste->next;
	  	continue; 
      }
      				/* pas abuser non plus */
      str_cat(&ecriture_courant, liste->header);
      str_cat(&ecriture_courant, "\r\n");
      liste=liste->next;
   }
   if (to_cancel) {
      if (supersedes) str_cat(&ecriture_courant, "Supersedes: ");
	else str_cat(&ecriture_courant, "Control: cancel ");
      str_cat(&ecriture_courant, to_cancel);
      str_cat(&ecriture_courant, "\r\n");
   }
   
   str_cat(&ecriture_courant, "\r\n");

   if (debug) fprintf(stderr, "Fin de l'écriture des headers : %s\n", ecriture_courant->lu);

  /* Copie du corps du message */
   lecture_courant=Deb_body;
   while (lecture_courant->lu[place_lu]!='\0') {
      if (lecture_courant->lu[place_lu]=='\n') {
	  deb_ligne=1;
	  str_cat(&ecriture_courant,"\r\n");
	  place_lu++; if (place_lu==lecture_courant->size) {
	     lecture_courant=lecture_courant->suivant;
	     if (lecture_courant==NULL) break;
	     place_lu=0;
	  }
      }
      else if ((lecture_courant->lu[place_lu]=='.') && (deb_ligne)) {
          ajoute_char(&ecriture_courant,'.');
	  deb_ligne=0;
      }	else {
	deb_ligne=0;
	str_ch_cat(&ecriture_courant, lecture_courant, place_lu, '\n');
	while (lecture_courant->lu[place_lu]!='\n') {
	   buf1=strchr(lecture_courant->lu+place_lu,'\n');
	   if (buf1==NULL) {
	      lecture_courant=lecture_courant->suivant;
	      place_lu=0;
	      if (lecture_courant==NULL) break;
	   } else place_lu=buf1-lecture_courant->lu;
	}
     }	
     if (lecture_courant==NULL) break;
   }
   if (get_char(ecriture_courant,1)!='\n') 
      str_cat(&ecriture_courant, "\r\n");
   str_cat(&ecriture_courant, ".\r\n");
   ecriture_courant->lu[ecriture_courant->size]='\0';
   if (debug) fprintf(stderr, "Fin de l'appel... %s\n", ecriture_courant->lu);
   return 0;
}


static void Save_failed_article() {
    FILE *tmp_file;
    Lecture_List *lecture_courant;
    
    tmp_file=open_postfile(TMP_REJECT_POST_FILE,"w");
    if (tmp_file==NULL) return;
    lecture_courant=Deb_body;
    while (lecture_courant->suivant) lecture_courant=lecture_courant->suivant;
    Copie_prepost(tmp_file, lecture_courant, lecture_courant->size, 0);
    fclose(tmp_file);
}

/* Poste un mail */
/* Retour : 1 : bon */
/* 	   -1 : pas bon */
static int Mail_article() {
   Lecture_List *a_ecrire=Deb_article;
   int fd, res;

   fd=Pipe_Msg_Start (1,0,MAILER_CMD); 
   	/* pas de stdout : sendmail envoie tout */
   	/* par mail...				*/
   if (fd==-1) return -1;
   do {
     res=write(fd,a_ecrire->lu,a_ecrire->size);
     if (res!=a_ecrire->size) return -1;
     a_ecrire=a_ecrire->suivant;
   } while (a_ecrire);
   Pipe_Msg_Stop(fd);
   return 1;
}

/* Poste franchement l'article         */
/* Peut-être à placer dans flrn_tcp ?  */
/* retour en cas d'échec :	       */
/* -1 : échec en lecture/écriture      */
/* -2 : post interdit		       */
/* -3 : pas de newsgroups valides      */
/* -4 : manque From		       */
/* -5 : manque Newsgroups	       */
/* -6 : manque Subject		       */
/* -7 : pas de corps (impossible)      */
/* -8 : article vide		       */
/* -9 : ligne trop longue	       */
/* -10 : adresse sous un format invalide */
/* -11 : pas autorisé */
/* -12 : post dans le futur */
/* -13 : erreur inconnue	       */
static int Post_article() {
    Lecture_List *a_ecrire=Deb_article;
    int res;
    char *buf;
    
    Screen_set_color(FIELD_NORMAL);
    res=write_command(CMD_POST, 0, NULL);
    if (res<0) return -1;

    res=return_code();
    if ((res<0) || (res>400)) return ((res<0) || (res>499) ? -1 : -2);
    
    do {
       res=raw_write_server(a_ecrire->lu, a_ecrire->size);
       if (res!=a_ecrire->size) return -1; /* on laisse tomber de toute facon */
       a_ecrire=a_ecrire->suivant;
    } while (a_ecrire);

    /* On n'utilise pas return_code pour le parsing */
    /* de toute facon, un timeout est exclu */
    res=read_server_with_reconnect(tcp_line_read, 3, MAX_READ_SIZE-1);
    if (res<3) return -1;
    if (debug) fprintf(stderr, "Lecture : %s", tcp_line_read);
    res=strtol(tcp_line_read, &buf, 10);
    if (res==240) return 1; 
    /* On suppose que res==441 */
    if (strstr(buf, "future")) return -12;
    if (strstr(buf, "allowed")) return -11;
    if (strstr(buf, "Internet")) return -10;
    if (strstr(buf, "too long")) return -9;
    if (strstr(buf, "empty")) return -8;
    if (strstr(buf, "no body")) return -7;
    if (strstr(buf, "Subject")) return -6;
    if ((strstr(buf, "valid")!=NULL) || (strstr(buf,"such")!=NULL)) return -3;
    if (strstr(buf, "Newsgroups")) return -5;
    if (strstr(buf, "From")) return -4;
    return -13;
}


/* Libere la memoire allouee */
static void Libere_listes() {

   free_chaine(Deb_article);
   free_chaine(Deb_body);
}


/* Place l'adresse e-mail de l'utilisateur dans str*/
/* taille maxi admise : 256 + length(nom) + 1 */
void Get_user_address(char *str) {
   char hostname[256];
   char *buf1;
 
   strcpy(str, flrn_user->pw_name);
   strcat(str, "@");
#ifdef DEFAULT_HOST
   strncpy(hostname, DEFAULT_HOST, 255-strlen(DOMAIN));
#else
#ifdef HAVE_GETHOSTNAME
   gethostname(hostname,255-strlen(DOMAIN)); /* PAS de test d'erreur */
#else
#error Vous devez definir DEFAULT_HOST dans flrn_config.h
#endif
#endif
  /* hostname, sur naoned au moins, renvoie naoned.ens.fr */
  /* Pour les autres machines, je sais pas. Ca semble pas */
  /* clair sur fregate.					  */
   buf1=strchr(hostname,'.');
   if (buf1) *(++buf1)='\0'; else strcat(hostname,".");
   strcat(hostname, DOMAIN);  
   strcat(str, hostname);
}

/* Extration des references pour le header References du post */
/* len_id donne la taille à réserver en plus pour le msgid du pere */
static char *extract_post_references (char *str, int len_id) {
   int len, nb_ref=0, i;
   char *buf=str, *buf2, *res;
   
   /* On calcule le nombre de references */
   while ((buf=strchr(buf,'<'))) {
     nb_ref++;
     buf=strchr(buf,'>');
     if (buf==NULL) break; /* BEURK */
   }
   if (nb_ref<MAX_NB_REF) {  /* On se pose pas de questions */
     len=strlen(str);
     res=safe_malloc((len+len_id+2)*sizeof(char));
     strcpy(res,str);
     strcat(res," ");
     return res;
   }
   /* Y'a des pb */
   /* On determine d'abord len */
   buf=str; buf=strchr(str,'>'); buf++;
   len=buf-str;
   buf2=strchr(buf,'<');
   for (i=1; i<nb_ref-4; i++) { buf2=strchr(buf2,'>'); buf2=strchr(buf2,'<'); }
   len+=1+strlen(buf2);
   res=safe_malloc((len+len_id+2)*sizeof(char));
   strncpy(res,str,buf-str);
   res[buf-str]='\0';
   strcat(res," ");
   strcat(res,buf2);
   strcat(res," ");
   return res;
}



/* Creation des headers du futur article */
/* Flag : 0 : normal 1 : mail -1 : cancel ou supersedes */
static int Get_base_headers(int flag, Article_List *article) {
   int res, len1, len2=0, len3, key, i;
   char *real_name, *buf;
   string_list_type *parcours;

   Header_post=safe_calloc(1,sizeof(Post_Headers));
   memset(Header_post,0,sizeof(Post_Headers));
   par_mail=(flag==1);
   /* On va d'abord gérer les headers persos... */
   /* Comme ca ils vont se faire écraser par les autres... */
   /* ces headers sont bien formatés... mais ils peuvent etre "reformates" */
   if (flag!=-1) {
      parcours=Options.user_header;
      while (parcours) {
        buf=strchr(parcours->str,':');
	len1=buf-parcours->str+1;
	for (i=0;i<NB_KNOWN_HEADERS;i++) 
	  if (strncasecmp(parcours->str,Headers[i].header,
	                          Headers[i].header_len)==0) break;
        if ((i!=NB_KNOWN_HEADERS) && (is_modifiable(i))) 
	  /* c'est a revoir... */
	{
	   Header_post->k_header[i]=safe_malloc(513);
	   Copy_format (NULL, buf+2, article, Header_post->k_header[i], 512);
	}
	else if (i==NB_KNOWN_HEADERS) {
	   Header_List *parcours2=Header_post->autres;
	   while (parcours2 && (parcours2->next)) parcours2=parcours2->next;
	   if (Header_post->autres==NULL) 
	      Header_post->autres=parcours2=safe_malloc(sizeof(Header_List));
	   else { 
	     parcours2->next=safe_malloc(sizeof(Header_List));
	     parcours2=parcours2->next;
	   }
	   parcours2->next=NULL;
	   parcours2->header=safe_malloc(513);
	   Copy_format (NULL, parcours->str, article, 
	                   parcours2->header, 512);
	   parcours2->num_af=0;
	}
	parcours=parcours->next;
      }
   }
   if (Pere_post) { 
      if ((Pere_post->headers==NULL)||(Pere_post->headers->all_headers==0))
	cree_header(Pere_post,0,0,0);
      if (Pere_post->headers==NULL) {
	   if (debug) fprintf(stderr,"Cree-headers a renvoye un resultat beurk !\n");
	   return -1;
      }
      /* Determiner si on doit faire la réponse par mail en priorité */
      if ((flag==0) && (Pere_post->headers->k_headers[FOLLOWUP_TO_HEADER])) {
        if  (strcasecmp(Pere_post->headers->k_headers[FOLLOWUP_TO_HEADER],"poster")==0) {
	  while (flag==0) {
	    Cursor_gotorc(Screen_Rows-1,0);
	    Screen_write_string("Répondre par mail (O/N/A) ? ");
	    key=Attend_touche();
	    key=toupper(key);
	    if (KeyBoard_Quit || (key=='A')) return -1;
	    if (key=='O') flag=1;
	    if (key=='N') break;
	  }
	  par_mail=flag; /* 0 ou 1 */
        } else if (strcasecmp(Newsgroup_courant->name,Pere_post->headers->k_headers[FOLLOWUP_TO_HEADER])!=0) { 
			/* On ne demande rien si on est dans le bon groupe */
	  while (1) {
	    Cursor_gotorc(Screen_Rows-1,0);
	    Screen_write_string("Suivre le followup (O/N/A) ? ");
	    key=Attend_touche();
	    key=toupper(key);
	    if (KeyBoard_Quit || (key=='A')) return -1;
	    if (key=='O') {
	        Header_post->k_header[NEWSGROUPS_HEADER]=
		 safe_strdup(Pere_post->headers->k_headers[FOLLOWUP_TO_HEADER]);
		break;
	    }
	    if (key=='N') break;	  
	  }
        } 
      }
      /* References */
      if (!supersedes) { /* Pas de References pour un supersedes ? */
        len3=strlen(Pere_post->msgid);
        if (Pere_post->headers->k_headers[REFERENCES_HEADER]) 
	     Header_post->k_header[REFERENCES_HEADER]=extract_post_references(Pere_post->headers->k_headers[REFERENCES_HEADER], len3);
        else { 
	  Header_post->k_header[REFERENCES_HEADER]=safe_malloc((len3+1)*sizeof(char));
	  *(Header_post->k_header[REFERENCES_HEADER])='\0';
        }
        strcat(Header_post->k_header[REFERENCES_HEADER], Pere_post->msgid);
      } else   /* Si... on duplique le header références... pour que "Re: "
		   ne soit pas injustifié */
	if (Pere_post->headers->k_headers[REFERENCES_HEADER]) 
	  Header_post->k_header[REFERENCES_HEADER]=safe_strdup(Pere_post->headers->k_headers[REFERENCES_HEADER]);
      /* Sujet */
      if (Pere_post->headers->k_headers[SUBJECT_HEADER]) {
	if (!supersedes) {
	  len1=strlen(Pere_post->headers->k_headers[SUBJECT_HEADER]);
	  if (strncasecmp(Pere_post->headers->k_headers[SUBJECT_HEADER],"re: ",4)!=0)
	    len2=4; else len2=0;
	  Header_post->k_header[SUBJECT_HEADER]=
		       safe_malloc((len1+len2+1)*sizeof(char));
	  if (strncasecmp(Pere_post->headers->k_headers[SUBJECT_HEADER],"re: ",4)!=0)
	    strcpy(Header_post->k_header[SUBJECT_HEADER], "Re: "); else
	      Header_post->k_header[SUBJECT_HEADER][0]='\0';
	  strcat(Header_post->k_header[SUBJECT_HEADER],
	      Pere_post->headers->k_headers[SUBJECT_HEADER]);
	} else 
	  Header_post->k_header[SUBJECT_HEADER]=
	     safe_strdup(Pere_post->headers->k_headers[SUBJECT_HEADER]);
      }
   }
   /* Newsgroups */
   if (Header_post->k_header[NEWSGROUPS_HEADER]==NULL) {
	 if (!supersedes) {
	    if (Newsgroup_courant->flags & GROUP_READONLY_FLAG) 
	        Header_post->k_header[NEWSGROUPS_HEADER]=safe_strdup("junk");
	    else
	 	Header_post->k_header[NEWSGROUPS_HEADER]=safe_strdup(Newsgroup_courant->name);
	 } else
	    Header_post->k_header[NEWSGROUPS_HEADER]=safe_strdup(Pere_post->headers->k_headers[NEWSGROUPS_HEADER]);
   }
   /* Headers To: et In-Reply-To: */
   Cursor_gotorc(1,0);
   Screen_erase_eos();
   if (par_mail) {   /* Ceci suppose que Pere_post soit défini */
     Header_List *parcours2=Header_post->autres;

     if (Pere_post->headers->k_headers[REPLY_TO_HEADER])
       Header_post->k_header[TO_HEADER]=safe_strdup(Pere_post->headers->k_headers[REPLY_TO_HEADER]); else
       Header_post->k_header[TO_HEADER]=safe_strdup(Pere_post->headers->k_headers[FROM_HEADER]); 
     while (parcours2 && (parcours2->next)) parcours2=parcours2->next;
     if (parcours2) {
       parcours2->next=safe_malloc(sizeof(Header_List));
       parcours2=parcours2->next;
     } else Header_post->autres=parcours2=safe_malloc(sizeof(Header_List));
     parcours2->next=NULL;
     parcours2->header=safe_malloc(15+strlen(Pere_post->msgid));
     parcours2->num_af=0;
     strcpy(parcours2->header,"In-Reply-To: ");
     strcat(parcours2->header,Pere_post->msgid);
     Screen_write_string("To : ");
     Screen_write_string(Header_post->k_header[TO_HEADER]);
   } else {
     Screen_write_string("Groupe : ");
     Screen_write_string(Header_post->k_header[NEWSGROUPS_HEADER]);
   }
   /* Sujet */
   if (Header_post->k_header[SUBJECT_HEADER]==NULL) {
       Header_post->k_header[SUBJECT_HEADER]=
	   safe_malloc((MAX_SUJET_LEN+1)*sizeof(char));
     if (flag==-1) strcpy(Header_post->k_header[SUBJECT_HEADER],"Cancel"); else
     {
       res=get_Sujet_post(Header_post->k_header[SUBJECT_HEADER]); 
       if (res==-1) return -1;
     }
   } 
   /* From */
   real_name=safe_strdup(flrn_user->pw_gecos);
   buf=strchr(real_name,','); if (buf) *buf='\0';
   len1=257+strlen(flrn_user->pw_name);
   if (Options.post_name)
     len2=4+strlen(Options.post_name);
   else
     len2=4+strlen(real_name);
   Header_post->k_header[FROM_HEADER]=safe_malloc((len1+len2)*sizeof(char));
   Get_user_address(Header_post->k_header[FROM_HEADER]);
   strcat(Header_post->k_header[FROM_HEADER]," (");
   if (Options.post_name) 
     strcat(Header_post->k_header[FROM_HEADER], Options.post_name);
   else 
     strcat(Header_post->k_header[FROM_HEADER], real_name);
   strcat(Header_post->k_header[FROM_HEADER],")");
   free(real_name);
   return 0;
}


/* Libération des headers de l'article */
static void Free_post_headers() {
   Header_List *liste1, *liste2;
   int i;

   if (Header_post==NULL) return;
   liste1=Header_post->autres;
   while (liste1) {
     liste2=liste1->next;
     if (liste1->header) free(liste1->header);
     free(liste1);
     liste1=liste2;
   }
   for (i=0; i<NB_KNOWN_HEADERS; i++) {
     if (Header_post->k_header[i]) 
        free(Header_post->k_header[i]);
   }
   free(Header_post);
}


/* encode un header selon la rfc2047, sans se poser la question		 */
/* des caractère spéciaux... (donc ne peut pour l'instant ne s'appliquer */
/* qu'au sujet...)							 */
static void encode_headers(char **header_to_encode) {
   char *place_to_put, *header_content=*header_to_encode;

   /* On suppose la taille max a 140+2*strlen */
   place_to_put=safe_malloc(140+strlen(header_content)*2);
   rfc2047_encode_string(place_to_put, 
       (unsigned char*) header_content, 75+strlen(header_content)*2);
   *header_to_encode=safe_realloc(*header_to_encode, strlen(place_to_put)+1);
   strcpy(*header_to_encode, place_to_put);
   free(place_to_put);
}

/* Cancel un message (fonction secondaire de ce fichier).		*/
/* Renvoie : >0 si le message est effectivement cancelé... 0 si le      */
/* cancel est annulé, et <0 si le cancel est refusé, ou bug...		*/
int cancel_message (Article_List *origine) {
   int res,key;
   char line[80];

   if (origine->numero==-1) return -1;
   supersedes=0;
   Pere_post=NULL; /* Pas besoin de References */
   res=Est_proprietaire(origine);
   if (res!=1) return -1;
   /* On pourrait demander confirmation */
   /* L'équivalent d'un Aff_fin */
   sprintf(line,"Canceler le message %d (O/N) : ",origine->numero);
   Cursor_gotorc(Screen_Rows-1,0);
   Screen_set_color(FIELD_ERROR);
   Screen_write_string(line);
   Screen_set_color(FIELD_NORMAL);
   Screen_erase_eol();
   Cursor_gotorc(Screen_Rows-1, strlen(line));
   key=Attend_touche();
   key=toupper(key);
   if (key!='O') return 0;
   /* Bon... Tant pis */
   res=Get_base_headers(-1,origine);
   if (res==-1) {
     Free_post_headers();
     return -1;
   }
   /* On remplie un corps bidon */
   Deb_body=alloue_chaine();
   strcpy(Deb_body->lu,"Cancel d'un message.\n");
   res=Format_article(origine->msgid);
   res=Post_article();
   Free_post_headers();
   Libere_listes();
   if (res<0) return -1;
   return 1;
}


/* Poste un message (fonction principale de ce fichier).		*/
/* Renvoie : >0 en cas de message posté, 0 en cas d'annulation, <0 en   */
/* en cas d'erreur.							*/
/* renvoie positifs : & 1 : post envoyé... & 2 : mail envoyé...		*/
/* flag est à 1 si demande de réponse par mail, a -1 si supersedes	*/
int post_message (Article_List *origine, char *name_file,int flag) {
    int res=1, res_post=0, res_mail=0;
    char *str=name_file;

    if (origine && (origine->numero<0)) {
       Aff_error_fin("L'article est hors du newsgroup !",1,-1);
       return -1;
    }
    if (flag==-1) {
      res=Est_proprietaire(origine);
      if (res!=1) {
	Aff_error_fin("Supersedes interdit !",1,-1);
        return -1;
      }
    }
    if (flag==-1) supersedes=1;
    Pere_post=origine;
    sigwinch_received=0;
    Deb_article=Deb_body=NULL;
    /* On détermine les headers par défaut de l'article */
    /* D'ou l'intérêt de Followup-to			*/
    res=Get_base_headers(flag,origine);
    if (res==-1) {
      Free_post_headers();
       return 0;
    }
    while ((*str) && (isblank(*str))) str++;
    if (*str!='\0') { 
       res=charge_postfile(str);
       if (res==0) return -1;
    } else {
      if (Pere_post) {
        Cursor_gotorc(2,0);
        if (flag!=-1) Screen_write_string("Votre réponse : "); else
	  Screen_write_string("Votre article de remplacement : ");
      }
      res=get_Body_post();
      Screen_set_screen_start(NULL,NULL);
    }
    if (!Header_post->k_header[SUBJECT_HEADER]) res=0;
    if (res<=0) {
      Screen_set_color(FIELD_ERROR);
      Cursor_gotorc(4,3); /* Valeurs au pif */
      if (res<0) Screen_write_string("Post annulé."); else
	Screen_write_string("Post ou sujet vide...");
      Screen_set_color(FIELD_NORMAL);
    }
    if (res<=0) { Free_post_headers(); Libere_listes(); return res; }
/* On n'encode que le sujet... C'est pas bo de toute façon */
    if (Header_post->k_header[SUBJECT_HEADER])
       encode_headers(&(Header_post->k_header[SUBJECT_HEADER]));
    res=Format_article((supersedes ? origine->msgid : NULL));
    if (Header_post->k_header[TO_HEADER] ||
	Header_post->k_header[CC_HEADER] ||
	Header_post->k_header[BCC_HEADER]) res_mail=Mail_article();
    if ((!par_mail) && (Header_post->k_header[NEWSGROUPS_HEADER])) 
         res_post=Post_article(); 
	 /* Si on ne veut pas répondre par mail, on ne répond pas par mail...*/
    if ((res_post<0) || (res_mail<0)) Save_failed_article();
    if (res_post<0) {
      /* on reecrit un Aff_error :( */
      Cursor_gotorc(1,0);
      Screen_erase_eos();
      Screen_set_color(FIELD_ERROR);
      Cursor_gotorc(Screen_Rows/2-1,3); /* Valeurs au pif */
      Screen_write_string("Post refusé... L'article est sauvé dans ~/dead.article");
      Cursor_gotorc(Screen_Rows/2,3);
      Screen_write_string("Raison du refus : ");
      switch (res_post) {
	case -1 : Screen_write_string("erreur GRAVE en lecture/ecriture."); 
		  break;
	case -2 : Screen_write_string("pas le droit de poster.");
		  break;
	case -3 : Screen_write_string("les newsgroups ne sont pas valides");
		  break;
	case -4 : Screen_write_string("header From manquant.");
		  break;
	case -5 : Screen_write_string("header Newsgroups manquant ou  invalide.");
		  break;
	case -6 : Screen_write_string("pas de sujet.");
		  break;
	case -7 : Screen_write_string("que des headers ???");
		  break;
	case -8 : Screen_write_string("le message est vide.");
		  break;
	case -9 : Screen_write_string("une ligne est trop longue.");
		  break;
        case -10: Screen_write_string("Un header contient une adresse invalide.");	break;
        case -11: Screen_write_string("Post non autorisé.");	
		  break;
	case -12: Screen_write_string("problème de date avec le serveur");
	default : Screen_write_string("inconnu : ");
		  break;
      }
      Cursor_gotorc(Screen_Rows/2+1,3);
      Screen_write_nstring(tcp_line_read,strlen(tcp_line_read)-2);
      Screen_set_color(FIELD_NORMAL);
    } else if (res_mail<0) Aff_error("Mail refusé... L'article est sauvé dans dead.article");
    Free_post_headers();
    Libere_listes();
    return ((res_post<0) || (res_mail<0) ? -1 : res_post+res_mail*2);
/* Bon, la fin est un peu rapide. Mais on verra bien. */
}
