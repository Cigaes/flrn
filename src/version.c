/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-2000  Damien Massé et Joël-Yann Fourré
 *
 *      version.c : affichage de la version
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

#include <stdio.h>
#include <slang.h>
#include <sys/utsname.h>

#include "flrn.h"
#include "options.h"

static UNUSED char rcsid[]="$Id$";

char version_string[]=
"Flrn version 0.9.0 par Damien et Jo, 06/2004.";

char short_version_string[]=
"Flrn (0.9.0 - 04/06)";

int version_number=90;

void print_version_defines(FILE *out)
{
   struct utsname uts;
   uname (&uts);
   fprintf(out, "System: %s %s\n", uts.sysname, uts.release);
   fprintf(out, "S-Lang version: %d\n", SLANG_VERSION);
   fprintf(out, "Options:\n");
#ifdef SENDMAIL
   fprintf(out, "MAILER_CMD=\"%s\"  ", MAILER_CMD);
#else
   fprintf(out, "SENDMAIL=\"\"  ");
#endif
#ifdef DEFAULT_HOST
   fprintf(out, "DEFAULT_HOST=\"%s\"  ", DEFAULT_HOST);
#endif
#ifdef DOMAIN
   fprintf(out, "DOMAIN=\"%s\"  ", DOMAIN);
#endif
   fprintf(out, "MAX_HEADER_LIST=%d\n", MAX_HEADER_LIST);
   fputs(
#ifdef MODE_EXPERT
         "+MODE_EXPERT  "
#else
         "-MODE_EXPERT  "
#endif
#ifdef USE_SLANG_LANGUAGE
         "+USE_SLANG_LANGUAGE\n"
#else
         "-USE_SLANG_LANGUAGE\n"
#endif
#ifdef GNKSA_DISPLAY_HEADERS
         "+GNKSA_DISPLAY_HEADERS  "
#else
         "-GNKSA_DISPLAY_HEADERS  "
#endif
#ifdef GNKSA_STANDARD_COMMANDS
         "+GNKSA_STANDARD_COMMANDS  "
#else
         "-GNKSA_STANDARD_COMMANDS  "
#endif
#ifdef GNKSA_ANALYSE_POSTS
         "+GNKSA_ANALYSE_POSTS\n"
#else
         "-GNKSA_ANALYSE_POSTS\n"
#endif
#ifdef GNKSA_NEWSGROUPS_HEADER
         "+GNKSA_NEWSGROUPS_HEADER  "
#else
         "-GNKSA_NEWSGROUPS_HEADER  "
#endif
#ifdef GNKSA_REFERENCES_HEADER
         "+GNKSA_REFERENCES_HEADER  "
#else
         "-GNKSA_REFERENCES_HEADER  "
#endif
#ifdef GNKSA_POINT_9
         "+GNKSA_POINT_9\n"
#else
         "-GNKSA_POINT_9\n"
#endif
#ifdef GNKSA_QUOTE_SIGNATURE
         "+GNKSA_QUOTE_SIGNATURE  "
#else
         "-GNKSA_QUOTE_SIGNATURE  "
#endif
#ifdef GNKSA_REWRAP_TEXT
         "+GNKSA_REWRAP_TEXT\n"
#else
         "-GNKSA_REWRAP_TEXT\n"
#endif
	,out);
   fprintf(out,"DEFAULT_DIR_FILE=\"%s\"  ", DEFAULT_DIR_FILE);
   fprintf(out," DEFAULT_FLNEWS_FILE=\"%s\"\n", DEFAULT_FLNEWS_FILE);
   fprintf(out,"DEFAULT_CONFIG_FILE=\"%s\"  ", DEFAULT_CONFIG_FILE);
   fprintf(out,"DEFAULT_CONFIG_SYS_FILE=\"%s\"\n", DEFAULT_CONFIG_SYS_FILE);
   fprintf(out,"TMP_POST_FILE=\"%s\"  ", TMP_POST_FILE);
   fprintf(out,"TMP_PIPE_FILE=\"%s\"\n", TMP_PIPE_FILE);
#ifdef USE_MKSTEMP
   fputs("+USE_MKSTEMP\n",out);
#else
   fputs("-USE_MKSTEMP\n",out);
#endif
   fprintf(out,"REJECT_POST_FILE=\"%s\"\n",REJECT_POST_FILE); 
#ifdef CHECK_MAIL
   fprintf(out,"+CHECK_MAIL  DEFAULT_MAIL_PATH=\"%s\"\n", DEFAULT_MAIL_PATH);
#else
   fprintf(out,"-CHECK_MAIL\n");
#endif
}

#if 0
extern char *program_ids[];
#endif

/* output version information on file handle out, for the given program
 * assumes format built by makeid, that is, first entry in program_ids
 * is __DATE__, second entry is compilation options, 
 * next entries are RCS-format Ids, full with
 * the $id: $ shennanigans so that this set of strings is recognized 
 * by ident as well. End marked with a null pointer.
 */
void print_version_info(FILE *out, char *program_name)
   {
#if 0
   char **s = program_ids+2;
#endif

   fprintf(out, "%s\n", version_string);

#if 0
   fprintf(out, "%s, compiled on %s with %s\n", program_name,
   program_ids[0],
   program_ids[1]);
#endif
   print_version_defines(out);

#if 0
   while (*s)
      {
      char *id = *s;

      id += 5;                /* skip over "$Id: " */
      while(*id != ',')       /* assumes "file,v" RCS syntax */
         fputc(*id++, out);
      id += 3;                /* skip over ",v " */
         {                    /* tabulate to 24 characters */
         unsigned i;
         for (i = id - (5+3) - *s; i < 24; i++)
            fputc(' ', out);
         }
      while(*id != ' ')       /* and add version number */
         fputc(*id++, out);
      fputc('\n', out);
      s++;
      }
#endif
   }

int version_has(char *request) {
    if (strcmp(request,"sendmail")==0) 
#ifdef SENDMAIL
       return 1;
#else
       return 0;
#endif
   if (strcmp(request,"slang_language")==0)
#ifdef USE_SLANG_LANGUAGE
       return 1;
#else
       return 0;
#endif
   if (strcmp(request,"check_mail")==0)
#ifdef CHECK_MAIL
       return 1;
#else
       return 0;
#endif
   return 0;
}
