rm '/home/harry/CS360-Final/a.out'
sudo dd if=/dev/zero of=diskimage bs=1024 count=1440
sudo mke2fs -b 1024 diskimage 1440
sudo chmod 777 diskimage
cc /home/harry/CS360-Final/main.c

'/home/harry/CS360-Final/a.out'
