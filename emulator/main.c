
 /*
 * Copyright (C) yajin 2008 <yajinzhou@gmail.com >
 *     
 * This file is part of the virtualmips distribution. 
 * See LICENSE file for terms of the license. 
 *
 */

 #define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <error.h>
#include<errno.h> 
#include<time.h>
 #include<signal.h>
 
#include  "utils.h"
#include  "mips64.h"
#include "device.h"
#include "vm.h"
#include "mips64_exec.h"
#include "ptask.h"



//char *log_file_name = NULL;
//FILE * log_file = NULL;
//#define LOGFILE_DEFAULT_NAME  "sim.txt"
char sw_version_tag[] = "20071220";
#define VERSION  "0.02"
#if 0
/* Create general log file */ 
static void create_log_file (void) 
{

	/* Set the default value of the log file name */ 
	if (!log_file_name)
	{
		if (!(log_file_name = strdup (LOGFILE_DEFAULT_NAME)))
		{
			fprintf (stderr, "Unable to set log file name.\n");
			exit (EXIT_FAILURE);
		}
	}
	if (!(log_file = fopen (log_file_name, "w")))
	{
		fprintf (stderr, "Unable to create log file (%s).\n",
				strerror (errno));
		exit (EXIT_FAILURE);
	}
}


/* Close general log file */ 
static void
close_log_file (void) 
{
	if (log_file)
		fclose (log_file);
	free (log_file_name);
	log_file = NULL;
	log_file_name = NULL;
}


#endif

void signal_gen_handler(int sig)
{
   switch(sig) {
      case SIGHUP:
         /* For future use */
         break;

      case SIGQUIT:
         printf("\nStop emulation\n");
         /*do not worry, exit will release all resource*/
         exit(EXIT_SUCCESS);
         break;

      case SIGINT:
      /* In theory, this shouldn't happen thanks to VTTY settings */
      break;
         
      default:
         fprintf(stderr,"Unhandled signal %d\n",sig);
   }
}


/* Setups signals */
static void setup_signals(void)
{
   struct sigaction act;

   memset(&act,0,sizeof(act));
   act.sa_handler = signal_gen_handler;
   act.sa_flags = SA_RESTART;
   sigaction(SIGHUP,&act,NULL);
   sigaction(SIGQUIT,&act,NULL);
   sigaction(SIGINT,&act,NULL);
}



int main(int argc,char *argv[])
{
	vm_instance_t *vm;
	char *configure_filename=NULL;

	printf("VirtualMIPS (version %s)\n",VERSION);
	printf("Copyright (c) 2008 yajin.\n");
	printf("Build date: %s %s\n\n",__DATE__,__TIME__);

	/* Initialize VTTY code */
	vtty_init();

	/* Create the default instance */
	if (!(vm = create_instance(configure_filename)))
		exit(EXIT_FAILURE);

	/*set seed for random value */ 
	srand ((int) time (0));
	/* Create general log file */ 
	//create_log_file ();

	/* Periodic tasks initialization */ 
	if (ptask_init (0) == -1)
		exit (EXIT_FAILURE);
	mips64_exec_create_ilt (vm->name);
    setup_signals();
	if (init_instance(vm)<0)
	{
		fprintf(stderr,"Unable to initialize instance.\n");
		/*exit will release all the resource!do not worry*/
		exit(EXIT_FAILURE);
	}

	vm_monitor (vm);
	//close_log_file ();
	return (0);

}



