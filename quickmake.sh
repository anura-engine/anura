cp anura anura~
make clean && time make "-j$(nproc)"
kdialog --msgbox "make finished"