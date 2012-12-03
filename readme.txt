Crypt-Bruteforce is a tool for broot standart linux password, encrypted with crypt function. It supports multiple modes:
1. Single mode. Brute password in a single thread.
2. Multi mode. Brute password in multiple threads.
3. Synchronous client-server mode. Server generate tasks for bruteforce, send them to clients and receive results.
Also, Crypt-Bruteforce supports asynchronous server mode, but it is unstable.

Run params:
-s -- Single mode.
-m -- Multi mode. [default]
-c -- Synchronous client mode.
-t -- Synchronous server mode.
-x -- Asynchronoys server mode.
-h string -- Hash to brute.
-b integer -- Block length. Can't be negative. [default = 1]
-l integer -- Maximum length. Can't be less then or equal to zero. [default = 6]
-p integer -- Port. Must be in range from 1 to 65536. [default = 3456]
-a string -- Address. [default = 0.0.0.0]

Hash param required for all modes except client mode. All other params are optional.

Created by Artem Sedanov, 2012.
funky.cat.sam@gmail.com
