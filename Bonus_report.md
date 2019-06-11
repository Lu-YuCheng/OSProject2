## Bonus

`On VirtualBox: Ubuntu 16.04, kernel version 4.15.0, with 1 allocated CPU`

We refer to “miniex” in TA’s slides to implement asynchronous I/O with workqueue. The concept of workqueue itself is quite simple. The core of issue is, nonetheless, that what we shall regard as an intact “work”, which would directly define what our asynchronous I/O mechanism is.

Initially, we tried creating work_handler function to pack the work inside. This did not take notable effect. So we turned to pack some function coming in handy. This somehow ran fine.

The work we define is basically the I/O between kernel socket and I/O method we designate (fcntl or mmap) in argument list. Two pictures shown below are our simulation: first one is synchronous I/O, and second one is asynchronous I/O.

![Synchronous I/O](https://raw.githubusercontent.com/Lu-YuCheng/OSProject2/master/photo_collection/62194067_332755374083625_4157321969349427200_n.png)

![ASynchronous I/O](https://raw.githubusercontent.com/Lu-YuCheng/OSProject2/master/photo_collection/62239269_2269922256456707_3956757261598064640_n.png)

|      | Master | Slave |    Time(ms)    | Ratio(Syn/Asyn) |
|:----:|:------:|:-----:|:----------:|:-----------------:|
|  Syn |  fcntl | fcntl |  6.366000  |                   |
| Asyn |  fcntl | fcntl |  5.292600  |       120.3%      |
|  Syn |  mmap  | fcntl |  7.011200  |                   |
| Asyn |  mmap  | fcntl |  4.955100  |       141.4%      |
|  Syn |  fcntl |  mmap | 928.790300 |                   |
| Asyn |  fcntl |  mmap |  16.396800 |       5664%       |
|  Syn |  mmap  |  mmap | 935.238300 |                   |
| Asyn |  mmap  |  mmap |  15.327100 |       6101%       |

For visual convenience, we got the result into table. Notice that because "master.c" runs before "slave.c", we adopted the slave result time, whileas master result time may contain user’s delay. As shown in the table above, in the first case (fcntl/fcntl) the speed of asyn is 120.3% of one of syn, i.e. asyn one is 20.3% faster than syn one. Second case (mmap/fcntl) also appears to be similar, which is 41.4% faster (Not that similar, though. But comparing to two coming cases). It seems to be satisfying, since the scale of optimization is large enough in real life if we can accelerate I/O by 20 ~ 40 percent. However, what’s hardly stunning is, we speed up the cases with slave running in mmap by 5564% and 6001%, by merely changing I/O policy from synchronous to asychronous!!
