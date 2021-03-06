#!/bin/bash
#
###############################################################
#                                                             #
# SUSE Linux Products GmbH 2013                               #
#                                                             #
# Interfaces monitor for wickedd                              #
#                                                             #
# Author: Pawel Wieczorkiewicz <pwieczorkiewicz@suse.de>      #
#                                                             #
###############################################################

EXPECTED_ARGS=0

STATE='state="%{?client-info/state}"'
CONFIG_ORIGIN='config-origin="%{?client-info/config-origin}"'
PERSISTENT='persistent="%{?client-state/persistent}"'
INIT_STATE='init-state="%{?client-state/init-state}"'
INIT_TIME='init-time="%{?client-state/init-time}"'
LAST_TIME='last-time="%{?client-state/last-time}"'

number='^[0-9]+$'
if [[ $# -lt $EXPECTED_ARGS || ! -z "$1" && ! $1 =~ $number ]]; then
	echo "Usage: `basename $0` [interval]"
	exit 1;
fi

while :; do

	if [[ ! -z "$1"  && $1 -ne 0 ]]; then
		clear;
	fi

	wicked show-xml | wicked xpath --reference '/object/interface' "name=\"%{?name}\" ${STATE} ${CONFIG_ORIGIN} ${PERSISTENT} ${INIT_STATE} ${INIT_TIME} ${LAST_TIME}" | column -t || exit;

	if [[ -z "$1"  || $1 -eq 0 ]]; then
		exit;
	fi

	sleep $1;
done
