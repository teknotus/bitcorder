#
# Regular cron jobs for the bitcorder package
#
0 4	* * *	root	[ -x /usr/bin/bitcorder_maintenance ] && /usr/bin/bitcorder_maintenance
