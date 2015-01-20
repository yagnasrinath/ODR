
CC = gcc
CFLAGS = -g 
LIBS = /users/cse533/Stevens/unpv13e/libunp.a -lpthread
PROGS = client_110010304 server_110010304 odr_110010304 

all: $(PROGS)

.PHONY: clean

#Building message api
msgapi.o: msgapi.c
	${CC} $(CFLAGS) -c msgapi.c

#Building hwaddr code
get_hw_addrs.o: get_hw_addrs.c
	${CC} ${FLAGS} -c get_hw_addrs.c

#Building print hwaddr code
#prhwaddrs.o: prhwaddrs.c
#	${CC} ${FLAGS} -c prhwaddrs.c
#
#prhwaddrs: prhwaddrs.o get_hw_addrs.o
#	${CC} -o prhwaddrs prhwaddrs.o get_hw_addrs.o ${LIBS}

#Building client code
hosts.o: hosts.c
	${CC} $(CFLAGS) -c hosts.c

#Building client code
time_client.o: time_client.c
	${CC} $(CFLAGS) -c time_client.c

client_110010304: msgapi.o time_client.o get_hw_addrs.o hosts.o
	${CC} -o client_110010304 time_client.o msgapi.o hosts.o get_hw_addrs.o ${LIBS}

#Building client code
time_server.o: time_server.c
	${CC} $(CFLAGS) -c time_server.c

server_110010304: msgapi.o time_server.o get_hw_addrs.o hosts.o
	${CC} -o server_110010304 time_server.o msgapi.o hosts.o get_hw_addrs.o ${LIBS}

#Building ODR code
odr.o: odr.c
	${CC} $(CFLAGS) -c odr.c

odr_functions.o: odr_functions.c
	${CC} $(CFLAGS) -c odr_functions.c

odr_110010304: msgapi.o odr.o get_hw_addrs.o odr_functions.o hosts.o
	${CC} -o odr_110010304 msgapi.o get_hw_addrs.o hosts.o odr_functions.o odr.o ${LIBS}

clean:
	rm msgapi.o time_client.o time_server.o odr.o client_110010304 server_110010304 odr_110010304 odr_functions.o hosts.o
#	rm msgapi.o get_hw_addrs.o time_client.o time_server.o odr.o client_110010304 server_110010304 odr_110010304 
