//reference https://backreference.org/2010/03/26/tuntap-interface-tutorial/ simpletun.c 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <net/if.h>
#include <linux/if_tun.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h> 
#include <sys/select.h>
#include <sys/time.h>
#include <errno.h>
#include <stdarg.h>
//include for serial
#include <termios.h> /*termio.h for serial IO api*/ 

#include "slip.h" /* SLIP encode and decode*/
#define MAX_STR_LEN 256
/* buffer for reading from tun/tap interface, must be >= 1500 */
#define BUFSIZE 1400   
#define CLIENT 0
#define SERVER 1
#define PORT 55555
/*serial setup and read write*/
char ATcommands[11][50] = {
			   "AT+IFC=2,2\r",
			   "AT+CPIN?\r",
			   "AT+CIPMODE=1\r",
			   "AT+CIPCCFG=5,1,1400,0,1,1460,50\r",
			   "AT+CGDCONT=1,\"ip\",\"\"\r", 
			   "AT+CSTT=\"internet.iot\"\r",  
			   "AT+CIICR\r", 
			   "AT+CIFSR\r",
			   "AT+CIPSHUT\r", 
			   "AT+CIPSTART=\"udp\",\"", 
			   "+++192.168.0.1"};
int command = 0;
//"AT+CIPSTART=\"udp\",\"140.113.216.91\",8888\r", +CIPCCFG: 5,1,1024,1,0,1460,50
int SetInterfaceAttribs(int fd, int speed, int parity)
{
        struct termios tty;
   
        memset (&tty, 0, sizeof tty);
        if (tcgetattr (fd, &tty) != 0) /* save current serial port settings */
        {
			printf("__LINE__ = %d, error %s\n", __LINE__, strerror(errno));
			return -1;
        }

        cfsetospeed (&tty, speed);
        cfsetispeed (&tty, speed);

        // disable IGNBRK for mismatched speed tests; otherwise receive break
        // as \000 chars
        tty.c_iflag |= IGNBRK;         // disable break processing
        //tty.c_lflag |= (ICANON);     // no signaling chars, no echo,
        tty.c_lflag = 0;               //  canonical processing
        tty.c_oflag &= ~OPOST;		   //No Output Processing
        tty.c_cc[VMIN]  = 1;           // read doesn't block
        //tty.c_cc[VMIN]  =  1 ;
        tty.c_cc[VTIME] =  0;     	   // in unit of 100 milli-sec for set timeout value

        tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl
		
		tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
        tty.c_cflag |= (CLOCAL | CREAD);// ignore modem controls,  enable reading
        tty.c_cflag &= ~(PARENB | PARODD);      // shut off parity
        tty.c_cflag |= parity;
        tty.c_cflag &= ~CSTOPB;      //disable 2 stop bit
        tty.c_cflag |= CRTSCTS;		 //enable hw flow control

        if (tcsetattr (fd, TCSANOW, &tty) != 0)
        {
            printf("__LINE__ = %d, error %s\n", __LINE__, strerror(errno));
            return -1;
        }
        return 0;
}/*SetInterfaceAttribs*/


void sendThread(int serialfd)
{
 char sendBuff[MAX_STR_LEN];
 memset(&sendBuff[0], 0, MAX_STR_LEN);
 
 int fd;
 
 fd = serialfd;
 
 while(command < 11)
 {
  snprintf(&sendBuff[0], MAX_STR_LEN, ATcommands[command]);
  write(fd, &sendBuff[0], strlen(&sendBuff[0]) ); 
 
  // sleep enough to transmit the length plus receive 25:  
  // approx 100 uS per char transmit
  if(command == 10)
	  break;
  else command ++;
  usleep((strlen(&sendBuff[0]) + 25) * 100);     
  usleep(5000*1000); 
  printf("cmd%d\n",command);
 }/*while*/
 printf("ready to send at command data\n"); 
 usleep(500*10000);
}/*sendThread */


/*end of serial setup and write*/
int debug;
char *progname;

/**************************************************************************
 * tun_alloc: allocates or reconnects to a tun/tap device. The caller     *
 *            must reserve enough space in *dev.                          *
 **************************************************************************/
int tun_alloc(char *dev, int flags) {

  struct ifreq ifr;
  int fd, err;
  char *clonedev = "/dev/net/tun";

  if( (fd = open(clonedev , O_RDWR)) < 0 ) {
    perror("Opening /dev/net/tun");
    return fd;
  }

  memset(&ifr, 0, sizeof(ifr));

  ifr.ifr_flags = flags;

  if (*dev) {
    strncpy(ifr.ifr_name, dev, IFNAMSIZ);
  }

  if( (err = ioctl(fd, TUNSETIFF, (void *)&ifr)) < 0 ) {
    perror("ioctl(TUNSETIFF)");
    close(fd);
    return err;
  }

  strcpy(dev, ifr.ifr_name);

  return fd;
}

/**************************************************************************
 * cread: read routine that checks for errors and exits if an error is    *
 *        returned.                                                       *
 **************************************************************************/
int cread(int fd, char *buf, int n){
  
  int nread;

  if((nread=read(fd, buf, n)) < 0){
    perror("Reading data");
    exit(1);
  }
  return nread;
}

/**************************************************************************
 * cwrite: write routine that checks for errors and exits if an error is  *
 *         returned.                                                      *
 **************************************************************************/
int cwrite(int fd, char *buf, int n){
  
  int nwrite;

  if((nwrite=write(fd, buf, n)) < 0){
    perror("Writing data");
    //exit(1);
  }
  return nwrite;
}

/**************************************************************************
 * do_debug: prints debugging stuff (doh!)                                *
 **************************************************************************/
void do_debug(char *msg, ...){
  
  va_list argp;
  
  if(debug) {
	va_start(argp, msg);
	vfprintf(stderr, msg, argp);
	va_end(argp);
  }
}

/**************************************************************************
 * my_err: prints custom error messages on stderr.                        *
 **************************************************************************/
void my_err(char *msg, ...) {

  va_list argp;
  
  va_start(argp, msg);
  vfprintf(stderr, msg, argp);
  va_end(argp);
}

/**************************************************************************
 * usage: prints usage and exits.                                         *
 **************************************************************************/
void usage(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "%s -i <ifacename> [-s<proxyIP>|-c <proxyIP>] [-p <port>] [-d]\n", progname);
  fprintf(stderr, "%s -h\n", progname);
  fprintf(stderr, "\n");
  fprintf(stderr, "-i <ifacename>: Name of interface to use (mandatory)\n");
  fprintf(stderr, "-s<proxyIP>|-c<proxyIP>: run in Host Computers (-s), or NBIOT deivces (-c) (mandatory)\n");
  fprintf(stderr, "-p <port>: specify proxy's open port (mandatory)\n");
  fprintf(stderr, "-d: outputs debug information while running\n");
  fprintf(stderr, "-h: prints this help text\n");
  exit(1);
}

int main(int argc, char *argv[]) {
  
  //random
  srand(1234);
  int tap_fd, option;
  int flags = IFF_TUN;
  char if_name[IFNAMSIZ] = "";
  int maxfd;
  int nread, nwrite;
  unsigned char buffer[BUFSIZE], encode_buffer[BUFSIZE*2], prev_encode_buffer[BUFSIZE*4];
  struct sockaddr_in server, client;
  unsigned char remote_ip[16] = "";            /* dotted quad IP string */
  unsigned short int port = PORT;
  int sock_fd, net_fd, optval = 1;
  socklen_t remotelen;
  unsigned long int tap2net = 0, net2tap = 0, nbiot_rx_err_num = 0;
  int cliserv = -1;    /* must be specified on cmd line */
  int rv_len = 0;
  progname = argv[0];
  
  unsigned long encode_len = 0, decode_len = 0;
  int prev_len = 0;
  int index, next_index;
  int foundEND;
  /* Check command line options */
  while((option = getopt(argc, argv, "i:s:c:p:hd")) > 0) {
    switch(option) {
      case 'd':
        debug = 1;
        break;
      case 'h':
        usage();
        break;
      case 'i':
        strncpy(if_name,optarg, IFNAMSIZ-1);
        break;
      case 's':
        cliserv = SERVER;
		strncpy(remote_ip,optarg,15);
        break;
      case 'c':
        cliserv = CLIENT;
        strncpy(remote_ip,optarg,15);
        break;
      case 'p':
        port = atoi(optarg);
        break;
      default:
        my_err("Unknown option %c\n", option);
        usage();
    }
  }

  argv += optind;
  argc -= optind;

  if(argc > 0) {
    my_err("Too many options!\n");
    usage();
  }

  if(*if_name == '\0') {
    my_err("Must specify interface name!\n");
    usage();
  } else if(cliserv < 0) {
    my_err("Must specify client or server mode!\n");
    usage();
  } else if((cliserv == CLIENT)&&(*remote_ip == '\0')) {
    my_err("Must specify client address!\n");
    usage();
  }
  else if((cliserv == SERVER)&&(*remote_ip == '\0')) {
    my_err("Must specify server address!\n");
    usage();
  }

  /* initialize tun/tap interface */
  if ( (tap_fd = tun_alloc(if_name, flags | IFF_NO_PI)) < 0 ) {
    my_err("Error connecting to tun/tap interface %s!\n", if_name);
    exit(1);
  }

  do_debug("Successfully connected to interface %s\n", if_name);
  if(cliserv == SERVER){
	if ( (sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("socket()");
		exit(1);
	}
  }
  if(cliserv == CLIENT) {
    /* Client, try to connect to server */

    net_fd = open("/dev/ttyUSB2", O_RDWR | O_NOCTTY | O_NONBLOCK);
	SetInterfaceAttribs(net_fd, B115200, 0); /* set speed to 115200 bps, 8n1 (no parity), timeout 2 secs*/
	if(0 > net_fd) 
	{
		perror("/dev/ttyUSB2"); 
		exit(-1); 
	}
	char tmp[6];
	sprintf(tmp, "%d", port);
	strcat(ATcommands[9], remote_ip);
	strcat(ATcommands[9], "\",");
	strcat(ATcommands[9], tmp);
	ATcommands[9][strlen(ATcommands[9])] = '\r';
	sendThread(net_fd);
	
    
  } 
  else {
    /* Server, wait for connections */
    //SERVER
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(remote_ip);
    server.sin_port = htons(port);

	
		if((nwrite=sendto(sock_fd, "+++192.168.0.2", 20, 0, (const struct sockaddr *) &server, sizeof(server))) < 0){
			perror("sendto data");
			exit(1);
		}
		if((nwrite=sendto(sock_fd, "+++192.168.0.2", 20, 0, (const struct sockaddr *) &server, sizeof(server))) < 0){
			perror("sendto data");
			exit(1);
		}
	net_fd = sock_fd;
	
  }
  
  /* use select() to handle two descriptors at once */
  maxfd = (tap_fd > net_fd)?tap_fd:net_fd;
  memset(prev_encode_buffer, 0, BUFSIZE*4);
  while(1) {
    int ret;
    fd_set rd_set;

    FD_ZERO(&rd_set); //clear the set to zero
    FD_SET(tap_fd, &rd_set); FD_SET(net_fd, &rd_set);  // add tap,net_fd to rd_set

    ret = select(maxfd + 1, &rd_set, NULL, NULL, NULL);

    if (ret < 0 && errno == EINTR){
      continue;
    }

    if (ret < 0) {
      perror("select()");
      exit(1);
    }

    if(FD_ISSET(net_fd, &rd_set)) {
	  /* Rpi read packet from nbiot*/
	  memset(encode_buffer, 0, BUFSIZE*2);
	  printf("read from net interface\n");
	  if(cliserv == CLIENT){
		//for mtu = 1200
		nread  = read(net_fd, encode_buffer, BUFSIZE*2);
		printf("nread: %d, \t\t\tprev_len: %d\n",nread, prev_len);
		if(nread == -1)
			printf("error when reading\n");

		for(int i = 0; i < nread; i++)	printf("%02x",encode_buffer[i]); printf("\n");fflush(stdout);
		//random
		if(rand() < (((double)RAND_MAX+1.0) * 0.95) || prev_len != 0)
			memcpy(&prev_encode_buffer[prev_len], encode_buffer, nread);
		else {
			nread = 0;
		}
		//decode [prev_encode_buffer + encode_buffer]
		foundEND = 0;
		for(index = 0; index < nread+prev_len; index++){
			if(prev_encode_buffer[index] == SLIP_END){
				foundEND = 1;
				if(prev_encode_buffer[index+1] == SLIP_END) index ++;
				for(next_index = index+1; next_index < nread+prev_len; next_index++){
					if(prev_encode_buffer[next_index] == SLIP_END){
						net2tap++;
						do_debug("NET2TAP %lu: Read %d bytes from the network\n", net2tap, next_index-index+1);
						for(int i = index; i<= next_index; i++)	printf("%02x",prev_encode_buffer[i]);
						printf("\n");
						memset(buffer, 0, BUFSIZE);
						slip_decode(&prev_encode_buffer[index], next_index-index+1, buffer, BUFSIZE, &decode_len);
						nwrite = cwrite(tap_fd, buffer, decode_len);
						do_debug("NET2TAP %lu: Written %d bytes to the tap interface\n", net2tap, nwrite);
						break;
					}
				}
				if(next_index == nread+prev_len){
					//cannot find end of frame
					//store the rest to the start of prev_encode_buffer
					memmove(prev_encode_buffer, prev_encode_buffer+index, nread+prev_len-index);
					prev_len = nread+prev_len-index;
					break;
				}
				else if(next_index == nread+prev_len-1){
					memset(prev_encode_buffer, 0, BUFSIZE*4);
					prev_len = 0;
					break;
				}
				else index = next_index;
			}
			
		}
		if(foundEND == 0){
			//no any END in prev_encode_buffer
			memset(prev_encode_buffer, 0, BUFSIZE*4);
			prev_len = 0;
		}
		
	  }
	  else{  
		  //cliserv == SERVER, host read packet from socket
		  if((nread=recvfrom(net_fd, buffer, BUFSIZE, 0, ( struct sockaddr *)&server, &rv_len)) < 0){
			perror("error : Recvfrom data");
			exit(1);
		  }
		  net2tap++;
		  do_debug("NET2TAP %lu: Read %d bytes to the net interface\n", net2tap, nread);
		  nwrite = cwrite(tap_fd, buffer, nread);
		  do_debug("NET2TAP %lu: Written %d bytes to the tap interface\n", net2tap, nwrite);
		  
	  } 
	  	  
	}
	
	if(FD_ISSET(tap_fd, &rd_set)) {
      /* data from tun/tap: just read it and write it to the network */
	  printf("read from tap interface\n");
      memset(buffer, 0, BUFSIZE);
	  memset(encode_buffer, 0, BUFSIZE*2);
	  nread = cread(tap_fd, buffer, BUFSIZE);
	  printf("tap : %d\n", nread);
	  tap2net++;
      do_debug("TAP2NET %lu: Read %d bytes from the tap interface\n", tap2net, nread);
      /* write packet to network*/
      if(cliserv == CLIENT){
		slip_encode(buffer, nread, encode_buffer, BUFSIZE*2, &encode_len);
		//random
		if(rand() < (((double)RAND_MAX+1.0) * 0.95) ){
			nwrite = write(net_fd, encode_buffer, encode_len);
			usleep(100*1000);
		}
	  }
      else{
		slip_encode(buffer, nread, encode_buffer, BUFSIZE*2, &encode_len);	
		if((nwrite=sendto(net_fd, encode_buffer, encode_len, 0, (const struct sockaddr *) &server, sizeof(server))) < 0){
			perror("sendto data");
			exit(1);
		}
		
	  }
	  do_debug("TAP2NET %lu: Written %d bytes to the network\n", tap2net, nwrite);
    }
  
  }
	
	
  return(0);
}
