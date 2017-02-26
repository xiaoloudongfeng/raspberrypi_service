基于树莓派的实时监控服务
==========================
##1.简介
树莓派的功能很强大，我的初衷只是想通过树莓派的GPIO，控制dht22获取室内温湿度，并显示在12864屏幕上<br>
写着写着顺手实现了实时监控树莓派的CPU、内存使用率的功能，并爬取了我所在地的天气<br>
12864屏幕的显示内容有：日期时间；CPU、内存利用率；室内温湿度；城市的天气（取自中国天气网）<br>
后来又顺手实现了一个异步的服务线程，将采集到的数据发送给Android的客户端<br>
反正脑洞是开不完的，估计还会添加一些功能，比如将采集到的数据存入redis之类的，以后再说吧，好玩才是最重要的<br>
写了一个systemd的脚本，可以通过systemctl来控制程序的启动，停止和重启，非常方便<br>
##2.依赖
GPIO库用的是bcm2835，dht22对时序要求比较严格，linux不是实时操作系统，延时读高低信号的时候会出现各种各样的问题<br>
这个库性能比较好，能降低出问题的概率，但是有另外一个问题，运行时需要root权限，可以通过修改特殊权限解决问题<br>
板子是Raspberry Pi 2，新老板子理论上可以运行，但是在读取dht22的时候可能会有点问题，需要修改一些东西<br>
我用的系统是ArchlinuxArm，官方系统没有试过，应该也能跑<br>
##3.联系方式
我的邮箱：xiaoloudongfeng@gmail.com<br>
