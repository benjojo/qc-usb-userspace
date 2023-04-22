#!/bin/sh
ipcs
ipcs -m|grep `whoami`|cut -d ' ' -f 2|xargs ipcrm shm
