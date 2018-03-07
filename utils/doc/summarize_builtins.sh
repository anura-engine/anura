#!/bin/sh


STATUS_ERR_UNSATISFIED_PRECOND=1

STATUS_SUCCESS=0


#   TODO Add the capability of being run from any directory.
if test "`pwd | sed 's/.*\///'`" != 'doc'
then
    echo "Still not supporting launching from different directory than '.'."
    exit ${STATUS_ERR_UNSATISFIED_PRECOND}
fi

./do.py >builtins_summary.md
exit ${STATUS_SUCCESS}
