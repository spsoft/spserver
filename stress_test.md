# Introduction #

Make a stress test for spserver/testsmtp


# Details #
```
bash # ./teststress -p 1025 -c 2000 -m 100
Create 2000 connections to server, it will take some minutes to complete.
.........................................................................................
.........................................................................................
......................
waiting for 2000 client(s) to exit
waiting for 2000 client(s) to exit
waiting for 2000 client(s) to exit
waiting for 1464 client(s) to exit


Test result :
Clients : 2000, Messages Per Client : 100
ExecTimes: 22.290628 seconds

client  Send    Recv
total : 200000  202000
```





