## WHTAT IS THIS

> dump back trace like android tombstone when receive one of this signals:

- SIGSEGV
- SIGBUS
- SIGFPE
- SIGILL
- SIGSYS
- SIGTRAP

> features avaliable:

- dump back trace information
- dump thread information when code crash


## LIMITATIONS

- only works on linux(tested on Ubuntu14.04 LTS)
- add **`-g`** option for gcc compile flags

## INSTALL

```
sudo apt-get install libunwind-dev
make
make test
```
