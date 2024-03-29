VERSION = 0.2.1
BIN_NAME = utminidec.a

INCS = -I.
LIBS =

# flags
CPPFLAGS = 
CFLAGS   = -std=c17 -pedantic -Wall -Wno-deprecated-declarations -Os ${INCS} ${CPPFLAGS}
LDFLAGS  = ${LIBS}

CC = gcc

