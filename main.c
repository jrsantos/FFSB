/*
 *   Copyright (c) International Business Machines Corp., 2001-2004
 *
 *   This program is free software;  you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or 
 *   (at your option) any later version.
 * 
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY;  without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program;  if not, write to the Free Software 
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#include <pthread.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include <assert.h>

#include "config.h"

#include "ffsb.h"
#include "util.h"
#include "parser.h"

/* State information for the polling function below */
struct ffsb_time_poll {
	struct timeval starttime;
	int wait_time;
};

/* This is the polling function used by the threadgroups to check
 * elapsed time, when it returns 1 they know it is time to stop
 */
static int ffsb_poll_fn(void *ptr)
{
	struct ffsb_time_poll * data = (struct ffsb_time_poll *)ptr;  
	struct timeval curtime, difftime;
	gettimeofday(&curtime, NULL);

	timersub(&curtime, &data->starttime, &difftime);
	if (difftime.tv_sec >= data->wait_time) {
		return 1;
	}
	return 0;
}

int main(int argc, char *argv[])
{
	int i;
	ffsb_config_t fc;
	ffsb_barrier_t thread_barrier, tg_barrier;
	tg_run_params_t *params;
	struct ffsb_time_poll pdata;
	struct timeval starttime, endtime, difftime;
	pthread_attr_t attr;
	ffsb_op_results_t total_results;
	double totaltime = 0.0f, usertime = 0.0f, systime = 0.0f;
	struct rusage before_self, before_children, after_self, after_children;
	pthread_t * fs_pts; /* threads to do filesystem creates in parallel */
	char * callout = NULL;

	char ctime_start_buf[32];
	char ctime_end_buf[32];

	memset(&before_self,0,sizeof(before_self));
	memset(&before_children,0,sizeof(before_children));
	memset(&after_self ,0,sizeof(after_self ));
	memset(&after_children ,0,sizeof(after_children ));

	ffsb_unbuffer_stdout();

	if (argc < 2) {
		fprintf(stderr,"usage: %s <config file> [time]\n",
			argv[0]);
		exit(1);
	}

	/* VERSION comes from config.h (which is autogenerated by autoconf) */
	printf("FFSB version %s started\n\n", VERSION);

	if (argc == 3) {
		pdata.wait_time = atoi(argv[2]);
		printf("benchmark time set to %u sec\n", pdata.wait_time);
		ffsb_parse_oldconfig(&fc,argv[1]);
		fc.time = pdata.wait_time;
	} else {
		ffsb_parse_newconfig(&fc, argv[1]);
		pdata.wait_time = fc.time;
	}

	if (fc.time)
		printf("benchmark time = %u\n", fc.time);
	 else
		printf("Only creating the fileset, not running benchmark.\n");

	pthread_attr_init(&attr);
	pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);

	for (i = 0; i < fc.num_threadgroups; i++)
		tg_print_config( &fc.groups[i] );

	fs_pts = ffsb_malloc(sizeof(pthread_t) * fc.num_filesys);

	gettimeofday(&starttime, NULL);
	for (i = 0; i < fc.num_filesys; i++) {
		fs_print_config(&fc.filesystems[i]); 		
		pthread_create(fs_pts + i,&attr, construct_ffsb_fs,
			       &fc.filesystems[i]); 
	}

	fflush(stdout);
	for (i = 0; i < fc.num_filesys; i++)
		pthread_join(fs_pts[i],NULL);

	gettimeofday(&endtime, NULL);
	timersub(&endtime, &starttime, &difftime);
	printf("fs setup took %ld secs\n", difftime.tv_sec);
	free(fs_pts);

	if (fc.time == 0) {
		printf("Setup complete, exiting\n");
		return 0;
	}

	params = ffsb_malloc(sizeof(tg_run_params_t) * fc.num_threadgroups);

	init_ffsb_op_results(&total_results);
	ffsb_barrier_init(&thread_barrier, fc.num_totalthreads);
	ffsb_barrier_init(&tg_barrier, fc.num_threadgroups + 1);

	ffsb_sync();

	/* Execute the callout if any and wait for it to return */
	if (callout = fc_get_callout(&fc)) {
		printf("executing callout: \n %s\n", callout);
		if (ffsb_system(callout) < 0) {
			perror("system");
			exit(1);
		}
	}

	/* Spawn all of the threadgroup master threads */
	for( i = 0 ; i < fc.num_threadgroups ; i++ ) {
		params[i].tg = &fc.groups[i];
		params[i].fc = &fc;
		params[i].poll_fn = ffsb_poll_fn;
		params[i].poll_data = &pdata;
		params[i].wait_time = FFSB_TG_WAIT_TIME;
		params[i].tg_barrier = &tg_barrier;
		params[i].thread_barrier = &thread_barrier;

		pthread_create(&params[i].pt, &attr, tg_run, &params[i] );
	}

	ffsb_getrusage (&before_self, &before_children); 
	gettimeofday (&pdata.starttime, NULL );

	ffsb_barrier_wait(&tg_barrier);  /* sync with tg's to start*/
 	printf("Starting Actual Benchmark At: %s\n",
	       ctime_r(&pdata.starttime.tv_sec, ctime_start_buf));
	fflush(stdout);

	/* Wait for all of the threadgroup master threads to finish */
	for (i = 0; i < fc.num_threadgroups; i++)
		pthread_join( params[i].pt, NULL );

	ffsb_sync();
	gettimeofday(&endtime, NULL);
	ffsb_getrusage(&after_self, &after_children);

	printf("FFSB benchmark finished   at: %s\n", 
	       ctime_r(&endtime.tv_sec, ctime_end_buf));
	printf("Results:\n");
	fflush(stdout);

	timersub(&endtime, &pdata.starttime, &difftime);

	totaltime = tvtodouble(&difftime);

	printf("Benchmark took %.2lf sec\n", totaltime);
	printf("\n");

	for (i = 0; i < fc.num_threadgroups; i++) {
		struct ffsb_op_results tg_results;
		ffsb_tg_t *tg = fc.groups + i;

		init_ffsb_op_results(&tg_results);

		/* Grab the individual tg results */
		tg_collect_results(tg, &tg_results);

		if (fc.num_threadgroups == 1)
			printf("Total Results\n");
		else
			printf("ThreadGroup %d\n", i);

		printf("===============\n");
		print_results(&tg_results, totaltime);

		if (tg_needs_stats(tg)) {
			ffsb_statsd_t fsd;
			tg_collect_stats(tg, &fsd);
			ffsb_statsd_print(&fsd);
		}
		printf("\n");

		/* Add the tg results to the total */
		tg_collect_results(&fc.groups[i], &total_results);
	}

	if (fc.num_threadgroups > 1) {
		printf("Total Results\n");
		printf("===============\n");
		print_results(&total_results,totaltime);
	}

#define USEC_PER_SEC ((double)(1000000.0f))

	/* sum up self and children after */
	usertime = (after_self.ru_utime.tv_sec + 
		    ((after_self.ru_utime.tv_usec)/USEC_PER_SEC)) +
		((after_children.ru_utime.tv_sec + 
	          ((after_children.ru_utime.tv_usec)/USEC_PER_SEC)));

	/* subtract away the before */
	usertime -= (before_self.ru_utime.tv_sec + 
		     ((before_self.ru_utime.tv_usec)/USEC_PER_SEC)) +
		((before_children.ru_utime.tv_sec + 
		  ((before_children.ru_utime.tv_usec)/USEC_PER_SEC)));


	/* sum up self and children after */
	systime = (after_self.ru_stime.tv_sec + 
		   ((after_self.ru_stime.tv_usec)/USEC_PER_SEC)) +
		((after_children.ru_stime.tv_sec + 
		  ((after_children.ru_stime.tv_usec)/USEC_PER_SEC)));

	/* subtract away the before */
	systime -= (before_self.ru_stime.tv_sec + 
		    ((before_self.ru_stime.tv_usec)/USEC_PER_SEC)) +
		((before_children.ru_stime.tv_sec + 
		  ((before_children.ru_stime.tv_usec)/USEC_PER_SEC)));

	printf("\n\n");
	printf("%.1lf%% User   Time\n", 100 * usertime / totaltime);
	printf("%.1lf%% System Time\n", 100 * systime / totaltime);
	printf("%.1f%% CPU Utilization\n", 100 * (usertime + systime) / 
	       totaltime);
	free(params);
	destroy_ffsb_config(&fc);

	return 0;
}
