# qhttp - a HTTP key-value server
qhttp (quick-http) is a HTTP server which can store and get key-value pairs. Client can get values with 'GET' mehod of HTTP request using customized keys, or store key-value pairs with 'POST' method of HTTP request.

I use [wrk](https://github.com/wg/wrk) (a modern HTTP benchmarking tool capable of generating significant load) to test qhttp's performance. The test result is in comparison with the result which is generated from [lighttpd](https://www.lighttpd.net/) by wrk in the same condition.

# Source Code File Introduction
* **run.cc**<br>contain main program
* **server.h**<br>a head file related to dealing with connection acception and data processing
* **server.cc**<br>realize method in server.h
* **epoll_event.h** <br>a head file related to dealing with system calls of epoll series
* **epoll_event.cc**<br>realize method in epoll_event.h
* **map.txt**<br>This file store key-value pairs, where keys are string and values are integer. As a demo, the key-value in this file means a city and the first two digits of correspoding ID number.

# Installation
1. Get the source code from this repository.
```
git clone https://github.com/zeyuanqiu/qhttp.git
```
2. Change current directory to 'build'.
```
cd qhttp/build
```
3. Compile.
```
cmake .. && make
```

# Run
1. Run the executable file.
```
./run <ip_address> <port_number>
```
2. Enter the uri below in the browser to get the values according to the keys 'beijing', 'tianjin' and 'shanxi'.
```
<ip_address>:<port_number>/?key=beijing&key=tianjin&key=shanxi
```
3. Check the result from browser.
```
key = beijing , value = 11 
key = tianjin , value = 12 
the key shanxi is not existed 
```
4. Check the log from the server console. I separate it below to see it clearly.
* Accept connection and read request from client.
```
------ INIT CONNECTION ------ 
------ ACCEPT CONNECTION ------ 
------ INIT CONNECTION ------ 
[WARN] Accept no connection 
------ READ REQUEST FROM CLIENT BEGIN ------ 
[INFO] Read client request data completed 
[INFO] Data from client 
GET /?key=beijing&key=tianjin&key=shanxi HTTP/1.1
Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8
Accept-Language: zh-Hans-CN,zh-Hans;q=0.5
Upgrade-Insecure-Requests: 1
User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/70.0.3538.102 Safari/537.36 Edge/18.18362
Accept-Encoding: gzip, deflate
Host: 9.134.113.239:50000
Proxy-Connection: Keep-Alive
connection: keep-alive

 
------ READ REQUEST FROM CLIENT END ------
```
* Parse HTTP request line.
```
------ REQUEST LINE PARSING START ------ 
[Info] Request method: GET 
[INFO] Get 3 key(s) from client 
[INFO] The key 1 is beijing 
[INFO] The key 2 is tianjin 
[INFO] The key 3 is shanxi 
[INFO] Request version: HTTP/1.1 
------ REQUEST LINE PARSING END ------
```
* Parse HTTP request head.
```
------ REQUEST HEAD PARSING START ------ 
[INFO] Request accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8 
[INFO] Request accept_language: zh-Hans-CN,zh-Hans;q=0.5 
[INFO] Request upgrade_insecure_requests: 1 
[INFO] Request user_agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/70.0.3538.102 Safari/537.36 Edge/18.18362 
[INFO] Request accept_encoding: gzip, deflate 
[INFO] Request host: 9.134.113.239:50000 
[INFO] Request proxy_connection: Keep-Alive 
[INFO] Request connection: keep-alive 
------ REQUEST HEAD PARSING END ------ 
```
* Parse HTTP request body.
```
------ REQUEST BODY PARSING START ------ 
[WARN] HTTP request content is empty 
------ REQUEST BODY PARSING END ------ 
```
* Integrate HTTP response and send it.
```
------ REQUEST RESPONSE SEND START ------ 
[INFO] The response is formatted 
HTTP/1.1 200 OK
Content-Length: 122
Content-Type: text/html

<html><body>key = beijing , value = 11 <br>key = tianjin , value = 12 <br>the key shanxi is not existed <br></html></body> 
------ REQUEST RESPONSE SEND END ------
```
5. Enter the script below in the **browser console** to post key-value pairs to the server. As you can see, I push three pairs of key-value to the server here.
```
fetch(new Request('url',{
    method:'POST', 
    headers: {'Content-Type': 'application/x-www-form-urlencoded'},
    body:"key=shanxi&value=61&key=gansu&value=62&key=qinghai&value=63"
})).then((resp)=>{console.log(resp)})
```
6. Chek the log information from the server console.
* Accept connection and read request from client.
```
------ ACCEPT CONNECTION ------ 
------ INIT CONNECTION ------ 
[WARN] Accept no connection 
------ READ REQUEST FROM CLIENT BEGIN ------ 
[INFO] Read client request data completed 
[INFO] Data from client 
POST /url HTTP/1.1
Origin: http://9.134.113.239:50000
Referer: http://9.134.113.239:50000/?key=beijing&key=tianjin&key=shanxi
User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/70.0.3538.102 Safari/537.36 Edge/18.18362
Accept: */*
Accept-Language: zh-Hans-CN,zh-Hans;q=0.5
content-type: application/x-www-form-urlencoded
Accept-Encoding: gzip, deflate
Content-Length: 59
Host: 9.134.113.239:50000
Proxy-Connection: Keep-Alive
Pragma: no-cache
connection: keep-alive

key=shanxi&value=61&key=gansu&value=62&key=qinghai&value=63 
------ READ REQUEST FROM CLIENT END ------
```
* Parse HTTP request line.
```
------ REQUEST LINE PARSING START ------ 
[Info] Request method: POST 
[INFO] Request uri: /url 
[INFO] Request version: HTTP/1.1 
------ REQUEST LINE PARSING END ------
```
* Parse HTTP request head.
```
------ REQUEST HEAD PARSING START ------ 
[INFO] Request user_agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/70.0.3538.102 Safari/537.36 Edge/18.18362 
[INFO] Request accept: */*, like Gecko) Chrome/70.0.3538.102 Safari/537.36 Edge/18.18362 
[INFO] Request accept_language: zh-Hans-CN,zh-Hans;q=0.5 
[INFO] Request accept_encoding: gzip, deflate 
[INFO] Request content-length: 59 
[INFO] Request host: 9.134.113.239:50000 
[INFO] Request proxy_connection: Keep-Alive 
[INFO] Request connection: keep-alive 
------ REQUEST HEAD PARSING END ------
```
* Parse HTTP request body.
```
------ REQUEST BODY PARSING START ------ 
[INFO] Insert into the map: key = shanxi, value = 61 
[INFO] Insert into the map: key = gansu, value = 62 
[INFO] Insert into the map: key = qinghai, value = 63 
------ REQUEST BODY PARSING END ------
```
* Integrate HTTP response and send it.
```
------ REQUEST RESPONSE SEND START ------ 
[INFO] The response is formatted 
HTTP/1.1 200 OK
Content-Length: 147
Content-Type: text/html

<html><body>The key-value you instered <br>key = shanxi , value = 61 <br>key = gansu , value = 62 <br>key = qinghai , value = 63 <br></html></body> 
------ WRITE INTO THE FILE START ------ 
------ WRITE INTO THE FILE END ------ 
------ REQUEST RESPONSE SEND END ------
```
# Test with wrk
Test the server with wrk and compare the result with that generated from lighttpd.
1. Stop all the output. Just comment out all the 'printf'  functions in the file, 'server.cc'.
2. Recompile the source code and run the executable file.
```
cmake .. && make && ./run 127.0.0.1 50000
```
3. Open a new  console and test the server using wrk.
```
wrk -t12 -d30s -c200 http://127.0.0.1:50000/?key=beijing\&key=randomcity
```
As we can see, the parameter `Requests/sec` below the report is 55630.89.
```
Running 30s test @ http://127.0.0.1:50000/?key=beijing&key=randomcity
  12 threads and 200 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     11.98ms   78.91ms 417.26ms   97.50%
    Req/Sec     11.19k     2.78k   23.57k    70.20%
  1451884 requests in 30.10s, 0.00B read
  Socket errors: connect 0, read 1669459, write 0, timeout 278
Requests/sec:  55630.89
Transfer/sec:       0.00B
```
4. The report below is from lighttpd with the same tool, wrk, in the same condition.
```
Running 30s test @ http://9.134.113.239:49999
  12 threads and 200 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     2.92ms  403.89us   9.64ms   96.82%
    Req/Sec     5.51k     1.16k   73.83k    99.94%
  1974695 requests in 30.10s, 409.01MB read
Requests/sec:  65610.13
Transfer/sec:     13.59MB
```
The parameter 'Requests/sec' of lighttpd is 65610.13, around six fifth of that from qhttp.

5. Test qhttp with 'POST' method using Lua script. Before you do this, comment out the module of file writing (around line 534-545 in server.cc).
```
// printf("------ WRITE INTO THE FILE START ------ \n");
// char seq[FILE_SEQ_LEN];
// memset(seq, '\0', FILE_SEQ_LEN);
// for (it = data_map.begin(); it != data_map.end(); it++)
// {
//     sprintf(seq, "%s%s %d\n", seq, it->first.c_str(), it->second);
// }

// int seq_len = strlen(seq);
// ofstream ofs;
// ofs.open("map.txt");
// ofs.write(seq, seq_len);
// ofs.close();
// printf("------ WRITE INTO THE FILE END ------ \n");
```
The following result looks good. However, it must leave space to improve.
```
Running 30s test @ http://127.0.0.1:50000
  12 threads and 200 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     2.89ms   17.61ms 667.75ms   98.51%
    Req/Sec     4.24k     2.06k   31.22k    77.31%
  Latency Distribution
     50%    1.02ms
     75%    1.05ms
     90%    1.10ms
     99%   84.74ms
  1489294 requests in 30.10s, 0.00B read
  Socket errors: connect 0, read 1489294, write 0, timeout 0
Requests/sec:  49479.22
Transfer/sec:       0.00B
```

# Optimization Process
**Use 'Requests/sec' from GET method as an indicator.**
* When I used `netstat -nap` to check the port status, I found that lots of ports are in 'CLOSE_WAIT' status. To solve this problem, I closed the connection after two conditions, the client close the connection actively or data transfer from the server is completed. The parameter 'Requests/sec' increased from around 15000 to 35000.
* I added the socket attribute 'SO_REUSEADDR' to all the sockets. The parameter 'Requests/sec' increased from around 35000 to 50000.
* I stopped all the output. The parameter 'Requests/sec' increased from around 50000 to 55000.

# Acknowledgements
Thanks to my mentor, **lennonyang**, for his selfless guidance!