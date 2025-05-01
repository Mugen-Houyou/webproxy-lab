# CS:APP Proxy Lab

## Student Source Files

This directory contains the files you will need for the CS:APP Proxy Lab.

---

### Starter Files

- **proxy.c**
- **csapp.h**
- **csapp.c**

These are starter files. `csapp.c` and `csapp.h` are described in your textbook. You may make any changes you like to these files. You may also create and hand in any additional files you like.

Please use `port-for-user.pl` or `free-port.sh` to generate unique ports for your proxy or tiny server.

---

### Build Instructions

- **Makefile**  
  This is the makefile that builds the proxy program.  
  - Type `make` to build your solution.  
  - Type `make clean` followed by `make` for a fresh build.  
  - Type `make handin` to create the tarfile that you will be handing in.  

You can modify it in any way you like. Your instructor will use your Makefile to build your proxy from source.

---

### Helper Scripts

- **port-for-user.pl**  
  Generates a random port for a particular user.  
  **Usage:** `./port-for-user.pl <userID>`

- **free-port.sh**  
  Handy script that identifies an unused TCP port for your proxy or tiny.  
  **Usage:** `./free-port.sh`

- **driver.sh**  
  The autograder for Basic, Concurrency, and Cache.  
  **Usage:** `./driver.sh`

- **nop-server.py**  
  Helper for the autograder.

---

### Additional Resource

- **tiny**  
  Tiny Web server from the CS:APP text.
