# writeup
It's just a newsest qemu run without -monitor.So you can use qemu-monitor to execve some commands.</br>
Once you enter into qemu monitor,you can use this to execve a command(The qemu process must run as root):</br>
netdev_add tap,id=net0,ifname=flag,script=/bin/cat</br>
