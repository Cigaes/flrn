#ifndef FLRN_EXTERN_H
#define FLRN_EXTERN_H

#include <stdio.h>
/* #include <slang.h> */
#include <sys/stat.h>

#include "group.h"
#include "art_group.h"
#include "flrn_string.h"
#include "flrn_glob.h"
#include "flrn_menus.h"
#include "flrn_filter.h"

/*   flrn_tcp.c    */
extern int connect_server (char * /*host*/, int /*port*/);
extern void quit_server (void);
extern int read_server (char * /*ligne*/, int /*deb*/, int /*max*/);
extern int read_server_with_reconnect
			(char * /*ligne*/, int /*deb*/, int /*max*/);
  /* appel pour post */
extern int raw_write_server (char * /*buf*/, unsigned int /*len*/);
extern int write_command (int /*num_com*/, int /*num_para*/, char ** /*param*/);
extern int reconnect_after_timeout(int /*refait_commande*/);
extern int discard_server(void);
extern int return_code (void);
extern int adjust_time(void);
extern int read_server_for_list (char*, int, int);

/*   flrn_files.c */
extern FILE *open_flrnfile (char * /*file*/,char * /*mode*/, int, time_t *);
extern void rename_flnewsfile (char * /*old_link*/,char * /*new_link*/);
extern FILE *open_postfile (char * /*file*/,char * /*mode*/);
extern int stat_postfile (char * /*file*/,struct stat * /*mode*/);
extern FILE *open_flhelpfile (char /*ext*/);
extern void Copy_article (FILE * /*dest*/, Article_List * /*article*/,
    int /*copie_head*/, char * /*avant*/);
extern int init_kill_file(void);
extern int newmail(char *);
extern int read_list_file(char *, Flrn_liste *);
extern int write_list_file(char *, Flrn_liste *);

/* flrn_shell.c */
extern int Launch_Editor (int /*flags*/);
extern int Pipe_Msg_Start(int /*flagin*/, int /*flagout*/, char * /*cmd*/);
extern int Pipe_Msg_Stop(int /*fd*/);

/*   options.c    */
extern void init_options(void);
extern void parse_options_line(char * /*ligne*/, int /*flag*/);
extern void dump_variables(FILE * /*file*/);
extern void dump_flrnrc(FILE * /*file*/);
extern void options_comp(char * /*option*/, int /*len*/);
extern void free_options(void);
extern void menu_config_variables(void);

/*   group.c      */
extern void init_groups(void);
extern void free_groups(int /*save_flnewsrc*/);
extern void new_groups(int /*opt_c*/);
extern Newsgroup_List *cherche_newsgroup(char * /*name*/, int /*exact*/, int);
extern Newsgroup_List *cherche_newsgroup_re (char * /*name*/, regex_t /*reg*/, int );
extern Liste_Menu *menu_newsgroup_re (char * /*name*/, regex_t /*reg*/,
    int /*avec_reg*/);
extern void zap_newsgroup(Newsgroup_List * /*group*/);
extern int NoArt_non_lus(Newsgroup_List * /*group*/);
extern int cherche_newnews(void);
extern void add_read_article(Newsgroup_List * /*group*/, int /*numero*/);
extern char *truncate_group (char *, int);
extern void test_readonly(Newsgroup_List *);
extern void zap_group_non_courant (Newsgroup_List *);
extern void Ligne_carac_du_groupe (void *, char *, int );

    
/*  art_group.c  */
extern int va_dans_groupe(void);
extern int cree_liens(void);
extern Article_Header *cree_header(Article_List * /*article*/,
    int /*rech_pere*/, int /*others*/, int);
extern void ajoute_reponse_a(Article_List * /*article*/);
extern Article_List *ajoute_message(char * /*msgid*/, int * /*should_retry*/);
extern Article_List *ajoute_message_par_num(int , int);
extern void detruit_liste(int);
extern void libere_liste(void);
extern void free_article_headers(Article_Header *);
extern Article_List *next_in_thread(Article_List * /*start*/,
    long /*flag*/, int * /*level*/,
    int /*deb*/, int /*fin*/, int /*set*/, int);
extern Article_List *root_of_thread(Article_List * /*article*/, int /*flag*/);
extern void article_read(Article_List * /*article*/);
extern int Recherche_article (int /*num*/, Article_List ** /*retour*/,
    int /*flags*/);
extern int ajoute_exte_article(Article_List * /*fils*/);
extern Article_Header *new_header(void);
extern int Est_proprietaire(Article_List * /*article*/);
extern void apply_kill_file(void );
extern Article_List *cousin_prev(Article_List *article);
extern Article_List *cousin_next(Article_List *article);


/* flrn_format */
extern time_t parse_date(char * /*s*/);
extern char *vrai_nom(char * /*nom*/);
extern char *local_date (char * /*date*/);
extern int str_estime_len (char *, int , int );
extern int to_make_len (char *, int , int );
extern void Copy_format (FILE * /*tmp_file*/, char * /*chaine*/,
    Article_List * /*article*/);
extern void ajoute_parsed_from (char *, char *);

/* flrn_xover */
extern int get_overview_fmt(void);
extern int cree_liste_xover(int /*n1*/, int /*n2*/, Article_List **);
extern int cree_liste_noxover(int /*n1*/, int /*n2*/,
    Article_List * /*article*/);
extern int cree_liste(int, int * );
extern int cree_liste_end(void);


/* tty_display.c */
extern int Init_screen(void);
extern void Reset_screen(void);
extern void sig_winch(int );
extern int Aff_article_courant(void);
extern void Aff_newsgroup_name(void);
extern void Aff_newsgroup_courant(void);
extern void Aff_not_read_newsgroup_courant(void);
extern char * Prepare_summary_line(Article_List * /*article*/,
    char * /*prev_subject*/, int /*level*/, char * /*buf*/, int /*buflen*/, int);
extern int Aff_summary_line(Article_List * /*article*/,int * /*row*/,
    char * /*prev_subject*/, int /*level*/);
extern Article_List * Menu_summary (int /*deb*/, int /*fin*/, int /*thread*/);
extern int Aff_fin(const char * /*str*/);
extern int Aff_error(const char * /*str*/);
extern int Aff_file(FILE * /*file*/, char *, char *);
extern int Liste_groupe(int /*n*/, char * /*mat*/, Newsgroup_List **);
extern int Aff_arbre(int,int,Article_List *, int, int, int, unsigned char **, int);

/* tty_keyboard.c */
extern int Init_keyboard(void);
extern int Attend_touche(void);
extern int getline(char * /*buf*/, int /*buffsize*/, int /*row*/, int /*col*/);
extern int magic_getline(char * /*buf*/, int /*buffsize*/, int /*row*/,
    int /*col*/, char * /*magic*/, int /*flag*/);

/* flrn_inter.c   */
int fonction_to_number(char *);
int call_func(int, char *);
extern int loop(char * /*opt*/);
extern void aff_opt_c(void);
extern void init_Flcmd_rev(void);
extern int Comp_cmd_explicite(char * /*str*/, int /*len*/);
extern int Bind_command_explicite(char * /*nom*/, int /*key*/, char * /*arg*/); 
extern void save_etat_loop(void);
extern void restore_etat_loop(void);


/* post.c	  */
extern void Get_user_address(char * /*str*/);
extern int cancel_message (Article_List * /*origine*/);
extern int post_message (Article_List * /*origine*/, char * /*name_file*/,
    int /*flag*/);

/* flrn_string.c */
extern Lecture_List *alloue_chaine(void);
extern int free_chaine(Lecture_List * /*chaine*/);
extern int ajoute_char(Lecture_List ** /*chaine*/, int /*chr*/);
extern int enleve_char(Lecture_List ** /*chaine*/);
extern char get_char(Lecture_List * /*chaine*/, int /*n*/);
extern int str_cat (Lecture_List ** /*chaine1*/, char * /*chaine*/);
extern int str_ch_cat(Lecture_List ** /*chaine1*/, Lecture_List * /*chaine2*/,
    int /*place*/, char /*chr*/);

/* flrn_menus.c */
extern void *Menu_simple (Liste_Menu * /*debut_menu*/, Liste_Menu * /*actuel*/,
    void action(void *,char *,int),
    int action_select(void *, char **, int, char *, int,int), char * /*titre*/);
extern void Libere_menu (Liste_Menu * /*debut*/);
extern void Libere_menu_noms (Liste_Menu * /*debut*/);
extern Liste_Menu *ajoute_menu(Liste_Menu * /*base*/, char * /*nom*/,
    void * /*lobjet*/);
extern Liste_Menu *ajoute_menu_ordre(Liste_Menu *, char *, void *, int);
extern int Bind_command_menu(char *, int, char *);
extern void init_Flcmd_menu_rev(void);

/* rfc2047.c */
extern void rfc2047_encode_string (char *, unsigned char *, size_t);
extern void rfc2047_decode (char *, const char *, size_t);

/* flrn_help.c */
extern void Aide (void);

/* flrn_pager.c */
extern int Page_message (int /*num_elem*/, int /*short_exit*/, int /*key*/, 
	int /*act_row*/, int /*row_deb*/, char * /*exit_chars*/, char *);
extern int Bind_command_pager(char *, int, char *);
extern void init_Flcmd_pager_rev(void);


/* flrn_regexp.c */
extern char * reg_string (char * /*pattern*/, int /*flag*/);

/* flrn_filter.c */
extern int check_article(Article_List *, flrn_filter *, int);
extern flrn_filter *new_filter(void);
extern int parse_filter(char *, flrn_filter *);
extern int parse_filter_flags(char *, flrn_filter *);
extern int parse_filter_action(char *, flrn_filter *);
extern void free_filter(flrn_filter *);
extern int parse_kill_file(FILE *);
extern void apply_kill(int);
extern void check_kill_article(Article_List *, int );
extern int add_to_main_list(char *);
extern int remove_from_main_list(char *);
extern void free_kill();
extern int in_main_list(char *);



/* flrn_color.c */
extern void free_highlights(void);

extern int parse_option_color(int /*func*/, char * /*line*/);
extern void Init_couleurs(void);
extern int Aff_color_line(int /*to_print*/, unsigned short * /*format_line*/,
    int * /*format_len*/, int /*field*/, char * /*line*/, int /*len*/,
    int /*bol*/, int /*def_color*/);
extern unsigned short *cree_chaine_mono (const char *, int, int);
extern void dump_colors_in_flrnrc (FILE *file);

/* Fonctions liées avant à slang... Dans flrn_slang.c */
extern void Screen_suspend(void);
extern void Screen_resume(void);
extern void Screen_get_size(void);
extern int Screen_init_smg(void);
extern void Screen_reset(void);
extern void Get_terminfo(void);
extern void Screen_write_char(char /*c*/);
extern void Screen_write_string(char * /*s*/);
extern void Screen_write_nstring(char * /*s*/, int /*n*/);
extern void Cursor_gotorc(int /*r*/, int /*c*/);
extern void Screen_erase_eol(void);
extern void Screen_erase_eos(void);
extern void Screen_refresh(void);
extern void Screen_touch_lines (int /*d*/, unsigned int /*n*/);
extern void Set_Use_Ansi_Colors(int /*n*/);
extern int  Get_Use_Ansi_Colors(void);
extern void Color_set(int /*n*/, char * /*s1*/, char * /*s2*/, char * /*s3*/);
extern void Color_add_attribute(int /*n*/, FL_Char_Type /*a*/);
extern void Mono_set (int /*n*/, char * /*s*/, FL_Char_Type /*a*/);
extern void Screen_set_color(int /*n*/);
extern int Put_tty_single_input (int /*a*/, int /*b*/, int /*c*/);
extern void React_suspend_char (int /*a*/);
extern int init_clavier(void);
extern void Set_Ignore_User_Abort (int /*n*/);
extern int Get_Ignore_User_Abort (void);
extern int Set_Abort_Signal(void (* /*a*/)(int));
extern int Keyboard_getkey(void);
extern void Reset_keyboard(void);
extern void free_text_scroll(void);
extern void Init_Scroll_window(int /*num*/, int /*beg*/, int /*nrw*/);
extern File_Line_Type *Ajoute_line(char * /*buf*/);
extern File_Line_Type *Change_line(int, char * /*buf*/);
extern File_Line_Type *Ajoute_form_Ligne(char * /*buf*/, int /*field*/);
extern File_Line_Type *Ajoute_color_Line(unsigned short *, int, int);
extern File_Line_Type *Rajoute_color_Line(unsigned short *, int, int, int);
extern char *Read_Line(char * /*out*/, File_Line_Type * /*line*/);
extern void Retire_line(File_Line_Type * /*line*/);
extern int Do_Scroll_Window(int /*n*/, int /*ob_update*/);
extern int Number_current_line_scroll(void);
extern void Screen_write_color_chars(unsigned short * /*s*/,unsigned int /*n*/);
extern int parse_key_name(char *);

#endif
