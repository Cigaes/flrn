/* flrn : lecteur de news en mode texte
 *
 *      site_config.h : configuration par d�faut de flrn
 *
 */

/* $Id$ */

#ifndef FLRN_SITE_CONFIG_H
#define FLRN_SITE_CONFIG_H
/* On d�finit ici la configuration par d�faut
 * On peut se r�f�rer � la page de man pour la signification des options
 * On peut faire afficher ces valeurs a flrn en lancant "flrn -s" */

#ifdef IN_OPTION_C

#ifdef GNKSA_DISPLAY_HEADERS /* d�fini dans config.h ou flrn_config.h */
int deb_header_list[] = {NEWSGROUPS_HEADER,FOLLOWUP_TO_HEADER,REFERENCES_HEADER,FROM_HEADER,REPLY_TO_HEADER,DATE_HEADER,SUBJECT_HEADER,-1}; /* header */
#else  /* configuration "forum" */
int deb_header_list[] = {REFERENCES_HEADER,FROM_HEADER,DATE_HEADER,SUBJECT_HEADER,-1}; /* header */
#endif

#define ISIZE_HEADER_LIST  (sizeof(deb_header_list)/sizeof(int))

int deb_weak_header_list[ISIZE_HEADER_LIST] = {-1};
int deb_hidden_header_list[ISIZE_HEADER_LIST] = {-1};

struct Option_struct Options = {
  "news",  		 /* server */
  DEFAULT_NNTP_PORT,    /* port (DEFAULT_NNTP_PORT = 119) */
  NULL,			/* flnews_ext */
  NULL, 		/* Post name - doit rester NULL */
  &(deb_header_list[0]), /* header */
  &(deb_weak_header_list[0]),	/* weak headers */
  &(deb_hidden_header_list[0]),	/* hidden headers */
  NULL,			/* user headers */
  NULL,			/* user_flags */
  NULL,			/* user_autocmd */
  0,			/* headers scroll */
#ifdef GNKSA_DISPLAY_HEADERS 
  0,			/* skip_line */
#else
  1,			/* skip_line */
#endif
  -1,			/* color, -1 = autodetect */
  1,			/* cbreak */
#ifdef CHECK_MAIL
  1,			/* check_mail */
#endif
  0,			/* forum_mode */
  0,			/* space_is_return */
  0,			/* edit_all_headers (1 conseill�) */
  0,			/* include_in_edit */
  1,			/* date_in_summary */
  0,			/* duplicate_subject */
  1,			/* use_mailbox */
  1,			/* ordered_summary */
  1,			/* threaded_next */
  0,			/* use_regexp */
  0,			/* use_menus */
  0,			/* with_cousins */
  0,			/* quit_if_nothing */
  0,			/* parse_from */
  NULL,			/* auto_subscribe */
  "^control|^[^.]+$",	/* auto_ignore */
  1,			/* subscribe_first */
  1,			/* default_subscribe */
  "> ",			/* index_string */
  1,			/* smart_quote */
  0,			/* zap_change_group */
  0,			/* scroll_after_end */
  "%n, dans son post %i, a écrit :",	/* attribution */
  0,			/* small_tree */
  ".flrnkill",		/* kill_file_name */
  1,			/* auto_kill */
  "",			/* savepath */
  "",			/* prefixe_groupe */
  " TDD*tdd",		/* flags_group */
  NULL,			/* hist_file_name */
  0,			/* warn_if_new */
  NULL,			/* default_flnewsfile */
  0,			/* short_errors */
  1,			/* help_line */
  NULL,			/* help_line_file */
  NULL,			/* auth_user */
  NULL,			/* auth_pass */
  NULL,			/* auth_cmd */
  NULL,			/* sign_file */
  0,			/* quote_all */
  0,			/* quote_sig */
  NULL,                 /* terminal_charset */
  NULL,                 /* editor_charset */
  NULL,		 	/* post_charsets */
  NULL,			/* files_charset */
#ifndef DOMAIN
  NULL,                  /* default_domain */
#endif
  -1,			 /* max_group_size */
  NULL,			 /* alternate */
  };

#endif
#endif
