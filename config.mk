VERSION = 0.1.0
BIN_NAME = utminidec.a

INCS =
LIBS =

# flags
CPPFLAGS = 
CFLAGS   = -std=c17 -pedantic -Wall -Wno-deprecated-declarations -Os ${INCS} ${CPPFLAGS}
LDFLAGS  = ${LIBS}

CC = gcc

