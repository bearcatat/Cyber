# Cyber 
- 一个网络编程学习项目
- 参考[ZLToolKit](https://github.com/ZLMediaKit/ZLToolKit)学习如何使用C++实现Rector模型，这里感谢作者分享这个网络框架。
- 在此基础上写了一个简单的静态服务器

# AB压测
这里简单的使用ab进行压力测试
```
ab -c 24 -n 100000 -H "Connection: close" -r  "http://localhost:16557/"
```
结果如下：
- Time taken for tests:   7.943 seconds
- Complete requests:      100000
- Failed requests:        0
- Total transferred:      28000000 bytes
- HTML transferred:       15900000 bytes
- Requests per second:    12589.84 [#/sec] (mean)
- Time per request:       1.906 [ms] (mean)
- Time per request:       0.079 [ms] (mean, across all concurrent requests)
- Transfer rate:          3442.54 [Kbytes/sec] received
