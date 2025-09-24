/* `time' utility to display resource usage of processes, main source file.
   Copyright (C) 1990-2021 Free Software Foundation, Inc.

   Originally written by David Keppel <pardo@cs.washington.edu>.
   Heavily modified by David MacKenzie <djm@gnu.ai.mit.edu>.
   Heavily modified (again) by Assaf Gordon <assafgordon@gmail.com>.

   This file is part of GNU Time.

   GNU Time is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   GNU Time is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GNU Time.  If not, see <http://www.gnu.org/licenses/>.
   */ 

#include <sys/wait.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include "error.h"

#include <time.h>

typedef struct timespec timespec;
        
// return {0, 0} if start is greater than end
static timespec time_sub(timespec end, timespec start)
{
        timespec temp;
        if ((end.tv_nsec-start.tv_nsec)<0) {
                temp.tv_sec = end.tv_sec-start.tv_sec-1;
                temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
        } else {
                temp.tv_sec = end.tv_sec-start.tv_sec;
                temp.tv_nsec = end.tv_nsec-start.tv_nsec;
        }
        if(temp.tv_sec < 0) {
                temp.tv_sec = 0;
                temp.tv_nsec = 0;
        }
        return temp;
}

static timespec run_cmd(int count, char **cmd)
{
        pid_t pid;
        int status;

        timespec start_time;
        clock_gettime(CLOCK_REALTIME, &start_time);

        for(int i = 0 ; i < count ; i++) {
                pid = fork ();
                if (pid < 0) {
                        fprintf(stderr, "Fork failed: error=%d\n", errno);
                        exit(1);
                } else if (pid == 0) {
                        int ret = execvp (cmd[0], cmd);
                        fprintf(stderr, "Failed to run: %s\n", cmd[0]);
                        exit(-errno);
                }

                waitpid(pid, &status, 0);
                if(!WIFEXITED(status) || WEXITSTATUS(status))
                        break;
        }

        timespec end_time;
        clock_gettime(CLOCK_REALTIME, &end_time);

        if(WIFEXITED(status)) {
                int exit_status = WEXITSTATUS(status);
                if(0 == exit_status) {
                        return time_sub(end_time, start_time);
                } else {
                        fprintf(stderr, "Process returned: %d\n", exit_status);
                        exit(1);
                }
        } else {
                fprintf(stderr, "Process did not terminate normally\n");
                exit(1);
        }
}

char **progname(char **argv)
{
        static char *prog[2] = {};
        prog[0] = argv[0];
        return prog;
}



int main(int argc, char **argv)
{
        char **cmd;
        int count;

        if(argc == 1) return 0;

        if(argc > 2) {
                errno = 0;
                count = (int)strtol(argv[1], NULL, 10);
                if(errno) {
                        errno = 0;
                        count = 1;
                        cmd = &argv[1];
                } else {
                        cmd = &argv[2];
                }
        } else {
                count = 1;
                cmd = &argv[1];
        }

        timespec base_time = run_cmd(count, progname(argv));
        timespec run_time = run_cmd(count, cmd);
        timespec total_time = time_sub(run_time, base_time);

        // fprintf(stderr, "%ld.%09lds\n", base_time.tv_sec, base_time.tv_nsec);
        // fprintf(stderr, "%ld.%09lds\n", run_time.tv_sec, run_time.tv_nsec);
        fprintf(stderr, "%ld.%09lds\n", total_time.tv_sec, total_time.tv_nsec);
}
