/* cmdline.h */

/* File autogenerated by gengetopt version 2.20  */

#ifndef CMDLINE_H
#define CMDLINE_H

/* If we use autoconf.  */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef CMDLINE_PARSER_PACKAGE
#define CMDLINE_PARSER_PACKAGE "ingen"
#endif

#ifndef CMDLINE_PARSER_VERSION
#define CMDLINE_PARSER_VERSION VERSION
#endif

struct gengetopt_args_info
{
  const char *help_help; /* Print help and exit help description.  */
  const char *version_help; /* Print version and exit help description.  */
  int engine_flag;	/* Run (JACK) engine (default=off).  */
  const char *engine_help; /* Run (JACK) engine help description.  */
  int engine_port_arg;	/* Engine OSC port (default='16180').  */
  char * engine_port_orig;	/* Engine OSC port original value given at command line.  */
  const char *engine_port_help; /* Engine OSC port help description.  */
  char * connect_arg;	/* Connect to existing engine at OSC URI (default='osc.udp://localhost:16180').  */
  char * connect_orig;	/* Connect to existing engine at OSC URI original value given at command line.  */
  const char *connect_help; /* Connect to existing engine at OSC URI help description.  */
  int gui_flag;	/* Launch the GTK graphical interface (default=on).  */
  const char *gui_help; /* Launch the GTK graphical interface help description.  */
  int client_port_arg;	/* Client OSC port.  */
  char * client_port_orig;	/* Client OSC port original value given at command line.  */
  const char *client_port_help; /* Client OSC port help description.  */
  char * load_arg;	/* Load patch.  */
  char * load_orig;	/* Load patch original value given at command line.  */
  const char *load_help; /* Load patch help description.  */
  char * path_arg;	/* Target path for loaded patch.  */
  char * path_orig;	/* Target path for loaded patch original value given at command line.  */
  const char *path_help; /* Target path for loaded patch help description.  */
  char * run_arg;	/* Run script.  */
  char * run_orig;	/* Run script original value given at command line.  */
  const char *run_help; /* Run script help description.  */
  
  int help_given ;	/* Whether help was given.  */
  int version_given ;	/* Whether version was given.  */
  int engine_given ;	/* Whether engine was given.  */
  int engine_port_given ;	/* Whether engine-port was given.  */
  int connect_given ;	/* Whether connect was given.  */
  int gui_given ;	/* Whether gui was given.  */
  int client_port_given ;	/* Whether client-port was given.  */
  int load_given ;	/* Whether load was given.  */
  int path_given ;	/* Whether path was given.  */
  int run_given ;	/* Whether run was given.  */

} ;

extern const char *gengetopt_args_info_purpose;
extern const char *gengetopt_args_info_usage;
extern const char *gengetopt_args_info_help[];

int cmdline_parser (int argc, char * const *argv,
  struct gengetopt_args_info *args_info);
int cmdline_parser2 (int argc, char * const *argv,
  struct gengetopt_args_info *args_info,
  int override, int initialize, int check_required);
int cmdline_parser_file_save(const char *filename,
  struct gengetopt_args_info *args_info);

void cmdline_parser_print_help(void);
void cmdline_parser_print_version(void);

void cmdline_parser_init (struct gengetopt_args_info *args_info);
void cmdline_parser_free (struct gengetopt_args_info *args_info);

int cmdline_parser_required (struct gengetopt_args_info *args_info,
  const char *prog_name);


#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* CMDLINE_H */
