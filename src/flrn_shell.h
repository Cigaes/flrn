#ifndef FLRN_FLRN_SHELL_H
#define FLRN_FLRN_SHELL_H

/* Les fonctions */

extern int Launch_Editor (int /*flags*/);
extern int Pipe_Msg_Start(int /*flagin*/, int /*flagout*/, char * /*cmd*/);
extern int Pipe_Msg_Stop(int /*fd*/);

#endif
