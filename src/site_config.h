#ifndef FLRN_SITE_CONFIG_H
#define FLRN_SITE_CONFIG_H
/* On définit ici la configuration par défaut
 * On peut se référer à la page de man pour la signification des options
 * On peut faire afficher ces valeurs a flrn en lancant "flrn -s" */

#ifdef IN_OPTION_C

struct Option_struct Options = {
  "news.rezo.ens.fr",   /* server */
  DEFAULT_NNTP_PORT,    /* port */
  NULL, 		/* Post name - doit rester NULL */
  {REFERENCES_HEADER,FROM_HEADER,DATE_HEADER,SUBJECT_HEADER,-1}, /* header */
  {-1},			/* weak headers */
  {-1},			/* hidden headers */
  NULL,			/* user headers */
  0,			/* headers scroll */
  1,			/* skip_line */
  -1,			/* color, -1 = autodetect */
  1,			/* cbreak */
  0,			/* forum_mode */
  0,			/* space_is_return */
  0,			/* cool_arrows, inversé ! */
  0,			/* edit_all_headers */
  1,			/* simple_post */
  0,			/* include_in_edit */
  1,			/* date_in_summary */
  0,			/* duplicate_subject */
  1,			/* use_mailbox */
  1,			/* ordered_summary */
  0,			/* threaded_space */
  0,			/* auto_edit */
  0,			/* use_regexp */
  0,			/* use_menus (très alpha) */
  0,			/* quit_if_nothing */
  NULL,			/* auto_subscribe */
  "^control|^[^.]+$",	/* auto_ignore */
  1,			/* subscribe_first */
  1,			/* default_subscribe */
  "> ",			/* index_string */
  1,			/* smart_quote */
  0,			/* zap_change_group */
  0,			/* scroll_after_end */
  "%n, dans son post %i, a écrit :",	/* attribution */
  0,			/* alpha_tree, temporaire */
  ".flrnkill",         /* kill_file_name */
  };

#endif
#endif
