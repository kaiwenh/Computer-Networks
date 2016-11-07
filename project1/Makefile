CC=gcc
CFLAGS=-c -Wall
LDFLAGS=
SOURCES=webserver.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=webserver

all: $(SOURCES) $(EXECUTABLE)
    
$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.c.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	$(RM) webserver
	$(RM) webserver.o
	$(RM) *~
