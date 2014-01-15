cp anura anura~
make clean && time nice -n 19 make "-j$(nproc)"
kdialog --msgbox "make finished"