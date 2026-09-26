HOSTNAME=$(hostname)
if [ "$(id -u)" -eq 0 ]; then
	PS1="root@${HOSTNAME}# "
else
	PS1="$(id -un)@${HOSTNAME}\$ "
fi
export PS1
HISTFILE="${HOME}/.ash_history"
HISTFILESIZE=100
export HISTFILE HISTFILESIZE
unset HOSTNAME