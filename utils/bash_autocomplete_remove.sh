set -e #Uninstall the bash tab completion installed by bash_autocomplete_setup.sh.
set -u
sudo rm -f /etc/bash_completion.d/frogatto
echo -e "\e[00;32mSuccess:\e[00m Tab completion for 'game' and 'frogatto' uninstalled."