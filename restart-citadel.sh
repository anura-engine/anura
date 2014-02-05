#Trap the killer signals so that we can exit with a good message.
trap "error_exit 'Received signal SIGHUP'" SIGHUP
trap "error_exit 'Received signal SIGINT'" SIGINT
trap "error_exit 'Received signal SIGTERM'" SIGTERM

#Alias the function so that it will print a message with the following format:
#prog-name(@line#): message
#We have to explicitly allow aliases, we do this because they make calling the
#function much easier (see example).
shopt -s expand_aliases
alias die='error_exit "Error ${0}(@`echo $(( $LINENO - 1 ))`):"'

LISTEN_PORT=23056
SERVER_NAME="anura"

# Kill exisiting server
OLDPID=`ps aux | grep "[.]/$SERVER_NAME --module=Citadel --utility=tbs_server --port $LISTEN_PORT" | tr -s " " | cut -d " " -f 2`
if [ -n "$OLDPID" ] ; then
  if ! kill $OLDPID ; then
    die "Couldn't kill old process"
  fi
fi

cd modules/Citadel && git pull && cd ../..
if [ $? != "0" ] ; then
  die "git pull failed ..."
fi


nohup $HOME/steam-runtime-sdk/runtime/run.sh ./$SERVER_NAME --module=Citadel --utility=tbs_server --port $LISTEN_PORT  > nohup.citadel.out &
if [ $? != "0" ] ; then
  die "Failed to run server on port $LISTEN_PORT"
fi

PID=`ps aux | grep "[.]/$SERVER_NAME --module=Citadel --utility=tbs_server --port $LISTEN_PORT" | tr -s " " | cut -d " " -f 2`
if [ -z $PID ] ; then
  echo "There was a problem getting the process ID of the running server."
else
  echo "Citadel server now running, PID=$PID"
fi
