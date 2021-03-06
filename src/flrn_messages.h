/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Mass� et Jo�l-Yann Fourr�
 *
 *      flrn_messages.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

#ifndef FLRN_FLRN_MESSAGES_H
#define FLRN_FLRN_MESSAGES_H


#define MES_UNKNOWN_CMD 0
#define MES_NOTHING_NEW 1
#define MES_EOG 2
#define MES_NO_MES 3
#define MES_OMIT 4
#define MES_POST_SEND 5
#define MES_POST_CANCELED 6
#define MES_ART_SAVED 7
#define MES_ABON 8
#define MES_DESABON 9
#define MES_NOSEL_THREAD 10
#define MES_ZAP	11
#define MES_NOTHER_GROUP 12
#define MES_NO_XREF 13
#define MES_CONTINUE 14
#define MES_TAG_SET 15
#define MES_CANCEL_CANCELED 16
#define MES_CANCEL_DONE 17
#define MES_OP_DONE 18
#define MES_MAIL_SENT 19
#define MES_MAIL_POST 20
#define MES_TEMP_READ 21
#define MES_NO_GROUP 22
#define MES_GROUP_EMPTY 23
#define MES_NOT_IN_GROUP 24
#define MES_NEGATIVE_NUMBER 25
#define MES_SAVE_FAILED 26
#define MES_UNK_GROUP 27
#define MES_NO_FOUND_GROUP 28
#define MES_REGEXP_BUG 29
#define MES_PIPE_BUG 30
#define MES_MES_NOTIN_GROUP 31
#define MES_BAD_TAG 32
#define MES_CANCEL_REFUSED 33
#define MES_EMPTY_HISTORY 34
#define MES_NO_HEADER 35
#define MES_REFUSED_HEADER 36
#define MES_FATAL 37
#define MES_NO_SEARCH 38
#define MES_NO_FOUND 39
#define MES_INVAL_FLAG 40
#define MES_FLAG_APPLIED 41
#define MES_MACRO_FORBID 42
#define MES_NO_MSGID 43
#define MES_MODE_THREAD 44
#define MES_MODE_NORMAL 45

#define NB_MESSAGES 46
extern const char *Messages[NB_MESSAGES];

#endif
