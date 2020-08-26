#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <strings.h>
#include <string.h>
#include <errno.h>
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <csignal>
#include <fcntl.h>
#include <cstring>

void *threadFunc(void *arg);
void signalHandler( int signum );

using namespace std;

int v_flag = 0;

struct thread_info
{
	int thread_num;
};

#define max_c 100

struct thread_info t_inf[max_c];
pthread_t threads[max_c];
int client_fd[max_c];
int thread_no = 0;

int main(int argc, char *argv[])
{
	cout << "-2 ";

	/* Your code here */
	srand(time(NULL));
	int parse_cmd;
	int num_bits = 200;
	int port_defult = 10000;
	cout << "-1 ";

	while ((parse_cmd = getopt (argc, argv, "avp:")) != -1) //parse command
		switch (parse_cmd)
		{
		case 'a':
			fprintf (stderr, "Chun Chang, chun3\n");
			exit(0);
			break;
		case 'p':
			port_defult = atoi(optarg); //change port number
			if (port_defult < 0)
			{
				printf("port number needs to be positive");
				return 1;
			}
			break;
		case 'v': //  debug output
			v_flag = 1;
			break;

		default:
			fprintf (stderr, "Chun Chang, chun3\n");
			abort ();}

	cout << "0 ";

	//build socket and bind socket
	int listen_fd = socket(PF_INET, SOCK_STREAM, 0);

	if (listen_fd < 0) {
		fprintf(stderr, "Cannot open socket (%s)\n", strerror(errno));
		exit(1);
	}

	cout << "1 ";

	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htons(INADDR_ANY);
	servaddr.sin_port = htons(port_defult);
	bind(listen_fd, (struct sockaddr*)&servaddr, sizeof(servaddr));

	int listen_status = listen(listen_fd, 100);
	int comm_fd;

	cout << "ready to pairing with port: " << port_defult;


	if(listen_status != 0)
	{
		printf("listening error\n");
	}
	else
	{
		signal(SIGINT, signalHandler);
		struct sockaddr_in clientaddr;

		while(true)//listening for requests
		{
			socklen_t clientaddrlen = sizeof(clientaddr);
			comm_fd = accept(listen_fd, (struct sockaddr*)&clientaddr, &clientaddrlen);
			t_inf[thread_no].thread_num = comm_fd;
			client_fd[thread_no] = comm_fd;
			//create thread for new communication
			pthread_create(&threads[thread_no], NULL, threadFunc, &t_inf[thread_no]);
			thread_no++;
		}
	}
	return 0;
}

//-----------------------------thread function
void *threadFunc(void *arg)
{
	thread_info *t_info = (thread_info*) arg;
	int comm_fd = t_info->thread_num;
	char message[] = "+OK Server ready (Author: Chun Chang)\r\n";
	write(comm_fd, message, sizeof(message) - 1);

	if(v_flag == 1)
	{
		fprintf(stderr, "[%d] New connection\n", comm_fd);
	}
	int q_flag = 1;
	char buf[1000];
	string buffer;
	bzero(buf, sizeof(buf));
	while(q_flag != 0)
	{
		int f = -1;
		buffer.clear();
		int ind = 0;
		while(f < 0 && ind < 999) // keep reading unitil get \r\n or over the limit of buffer
		{
			bzero(buf, sizeof(buf));
			read(comm_fd, &buf[ind], 1);
			buffer += buf[ind];
			ind++;
			f = buffer.find("\r\n");
		}
		if (ind > 999) // if command >= 1000, skip the loop
		{continue;}

		string UPPER;
		UPPER.clear();
		for(int i = 0; i < ind; i++)
		{	UPPER += toupper(buffer.at(i));}

		int echo_flag = UPPER.find("ECHO");
		int quit_flag = UPPER.find("QUIT");

		if(v_flag == 1)
		{ cout<<"[C]: " << buffer;}
		if(echo_flag >= 0)
		{
			string message_back;
			message_back.clear();
			message_back = "+OK " + buffer.substr(5,ind); + "\r\n";
			write(comm_fd,&message_back[0], ind - 1);
			if(v_flag == 1)
			{ cout<< "[S]: " << message_back;}
		}

		else if(echo_flag < 0 && quit_flag < 0)
		{
			char err_msg[] = "-ERR Unknown command\r\n";
			if(v_flag == 1)
			{ fprintf (stderr, "[S]: -ERR Unknown command from %d\n", comm_fd);}
			write(comm_fd,&err_msg[0], sizeof(err_msg) - 1);
		}
		else if(quit_flag >= 0)
		{
			char bye[] = "+OK Goodbye!\r\n";
			write(comm_fd,&bye[0], sizeof(bye));
			close(comm_fd);
			if(v_flag == 1)
			{ 	cout << "[C]: " << bye;
				fprintf(stderr, "[%d] Connection closed", comm_fd);}
			pthread_exit(NULL);
		}
	}

}

//-----------------------------signal handler
void signalHandler( int signum ) {
	//write message into thead and terminate thread
	string err_shut = "-ERR server shutting down\r\n";
	void *sta;
	for (int i = 0; i < thread_no; i++)
	{
		write(client_fd[i],&err_shut[0], sizeof(err_shut));
		pthread_cancel(threads[i]);
	}
	exit(signum);
}

