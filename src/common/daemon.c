#ifndef _GNU_SOURCE
	//for asprintf
	#define _GNU_SOURCE
#endif

#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <err.h>
#include <stdio.h>
#include <sys/file.h>
#include <stdarg.h>

#include "daemon.h"
#include "../common/bstr.h"
#include "boithohome.h"

#define CANT_IGNORE_SIGCHLD 1;

const int TIMEOUT = 15; /* or whatever... */ 

    /*
    ** server.c -- a stream socket server liabery
    ** Mye av koden er hentet fra http://www.ecst.csuchico.edu/~beej/guide/net/html/
    */


void sigchld_handler(int sig __attribute__((unused))) {
	int s;
    	while (waitpid(-1,&s,WNOHANG) > 0)
     
	continue;
}

int socketsendsaa(int socketha,char **respons_list[],int nrofresponses) {

	int i,len;	

	fprintf(stderr, "daemon: socketsendsaa(socket=%i)\n", socketha);
	printf("socketsendsaa: nr %i\n",nrofresponses);

	sendall(socketha,&nrofresponses, sizeof(int));

	for (i=0;i<nrofresponses;i++) {
		len = (strlen((*respons_list)[i]) +1); //+1 da vi sender \0 ogsaa

		if (!sendall(socketha,&len,sizeof(len))) {
			perror("sendall");
			return 0;
		}

		if (!sendall(socketha,(*respons_list)[i],len)) {
			perror("sendall");
			return 0;
		}
	}

	return 1;
}

//motar en enkel respons liste. Den begynner med en int som sier hov lang den er
int socketgetsaa(int socketha,char **respons_list[],int *nrofresponses) {

	fprintf(stderr, "daemon: socketgetsaa(socket=%i)\n", socketha);

        int intresponse,i,len;

        if (!recvall(socketha,&intresponse,sizeof(intresponse))) {
                return 0;
        }

        #ifdef DEBUG
		printf("nr of elements %i\n",intresponse);
        #endif

        (*respons_list) = malloc((sizeof(char *) * intresponse) +1);
        (*nrofresponses) = 0;

        for (i=0;i<intresponse;i++) {
		//laster ned hvor lang den er
		if (!recvall(socketha,&len,sizeof(len))) {
                	return 0;
        	}
		(*respons_list)[(*nrofresponses)] = malloc(len +1);

                if (!recvall(socketha,(*respons_list)[(*nrofresponses)],len)) {
                        return 0;
                }

                #ifdef DEBUG
			printf("record \"%s\"\n",(*respons_list)[(*nrofresponses)]);
                #endif


                ++(*nrofresponses);
        }

        (*respons_list)[(*nrofresponses)] = '\0';

	return 1;
}

//rutine som binder seg til PORT og kaller sh_pointer hver gang det kommer en ny tilkobling
int sconnect (void (*sh_pointer) (int), int PORT, int noFork, int breakAfter) {

        int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
        struct sockaddr_in my_addr;    // my address information
        struct sockaddr_in their_addr; // connector's address information
        socklen_t sin_size;
        struct sigaction sa;
        int yes=1;
	pid_t session;
	pid_t leader;
	static int count = 0;

	leader = getpid();
	session = getpgrp();
	printf("We are: %d\n", session);

	fprintf(stderr, "daemon: sconnect(port=%i)\n", PORT);

        if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
            perror("socket");
            exit(1);
        }

  	#ifndef NO_REUSEADDR
       		if (setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1) {
              		perror("setsockopt");
              		exit(1);
        	}
	#endif

	#ifdef DEBUG
		printf("will listen on port %i\n",PORT);
        #endif

        my_addr.sin_family = AF_INET;         // host byte order
        my_addr.sin_port = htons(PORT);     // short, network byte order
        my_addr.sin_addr.s_addr = INADDR_ANY; // automatically fill with my IP
        memset(&(my_addr.sin_zero), '\0', 8); // zero the rest of the struct

        if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) == -1) {
	    fprintf(stderr,"Can't bind to port %i\n",PORT);
            perror("bind");
            exit(1);
        }

        if (listen(sockfd, BACKLOG) == -1) {
            perror("listen");
            exit(1);
        }

	
	// http://groups.google.com/groups?hl=no&lr=&c2coff=1&rls=GGLD,GGLD:2004-19,GGLD:en&selm=87iuv0b7td.fsf%40erlenstar.demon.co.uk&rnum=2
	// bare ignorerer signalet og lar barne d� hvis dette er ok. P� rare platformer kan vi f� problemer
	// m� i s�falt gj�re om flagget
	#ifdef CANT_IGNORE_SIGCHLD
		//sliter met at denne ikke fungerer. M� fikses om vi skal kj�re p� andre plattformer
		printf("CANT_IGNORE_SIGCHLD: on\n");
		sa.sa_handler = sigchld_handler; // reap all dead processes
		sa.sa_flags = SA_RESTART;		
	#else
		printf("CANT_IGNORE_SIGCHLD: off\n");
	   	sa.sa_handler = SIG_IGN;
	    	sa.sa_flags = 0;
	#endif
	sigemptyset(&sa.sa_mask);
	
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
                        perror("sigaction");
                        exit(1);
                }

	printf("bind to port %i ok. Antering accept loop\n",PORT);
        while(1) {  // main accept() loop
            sin_size = sizeof(struct sockaddr_in);
            if ((new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size)) == -1) {
                perror("accept");
                continue;
            }
            printf("server: got connection from %s\n", inet_ntoa(their_addr.sin_addr));
	    #ifdef DEBUG
		printf("runing in debug mode\n");
		#ifdef DEBUG_WITH_FORK
			printf("roning in debug but form mode (DEBUG_WITH_FORK)\n");

	            if (!fork()) { // this is the child process
			printf("Forket to new prosses\n");
	
	                close(sockfd); // child doesn't need the listener
	                sh_pointer(new_fd);
                
			close(new_fd);

			printf("server: closeing connection from %s\n", inet_ntoa(their_addr.sin_addr));

	                exit(0);
	            }
		    close(new_fd);  // parent doesn't need this

		#else
			printf("runing in debug mode. Wont fork. (problematik to us gdb then)\n");
                	sh_pointer(new_fd);
			close(new_fd);
			printf("sconnect: socket closed\n");

		#endif
	    #else
		if (noFork) {
			printf("Not in debug mode, but Wont fork.\n");
                        sh_pointer(new_fd);
                        close(new_fd);
			printf("sconnect: socket closed\n");
		}
		else {
		    	printf("runing in normal fork mode\n");

        	    	if (!fork()) { // this is the child process
				if (setpgid(getpid(), session) == -1)
					warn("setpgid");
				printf("Forket to new prosses\n");

        		        close(sockfd); // child doesn't need the listener
        		        sh_pointer(new_fd);
                
				close(new_fd);

				printf("server: closeing connection from %s\n", inet_ntoa(their_addr.sin_addr));

            		    exit(0);
            		}
	    		close(new_fd);  // parent doesn't need this
		}
	    #endif

		if (breakAfter != 0) {
			++count;
	
        		if (count >= breakAfter) {
				printf("exeting after %i connects\n",count);
				break;
			}
		}
        }

	fprintf(stderr, "daemon: ~sconnect()\n");
        return 0;
} 


#ifdef WITH_DAEMON_THREAD

#ifndef WITH_THREAD
	#error "Hvis man skal bruke WITH_DAEMON_THREAD, m� man ogs� definere WITH_THREAD. Definer WITH_THREAD"
#endif
//rutine som binder seg til PORT og kaller sh_pointer hver gang det kommer en ny tilkobling
// Threaded
int sconnect_thread(void (*sh_pointer)(int), int port) {
	int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
	struct sockaddr_in my_addr;    // my address information
	struct sockaddr_in their_addr; // connector's address information
	socklen_t sin_size;
	int yes=1;

	#ifdef DEBUG_BREAK_AFTER
		static count = 0;
	#endif

	fprintf(stderr, "daemon: sconnect_thread(port=%i)\n", port);

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}

	if (setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1) {
		perror("setsockopt");
		exit(1);
	}

	#ifdef DEBUG
		printf("will listen on port %i\n", port);
	#endif

	my_addr.sin_family = AF_INET;         // host byte order
	my_addr.sin_port = htons(port);     // short, network byte order
	my_addr.sin_addr.s_addr = INADDR_ANY; // automatically fill with my IP
	memset(&(my_addr.sin_zero), '\0', 8); // zero the rest of the struct

	if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) == -1) {
		fprintf(stderr,"Can't bind to port %i\n", port);
		perror("bind");
		exit(1);
	}

	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	printf("Bound to port %d ok. Antering accept loop\n", port);
	for (;;) {
		sin_size = sizeof(struct sockaddr_in);
		if ((new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size)) == -1) {
			perror("accept");
			continue;
		}
		printf("server: got connection from %s\n", inet_ntoa(their_addr.sin_addr));
		#ifdef DEBUG
			printf("runing in debug mode\n");
			printf("runing in debug mode. Will not split out in several threads.\n");
			sh_pointer(new_fd);
			printf("sconnect: socket closed\n");
		#else
			pthread_t thread;
			printf("runing in normal thread mode\n");

			pthread_create(&thread, NULL, sh_pointer, (void *)new_fd);
			printf("Thread spawned...\n");
			pthread_detach(thread);
		#endif

		#ifdef DEBUG_BREAK_AFTER
			++count;
	
        		if (count >= DEBUG_BREAK_AFTER) {
				printf("exeting after %i connects\n",count);
				break;
			}
		#endif

	}

	fprintf(stderr, "daemon: ~sconnect()\n");
	return 0;
} 

#endif

//rutine som binder seg til PORT og kaller sh_pointer hver gang det kommer en ny tilkobling
int sconnect_simple(void (*sh_pointer)(int), int port)
{
	int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
	struct sockaddr_in my_addr;    // my address information
	struct sockaddr_in their_addr; // connector's address information
	socklen_t sin_size;
	int yes=1;

	#ifdef DEBUG_BREAK_AFTER
		static count = 0;
	#endif

	fprintf(stderr, "daemon: sconnect_thread(port=%i)\n", port);

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}

	if (setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1) {
		perror("setsockopt");
		exit(1);
	}

	#ifdef DEBUG
		printf("will listen on port %i\n", port);
	#endif

	my_addr.sin_family = AF_INET;         // host byte order
	my_addr.sin_port = htons(port);     // short, network byte order
	my_addr.sin_addr.s_addr = INADDR_ANY; // automatically fill with my IP
	memset(&(my_addr.sin_zero), '\0', 8); // zero the rest of the struct

	if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) == -1) {
		fprintf(stderr,"Can't bind to port %i\n", port);
		perror("bind");
		exit(1);
	}

	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	printf("Bound to port %d ok. Antering accept loop\n", port);
	for (;;) {
		sin_size = sizeof(struct sockaddr_in);
		if ((new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size)) == -1) {
			perror("accept");
			continue;
		}
		printf("server: got connection from %s\n", inet_ntoa(their_addr.sin_addr));
		sh_pointer(new_fd);

		close(new_fd);

		#ifdef DEBUG_BREAK_AFTER
			++count;
	
        		if (count >= DEBUG_BREAK_AFTER) {
				printf("exeting after %i connects\n",count);
				break;
			}
		#endif

	}

	fprintf(stderr, "daemon: ~sconnect()\n");
	return 0;
} 

int cconnect (char *hostname, int PORT) {

        int sockfd;
	int sockedmode;
	int Conectet;
        struct hostent *he;
        struct sockaddr_in their_addr; // connector's address information 

	#ifdef DEBUG
		fprintf(stderr, "daemon: cconnect(hostname=\"%s\", port=%i)\n", hostname, PORT);
	#endif


        if ((he=gethostbyname(hostname)) == NULL) {  // get the host info 
		perror("gethostbyname");
		#ifdef DEBUG
			fprintf(stderr, "daemon: ~cconnect()\n");
		#endif
		return(0);
        }

        if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		#ifdef DEBUG
			fprintf(stderr, "daemon: ~cconnect()\n");
		#endif
		return(0);
        }

        their_addr.sin_family = AF_INET;    // host byte order 
        their_addr.sin_port = htons(PORT);  // short, network byte order 
        their_addr.sin_addr = *((struct in_addr *)he->h_addr);
        memset(&(their_addr.sin_zero), '\0', 8);  // zero the rest of the struct 


	// Set sockfd to non-blocking mode.  
	sockedmode = fcntl (sockfd, F_GETFL, 0); 
	fcntl (sockfd, F_SETFL, sockedmode | O_NONBLOCK); 
	
	Conectet = 0;
	
	// Establish non-blocking connection server.  
	if (connect(sockfd, (struct sockaddr *)&their_addr, sizeof(struct sockaddr)) == -1) { 
		if (errno == EINPROGRESS) { 
			struct timeval tv = {TIMEOUT, 0}; 
			fd_set rd_fds, wr_fds; 
			FD_ZERO (&rd_fds); 
			FD_ZERO (&wr_fds); 
			FD_SET (sockfd, &wr_fds); 
			FD_SET (sockfd, &rd_fds); 

			// Wait for up to TIMEOUT seconds to connect. 
			if (select (sockfd + 1, &rd_fds, &wr_fds, 0, &tv) <= 0) {
				#ifdef DEBUG
					fprintf(stderr, "daemon: ~cconnect()\n");
				#endif
				return 0;
			}
			// Can use getpeername() here instead of connect(). 
			else if (connect (sockfd, (struct sockaddr *) &their_addr, sizeof their_addr) == -1 && errno != EISCONN) { 
				#ifdef DEBUG
					fprintf(stderr, "daemon: ~cconnect()\n");
				#endif
				return 0;
			}
			else {
				Conectet = 1;
			}
		} 
	}

	fcntl (sockfd, F_SETFL, sockedmode);
	
	#ifdef DEBUG
		fprintf(stderr, "daemon: ~cconnect(socket=%i)\n", sockfd);
	#endif

	return sockfd;
} 


int sendall(int s, const void *buf, int len) {

        int total = 0;        // how many bytes we've sent
        int bytesleft = len; // how many we have left to send
        int n;
	int tosend;
	
	#ifdef DEBUG
		fprintf(stderr, "sendall(s=%i, len=%i)\n",s,len);
	#endif

        while(total < len) {

		if (bytesleft > 16392) {
			tosend = 16392;
		}
		else {
			tosend = bytesleft;
		}

		if ((n = send(s, buf+total, tosend, MSG_NOSIGNAL)) == -1) {

			fprintf(stderr,"sendall: send() in main while loop. total=%i, len %i\n",total, len);
			perror("send()");
			return 0;
		}

		if (n == -1) { 
			printf("dident manage to send all the data as %s:%d.\n",__FILE__,__LINE__);
 
			return 0;
		}
	    
		total += n;
		bytesleft -= n;
        }


	return total;
}

/************************************************************************************

 Ruting som tar inn data som skal sende, pakker de i en minne blok, og sender de som en send. Dette _kan_ �ke socket 
 hastigheten hvis vi kj�rer med TCP_NODELAY

 eks: 
	sendallPack(socket, 4, &n, sizeof(n), collections, (maxSubnameLength+1)*n);
************************************************************************************/
int sendallPack(int s, int numargs, ...) {

	va_list listPointer;
	void *p;
	void *blockToSend;
	int size = 0;
	int i;
	void *data;
	int datasize;
	int ret;

	#ifdef DEBUG
	fprintf(stderr,"sendallPack(numargs=%i) \n",numargs);
	#endif

	/***********************************************
	 g�r gjenom dataene en gang for � finne total st�relse
	***********************************************/

	va_start( listPointer, numargs );

    	for( i = 0 ; i < numargs; i+=2)
    	{
        	data = va_arg( listPointer, void* );
        	datasize = va_arg( listPointer, int );

		size += datasize;
	}

	va_end( listPointer );


	/***********************************************
	 oppretter minne for � kopiere data inn i
	***********************************************/


	if ((blockToSend = malloc(size)) == NULL) {
		perror("sendallPack: malloc p");
		return 0;
	}
	p = blockToSend;

	/***********************************************
	 g�r gjenom dataene en gang til for � kopiere de inn
	***********************************************/

	va_start( listPointer, numargs );

    	for( i = 0 ; i < numargs; i+=2)
    	{
        	data = va_arg( listPointer, void* );
        	datasize = va_arg( listPointer, int );

		#ifdef DEBUG
			fprintf(stderr,"i=%i, datasize=%i, p=%p, size=%i\n",i, datasize, p, size);
		#endif

		memcpy(p,data,datasize);
		p += datasize;
	}

	va_end( listPointer );

	/***********************************************
	 N� n�r vi har lagget en pakke ut av det, gj�r vi det faktisk send kallet.
	***********************************************/

	ret = sendall(s,blockToSend,size);
	free(blockToSend);

	return ret;

}

int recvall(int sockfd, void *buf, int len) {

	int total = 0;
        int bytesleft = len; // how many we have left to send
	int n;

	#ifdef DEBUG
		printf("will read %i",len);
	#endif

	while(total < len) {

		//runarb: 17.07.2007
		//hum, n�r vi bruker bloacking i/o s� skal det vel bare bli 0 hvis det uikke er mere data og lese? 
		//read skal altid blokke til det er noe data og lese, uanset hvor lang tid det tar
		//if ((n = read(sockfd, buf+total, bytesleft)) == -1) {
		if ((n = read(sockfd, buf+total, bytesleft)) <= 0) {
			return 0;
		}
		if (n == 0)
			return 0;

		#ifdef DEBUG
			printf("recved %i bytes. total red %i, left %i, total to get %i\n",n,total,bytesleft,len);
		#endif

		total += n;
            	bytesleft -= n;
	}

	return total;

}

int sendpacked(int socket,short command, short version, int dataSize, void *data,char subname[]) {

        struct packedHedderFormat packedHedder;
	void *buf;
	size_t len;
	int forret = 0;

	#ifdef DEBUG
		fprintf(stderr, "sendpacked(socket=%i, command=%d, version=%d, dataSize=%i, subname=%s)\n",socket,command,version,dataSize,subname);
	#endif

	//siden vi skal sende pakken over nettet er det like g�tt � nullstille all data. Da slipper vi at valgring klager ogs�.
	memset(&packedHedder,0,sizeof(packedHedder));

        //setter sammen hedder
        packedHedder.size       = sizeof(struct packedHedderFormat) + dataSize;
        packedHedder.version    = version;
        packedHedder.command    = command;
	strscpy(packedHedder.subname,subname,sizeof(packedHedder.subname));

	if (data != NULL) {
		len = sizeof(packedHedder) + dataSize;
		if ((buf = malloc(len)) == NULL) {
			printf("sendpacked: malloc");
			return 0;
		}
		memcpy(buf, &packedHedder, sizeof(packedHedder));
		memcpy(buf+sizeof(packedHedder), data, dataSize);
	} else {
		buf = &packedHedder;
		len = sizeof(packedHedder);
	}

	if (!sendall(socket, buf, len)) {
		printf("sendpacked: can't sendall()\n");
		goto end_error;
	}

	//setter at vi skal returnere 1, som er OK
	forret =  1;

	end_error:
		if (data != NULL) {
			free(buf);
		}

	#ifdef DEBUG
		fprintf(stderr, "~sendpacked(ret=%i)\n",forret);
	#endif

	//dene returneres altid, ogs� hvs vi ikke gjort en gotoend_error
	return forret;

}

void wait_loglock(char *name) {
	int fdlock;
	char *path;

	asprintf(&path, "%s/%s.log.lock", bfile("var/"), name);
	fdlock = open(path, O_RDONLY);
	free(path);
	if (fdlock == -1)
		return;
	
	flock(fdlock, LOCK_SH); // Block until exclusive lock is released
	close(fdlock);
}
