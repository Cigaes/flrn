/* flrn v 0.3								*/
/*		flrn_tcp.c        22/06/98				*/
/*									*/
/* Communication avec le serveur... Essentiellement tout ce qui se      */
/* rapporte a la gestion d'une socket...				*/


#include <ctype.h>      /* a voir */
#include <sys/types.h>  /* socket */
#include <sys/socket.h> /* Internet address manipulation routines */
#include <netinet/in.h> /* Internet address manipulation routines */
#include <arpa/inet.h>  /* Internet address manipulation routines */
#include <netdb.h>      /* appel au DNS */
#include <unistd.h>     /* read/write */
#include <sys/time.h>   /* select */
#include <stdlib.h>     /* atoi */
#include <stdio.h>      
#include <strings.h>
#include <errno.h>

#include "flrn.h"
#include "config.h"
#include "options.h"
#include "flrn_tcp.h"

int Date_offset;

/* Commandes au serveur */
/* cf flrn_config.h. On fixe le nombre pour vérifier la correspondance */
static const char *tcp_command[NB_TCP_CMD] = {
    "QUIT", "MODE READER", "NEWGROUPS", "NEWNEWS", "GROUP",
    "STAT", "HEAD", "BODY", "XHDR", "XOVER", "POST", "LIST", "DATE", 
    "ARTICLE" };
int server_command_status[NB_TCP_CMD] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};

/* Ligne lue (cf flrn_glob.h) */
char tcp_line_read[MAX_READ_SIZE];
/* file desciptor de la socket */
int tcp_fd;

static char filter_cmd_list [MAX_NEWSGROUP_LEN+1];
static int renvoie_direct; /* on renvoie tcp_line_read à la prochaine lecture */
			   /* on DOIT se limiter à un code de retour...       */

/* Connection au port d'une machine. La fonction est rudimentaire pour  */
/* l'instant, mais j'ai peur que ça nécessite quelques améliorations    */
/* pour l'avenir.							*/
/* Je m'inspire directement de slrn et de traqueur.c (cf Fba et moi)    */
/* D'autre part, la procedure ne recherchera a joindre le serveur que   */
/* par une adresse (cf sltcp.c de slrn pour plus de rigueur)		*/
/* Note : requires host != NULL						*/
/* Note : dans slrn, on rajoute des tests pour siglongjmp et sigsetjmp, */
/* mais je prefere ne pas m'en occuper (non mais sans blague).		*/

static int contact_server (char *host, int port) {
   struct hostent *hp;      /* Le serveur ( si ! )       */
   struct sockaddr_in s_in; /* la socket  ( si aussi ! ) */
#if 0
   struct protoent *tcpproto;
   int on=1;   		   /* pour setsockopt		*/
#endif
  
   /* Determination du serveur */
#ifdef HAVE_INET_ATON
   if (!inet_aton(host,&(s_in.sin_addr))) /* inet_addr est OBSOLETE, selon Linux */
#else
   if ((s_in.sin_addr.s_addr=inet_addr(host))==-1) /* mais pas pour tout le monde */
#endif
   {  /* host est un nom de machine */
      hp=(struct hostent *) gethostbyname(host);
      if (!hp) {
         fprintf(stderr, "%s : Unknown host.\n", host);
         return -1;
      }
      s_in.sin_addr.s_addr=*(unsigned long int *)(hp->h_addr_list[0]); /* UNE adresse */
      s_in.sin_family=hp->h_addrtype;   /* toujours AF_INET, parait-il*/
   }
   else
      s_in.sin_family=AF_INET;
   s_in.sin_port = htons((unsigned short) port);

   /* Creation de la socket */
   tcp_fd=socket(s_in.sin_family, SOCK_STREAM, 0);
   if (tcp_fd < 0) {
      perror("socket");
      return -1;
   }

   /* cette operation n'est pas faite dans slrn... on verra            */
/*
   if ((tcpproto = (struct protoent *) getprotobyname("tcp")) != NULL)
      setsockopt(tcp_fd, tcpproto->p_proto, TCP_NODELAY, &on, sizeof(int));
*/

   /* La commence normalement un essai par adresse. On n'en a garde qu'une */
   /* donc ce sera plus facile.						  */
   if (connect(tcp_fd, (struct sockaddr *)&s_in, sizeof(s_in)) < 0) {
      perror("connect");
      /* on ferme la socket avant de sortir */
      close(tcp_fd);
      return(-1);
   }
   /* Et voila... Aussi simple que ca */   
   return (tcp_fd);
}



/* Lecture "brute" du serveur						*/
/* Cette fonction lit des données sur le serveur, et renvoie sur buf ce */
/* qui est lu. Le résultat de la fonction est le nombre d'octets        */
/* lus.									*/ 
/* min est le nombre d'octects minimum que l'on s'attend a pouvoir lire */
/* (en cas de reponse a une requete).					*/
/* Cette fonction renvoie -1 en cas d'erreur, 0 si connexion coupee.	*/
/* ATTENTION : on suppose avoir assez de place dans buf.		*/

static int raw_read_server (char *buf, int min) {
#if 0
   fd_set rfds;
#endif
/*   struct timeval tv;
   int ret_sel,attempt=0; */
   int ret_read;
   int lus=0;

   while (lus<min)
   {
#if 0   /* on bloque betement */
      tv.tv_sec=2+attempt*3;  /* Cette valeur est éventuellement à modifier */
      tv.tv_usec=0;
      FD_ZERO(&rfds);
      FD_SET(tcp_fd, &rfds);
      ret_sel=1;
      ret_sel=select(tcp_fd+1, &rfds, NULL, NULL, &tv);
      if (ret_sel<1) {
         if (ret_sel==0)
	 { if (debug) fprintf(stderr,"Temps d'attente dépassé... "); }
         else { perror("select dans raw_read"); return -1; }
         attempt++;
         if (attempt==3) {
            if (debug) fprintf(stderr, "On abandonne.\n");
            return -1;
         } else if (debug) fprintf(stderr, "On reprend.\n");
      }
#else
     ret_read=read(tcp_fd,buf+lus,MAX_BUF_SIZE-1-lus);
     if (ret_read<0) { /* dans ce cas, on laisse tomber */ 
	if (errno==EINTR)
	   continue;
	else
	  { perror ("Read : "); return -1; }
     }
     if (ret_read==0) /* pourquoi ? */
     {  if (debug) fprintf (stderr, "Qu'est-ce que signifie read=0 ?\n");
	return -1;
     }
     lus+=ret_read;
#endif
   }
   return lus;
}

/* Lecture du serveur							*/
/* Cette fonction, qui appelle la fonction precedente, s'occupe de      */
/* raffiner la lecture, et renvoie les donnees ligne par ligne.		*/
/* Cette fonction renvoie la taille de la ligne rendue.			*/
/* Enfin, elle initialise ses valeurs statiques si deb=-1 (appel en     */
/* debut de programme). deb=1 indique un debut de ligne, deb=3 indique  */
/* qu'on attend un code. 						*/
/* NOTE : cette fonction suppose que la ligne contient suffisamment de  */
/* place (max+1).							*/
/* NOTE2 : on place '\0' a la fin de la lecture...			*/

int read_server (char *ligne, int deb, int max)
{
   static char raw_buf[MAX_BUF_SIZE], *ptr;
   static int stockes;
   int rendus, ret_raw_read, ret;
   char *ptr2, *ligne_ptr=ligne;

   if (deb==-1) {
      stockes=0;
      return 0;  /* retour sans importance */
   }
 
   if (stockes<=0) {  /* nouvelle lecture */
      ret_raw_read=raw_read_server(raw_buf, (deb==0 ? 1 : deb) );
      if (ret_raw_read<=0)   /* BUG */
         return -1;
      ptr=raw_buf;
      stockes=ret_raw_read;
      raw_buf[stockes]='\0';
   }

   ptr2=strchr (ptr, (int)'\n');
   if (ptr2==NULL) {
      if ((rendus=strlen(ptr))<max) {
         stockes-=rendus; /* devrait etre zero, a moins de lire '\0' */
         strcpy(ligne, ptr);
         ptr+=rendus;
         ligne_ptr+=rendus;
         if (stockes>0) { ligne_ptr++; /* on a effectivement lu '\0' ????? */
                          rendus++;
                          stockes--; ptr++;
                          if (rendus==max) {   /* Ouinnnn */
                              ligne[max]='\0'; 
                              return max;
                          }
                        }
         ret=read_server(ligne_ptr, 0, max-rendus);
         if (ret==-1) return -1;
         rendus+=ret;
         ligne[rendus]='\0';
         return rendus;
      } else
      {
         rendus=max;
         strncpy(ligne, ptr, max);
         ptr+=rendus;
         stockes-=rendus;
         ligne[max]='\0';
         return max;
      }
   } else
   {
      ptr2++;
      if (ptr2-ptr>max) {   /* Que des emmerdes ! */
         rendus=max;
         strncpy(ligne, ptr, max);
         ptr+=rendus;
         ligne[max]='\0';
         return max;
      } else {  /* le cas qui devrait marcher le plus souvent... */
         rendus=ptr2-ptr;
         strncpy(ligne, ptr, rendus);
         ptr=ptr2;
         stockes-=rendus;  /* stockes devrait etre toujours >= 0 */
         ligne[rendus]='\0';
         return rendus;
      } 
   }
   /* Unreachable */
}

/* parse du timeout : on teste "imeout" ou "timed" */
/* on revoie 1 si le test est juste */
   /* I know of only two timeout responses:  (slrn)
    *     * 503 Timeout
    *     * 503 connection timed out
   */
static int _nntp_try_parse_timeout(char *str) {
   if (strstr(str,"imeout") || strstr(str,"timed")) return 1;
   return 0;
}

/* Cette fonction fait un read_server de retour et se reconnecte au cas ou */
int read_server_with_reconnect (char *ligne, int deb, int max) {
   int lus, code;

   if (renvoie_direct) {
      renvoie_direct=0;
      if (ligne==tcp_line_read) return strlen(tcp_line_read);
      /* Aie, ici c'est un bug. On élimine l'idée du renvoie_direct */
      /* Mais logiquement, il ne peut y avoir ce bug */
   }
   lus=read_server(ligne, deb, max);
   if (lus<3) {
      if (debug) fprintf(stderr, "Echec en lecture du serveur\n");
      return -1;
   }
   
   if (debug) fprintf(stderr, "Lecture : %s", ligne);
   code=atoi(ligne);

   /* On va essayer de gerer le cas du timeout */
   if ((code==503) && (_nntp_try_parse_timeout(ligne))) {
     if (debug) fprintf(stderr,"Un timeout... %s",ligne);
       if (reconnect_after_timeout(1)<0) return -1;
       /* envoie aussi toutes les bonnes commandes au serveur */
       return read_server_with_reconnect(ligne,deb,max);
   }
   return lus;
}

/* pattern matching uniquement avec des '*' */
static int pattern_match(char *chaine, char *pat) {
  char *p1=chaine,*p2=pat;

  while ((*p1!='\0') && (*p2!='\0')) {
    if (*p2=='*') {
       while (*(++p2)=='*');
       if (*p2=='\0') return 1;
       while (*p1) {
          if (pattern_match(p1,p2)) return 1;
          p1++;
       }
       return 0;
    }
    if (*p2!=*p1) return 0;
    p2++; p1++;
  }
  return 1;
}


/* read_server_for_list : cas particulier : on peut éventuellement filtrer */
/* la sortie...								   */
int read_server_for_list (char *ligne, int deb, int max) {
    int ret;
    char *buf;

    if (filter_cmd_list[0]=='\0') return read_server(ligne,deb,max);
    while (1) {
      ret=read_server(ligne,deb,max);
      if (ligne[1]=='\r') return ret;
      buf=strchr(ligne,' ');
      if (buf) *buf='\0';
      if (pattern_match(ligne,filter_cmd_list)) {
         if (buf) *buf=' ';
	 return ret;
      }
    }
}

/* Vire tout ce que peut envoyer le serveur jusqu'au prochain "\r\n.\r\n" */
/* renvoie -1 en cas d'erreur.						  */
int discard_server() {
   int ret;

   if (debug) fprintf(stderr,"Discard server\n");
   do {
      ret=read_server(tcp_line_read, 1, MAX_READ_SIZE-1);
      if (ret<1) return -1;
      if ((tcp_line_read[0]=='.') && (tcp_line_read[1]!='.')) return 0;
      while (tcp_line_read[ret-1]!='\n') {
           ret=read_server(tcp_line_read,1,MAX_READ_SIZE);
           if (ret<1) return -1;
      }
   } while (1);
}


/* Ouverture PROPRE de la connexion					*/
/* Cette fonction se charge d'appeler connect_server, et lit la reponse */
/* du serveur a la connexion.						*/
/* Elle renvoie -1 en cas d'echec de la connexion, le code renvoye par */
/* le serveur sinon. Si ce code indique un refus du serveur, la         */
/* fonction coupe la connexion.					*/
int connect_server (char *host, int port) {
    int ret, code;
   
    if (host==NULL) {
       fprintf(stderr,"Pas de serveur où se connecter.\n");
       return -1; 
    }
    if (port==0) port=Options.port;
    ret=contact_server(host, port);
    if (ret<0) {
       fprintf(stderr, "Echec de la connexion au serveur : %s\n", host);
       return -1;
    }
     
    code=return_code();

    if ((code!=200) && (code!=201))
    {
       if (debug) fprintf (stderr,"Accès refusé. On va donc quitter :-( \n"); 
       close (tcp_fd);
       return code;
    }
    server_command_status[CMD_QUIT]=CMD_FLAG_MAXIMAL;
    server_command_status[CMD_MODE_READER]=CMD_FLAG_MAXIMAL;
    server_command_status[CMD_POST]=(code==200 ? CMD_FLAG_MAXIMAL : 
    						 CMD_FLAG_TESTED);
    write_command(CMD_MODE_READER, 0, NULL);
    
    ret=return_code();

    if (debug) fprintf (stderr,"Serveur retourne (mode reader) : %d\n", code);

/* On place juste XMODE READER au cas ou...*/
/* Non nécéssaire (trn4 ne le fait pas) */
/*
    if (ret>400) {
      raw_write_server("XMODE READER\r\n", 14);
      ret=return_code();
    }
*/

    return code;
}

int adjust_time()
{
  long heure_serveur, heure_locale;
  int res,code;
  char *buf;

  Date_offset=0;
  write_command(CMD_DATE, 0, NULL);
  res=read_server_with_reconnect(tcp_line_read, 3, MAX_READ_SIZE-1);
  if (debug) fprintf(stderr,"Yo :%s\n",tcp_line_read);
  if (res<0) return -1;
  buf=strchr(tcp_line_read,' ');
  code = time(NULL);
  if (buf) {
    code=strtol(buf+9,&buf,10);
    heure_serveur=code %100 + 60*(((code/100)%100) + 60*(((code/10000)%100)));
  } else
    heure_serveur=code;

  heure_locale=time(NULL);
/*  heure_locale = mktime(gmtime(&heure_locale))%86400; */
  Date_offset = ((heure_serveur - heure_locale)%86400);
  if (Date_offset > 43200) Date_offset = Date_offset - 86400;
  if (Date_offset < -43200) Date_offset = Date_offset + 86400;
  if (debug)
    fprintf(stderr,"Yu :%ld %ld %d\n",heure_serveur,
	heure_locale,Date_offset);
  return 0;
}

/* Lecture juste de la ligne du retour d'une commande.			*/
/* On utilise read_server_with_reconnect				*/
int return_code () {
   int lus, code;

   lus=read_server_with_reconnect(tcp_line_read, 3, MAX_READ_SIZE-1);
   if (lus<3) return -1;
   
   code=atoi(tcp_line_read);

   /* Ici, on gere le cas d'une ligne trop longue */
   while (tcp_line_read[lus-1]!='\n') {
      if (debug) fprintf(stderr, "ligne tres longue ???\n");
      lus=read_server(tcp_line_read, 1, MAX_READ_SIZE-1);
   
       if (lus<1) {
          if (debug) fprintf(stderr, "Echec en lecture du serveur\n");
          return -1;
       }
       if (debug) fprintf(stderr, "%s", tcp_line_read);
    }
    return code;
}

/* Cette fonction ecrit brutalement au serveur, jusqu'au bout, si c'est */
/* possible.								*/
int raw_write_server (char *buf, unsigned int len) {
   int res;
   unsigned int total;

   total=0;
  
   while (total != len)
   { 
      res=write(tcp_fd, buf+total, (len-total)); /* slrn met buf ??? */
      if (res==-1)
      {
	/* euh... On n'est pas en mode non bloquant... */
#ifdef EAGAIN
          if (errno == EAGAIN) 
             {
                sleep (1);
                continue;
             }
#endif
#ifdef EWOULDBLOCK
          if (errno == EWOULDBLOCK)
             {
                sleep (1);
                continue;
             }
#endif
	  if (errno == EINTR) continue;
	  if (debug) fprintf(stderr,"Erreur en ecriture\n");
          return total;
      }
      total += (unsigned int) res;
   }
   return total;
}

/* Ecriture d'une commande d'une ligne.					*/
/* Parametre : numero de la commande. nombre de parametres, tableau sur */
/* les parametres.							*/
static char line_write[MAX_READ_SIZE];
   /* on sort cette variable de la fonction : elle sert dans */
   /* reconnect_after_timeout.				     */

static int raw_write_command (int num_com, int num_param, char **param) {
   char *ptr;
   int size, pram, tmp_len;
 
   pram=0; ptr=line_write;

   strcpy (line_write, tcp_command[num_com]);
   size=strlen(line_write); ptr+=size;
   while (pram<num_param) {
       *(ptr++)=' '; size++;
       tmp_len=strlen(param[pram]);
       if (MAX_READ_SIZE-size-tmp_len<3) {
          if (debug) fprintf(stderr, "Ligne a envoyer trop longue !");
          return -1;
       }
       strncpy (ptr, param[pram], tmp_len);
       ptr+=tmp_len; size+=tmp_len;
       pram++;
   }
   *(ptr++)='\r'; *(ptr++)='\n'; size+=2;

   return raw_write_server(line_write, size);
}

/* Cette commande va maintenant être BEAUCOUP plus difficile */
/* C'est une petite vacherie à elle toute seule		     */
/* retour de -2 : commande qui ne donne rien (list newsgroups avec */
/* list minimal...						   */
int write_command (int num_com, int num_param, char **param) {
   int ret,code;
   char **param2=param;
   
   filter_cmd_list[0]='\0';
   renvoie_direct=0;
   if ((server_command_status[num_com] & CMD_FLAG_MAXIMAL)==CMD_FLAG_MAXIMAL) 
     return raw_write_command(num_com, num_param, param2);

   switch (num_com) {
      /* NEWGROUPS : on se fiche totalement que ça marche ou non */
      /* NEWNEWS   : appelé par cherche_newnews, le retour -1 n'est pas grave */
      /* GROUP     : on ne peut pas le rejeter, on le suppose donc maximal */
      case CMD_NEWGROUPS : case CMD_NEWNEWS : case CMD_GROUP :
          server_command_status[num_com]=CMD_FLAG_MAXIMAL;
	  return raw_write_command(num_com, num_param, param2);
      /* STAT : pfiouuuu... pour l'instant, je laisse tomber */
      /* HEAD : idem */
      /* BODY : idem */
      /* ARTICLE : idem */
      case CMD_STAT : case CMD_HEAD : case CMD_BODY : case CMD_ARTICLE :
          server_command_status[num_com]=CMD_FLAG_MAXIMAL;
	  return raw_write_command(num_com, num_param, param2);
      /* XHDR : bon, ça louze, mais si on est là, XOVER n'existe pas */
      /* XOVER : on n'installe pas le test tout de suite... */
      case CMD_XHDR : case CMD_XOVER :
          server_command_status[num_com]=CMD_FLAG_MAXIMAL;
	  return raw_write_command(num_com, num_param, param2);
      /* POST : théoriquement déjà testé */
      case CMD_POST :
          return raw_write_command(num_com, num_param, param2);
      /* LIST : ah ! enfin le truc important !!! */
      /* On suppose le nombre de paramètre à 0 ou 1 (2 => active) */
      case CMD_LIST :
          if ((num_param==0) && 
	     ((server_command_status[num_com] & CMD_FLAG_KNOWN)
	        ==CMD_FLAG_KNOWN)) 
          return raw_write_command(num_com, num_param, param2);
	  /* On se fout de overview.fmt */
	  if ((num_param>0) && (strcmp(*param2,"overview.fmt")==0))
	      return raw_write_command(num_com, num_param, param2);
	  if ((server_command_status[num_com] & CMD_FLAG_KNOWN)
	            ==CMD_FLAG_KNOWN) {
	     if (num_param==2) {
	        if (strncmp(*param2,"active",6)==0) param2++;
		else return -2; /* erreur */
             }
	     if (strncmp(*param2,"active ",7)==0) *param2=*param2+7;
	     strncpy(filter_cmd_list,*param2,MAX_NEWSGROUP_LEN-1);
             return raw_write_command(num_com,0,NULL);
	  }
	  if ((server_command_status[num_com] & CMD_FLAG_TESTED)
	            ==CMD_FLAG_TESTED) return -1; 
		    	/* impossible pour l'instant */
          ret=raw_write_command(num_com, num_param, param2);
	  if (ret<0) return -1;
	  code=return_code();
	  if (code>400) {
	     server_command_status[CMD_LIST]=CMD_FLAG_KNOWN;
	     return write_command(CMD_LIST, num_param, param);
	  }
	  if (num_param>0) server_command_status[CMD_LIST]=CMD_FLAG_MAXIMAL;
	  renvoie_direct=1;
	  return ret;
       /* CMD_DATE : un détail dans un sens */
       case CMD_DATE :
          if ((server_command_status[num_com] & CMD_FLAG_KNOWN)
		                       ==CMD_FLAG_KNOWN)
	     return raw_write_command(CMD_DATE, 0, NULL);
	  ret=raw_write_command(CMD_DATE, 0, NULL);
	  if (ret<0) return -1;
	  code=return_code();
	  if (code>500) return -1;
	  renvoie_direct=1;
	  return ret;
       /* Par défaut, hum... Ben... on retourne */
       default : return raw_write_command(num_com, num_param, param);
   }
}


/* on renvoie 0 si la connexion est un succes, -1 sinon */
/* si refait_commande=1, on envoie la commande GROUPE, puis */
/* l'ancienne commande...				 */
int reconnect_after_timeout(int refait_commande) {
   int code, ret;
   char *chaine_to_sauve, *ptr;

   close(tcp_fd);
   ptr=strchr(line_write,'\n');
   if (ptr) *(++ptr)='\0';
   chaine_to_sauve=safe_strdup(line_write);
   code=connect_server(Options.serveur_name,0);
   if ((code!=200) && (code!=201)) { free(chaine_to_sauve); return -1; }
   adjust_time();   /* il faut peut-être mieux le refaire, si c'est possible */
   /* reste a envoyer la bonne commande pour GROUP */
   if (refait_commande) {
     if ((ret=va_dans_groupe())==-1) { free(chaine_to_sauve); return -1; }
     if (ret==0) {
       code=return_code(); 
       if (code<0) { free(chaine_to_sauve); return -1; }
     }
     if (debug) fprintf(stderr, "Reste a renvoyer la commande bugguante\n");
     raw_write_server(chaine_to_sauve, strlen(chaine_to_sauve)); 
      /* on renvoie la commande... */
   }
   if (debug) fprintf(stderr, "Reconnexion réussie ...\n");
   free(chaine_to_sauve);
   return 0;
}

/* Termine la connexion au serveur					*/
/* Cette fonction se charge d'envoyer QUIT au serveur, avant de quitter */
void quit_server () {
  int res;

  res=write_command(CMD_QUIT,0,NULL);
  if (res>0)
  {
    res=read_server(tcp_line_read, 3, MAX_READ_SIZE-1);
    if (res<0) return;
    if (debug) fprintf(stderr, "lecture : %s", tcp_line_read);
  }
  close(tcp_fd);   /* Bon, cette procedure est non fiable. */
}
