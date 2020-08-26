#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <strings.h>
#include <string.h>
#include <cstring>
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
#include <fstream>
#include <dirent.h>
#include <vector>
#include <sys/file.h>

void *threadFunc(void *arg);
void signalHandler( int signum );
int response(int i, int fd);
int receive_HELO(int fd);
int error_msg(int fd);
int receive_MAILFROM(int fd);
int receive_DATA(int fd, int i);
int receive_QUIT(int fd);

using namespace std;

pthread_mutex_t key;

bool traverseLocalUsers(char name[], string fold);
int receive_RCPTO(int fd, char name[], string folder);

int v_flag = 0;

struct thread_info
{int thread_num;};

#define max_c 100

struct thread_info t_inf[max_c];
pthread_t threads[max_c];
int client_fd[max_c];
int thread_no = 0;
time_t current;
string dir;

int main(int argc, char *argv[])
{
	/* Your code here */
	int parse_cmd;
	int smtp_port = 2500;

	if (argc < 2)
	{ fprintf (stderr, "Directory is not given\n");
	exit(1);}
	else
	{	dir = argv[argc - 1];}
	// store directory name in dir

	while ((parse_cmd = getopt (argc, argv, "avp:")) != -1) //parse command
		switch (parse_cmd)
		{
		case 'a':
			fprintf (stderr, "Chun Chang, chun3\n");
			exit(0);
			break;
		case 'p':
			smtp_port = atoi(optarg); //change port number
			if (smtp_port < 0)
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

	//build socket and bind socket
	int listen_fd = socket(PF_INET, SOCK_STREAM, 0);

	if (listen_fd < 0) {
		fprintf(stderr, "Cannot open socket (%s)\n", strerror(errno));
		exit(1);}

	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htons(INADDR_ANY);
	servaddr.sin_port = htons(smtp_port);
	bind(listen_fd, (struct sockaddr*)&servaddr, sizeof(servaddr));

	int listen_status = listen(listen_fd, 10);
	int comm_fd;

	if(listen_status != 0)
	{printf("listening error\n");}
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
	char greeting[] = "220 localhost chun3\r\n";

	write(comm_fd, greeting, sizeof(greeting) - 1);

	if(v_flag == 1)
	{fprintf(stderr, "[%d] New connection\n", comm_fd);}

	int q_flag = 1;
	char buf[1000];
	bzero(buf, sizeof(buf));
	string buffer;
	vector<string> users;
	vector<int> u_len;
	char sender[100];
	char r_time[64];
	int sender_len;
	bool user_flag = false; // check for existence of users
	while(q_flag != 0)
	{
		int f = -1;
		buffer.clear();
		int ind = 0;
		bzero(buf, sizeof(buf));

		while(f < 0 && ind < 999) // keep reading unitil get \r\n or over the limit of buffer
		{
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

		if(v_flag == 1)
		{ cout<<"[C]: " << buffer;}

		int server_res;

		if(UPPER.find("HELO") == 0 && buffer.length() > 6)
		{ server_res = receive_HELO(comm_fd);}
		else if(UPPER.find("MAIL FROM") == 0 && strchr(buf, '>') > 0 && strchr(buf, '<') > 0) // store sender's name
		{
			server_res = receive_MAILFROM(comm_fd);
			char* t_s = strchr(buf, '>');
			char* s_s = strchr(buf, '<');
			sender_len = t_s -s_s + 1;
			for (int i = 0; i < sender_len; i++)
			{ sender[i] = buf[s_s - buf + i];}
			time(&current);
			strcpy(r_time,ctime(&current));

			if(v_flag == 1)
			{ fprintf (stderr, "[%d]: MAIL FROM message from %s\n", comm_fd, sender);}
		}
		else if(UPPER.find("RCPT TO") == 0 && strchr(buf, '<') > 0 && strchr(buf, '@') > 0)
		{
			//check local users name
			char* t = strchr(buf, '@');
			char* s = strchr(buf, '<');
			int start = s - buf + 1;
			int len = t - s - 1;

			char user[len];
			for (int i = 0; i < len; i++)
			{ user[i] = buf[start + i];}
			server_res = receive_RCPTO(comm_fd, user, dir);
			if (server_res == 251)
			{ u_len.push_back(len); // if the receiver exists, store the name to the array
			users.push_back(user);}

			if(v_flag == 1)
			{  cout << "[" << comm_fd << "] S: recipient: " << user << "\n";}
		}
		else if(UPPER.find("NOOP") == 0)
		{{ fprintf (stderr, "[%d] S: NOOP received and do nothing\n", comm_fd);}}
		else if(UPPER.find("DATA") == 0 && users.size() > 0)//read data until end period is found.
		{
			server_res = receive_DATA(comm_fd, 0);
			string data;
			int period_find = -1;
			ind = 0;
			char read_buf[1000];
			bzero(read_buf, sizeof(read_buf));
			while(period_find < 0) // keep reading unitil get \r\n or over the limit of buffer
			{
				read(comm_fd, &read_buf[ind], 1);
				data += read_buf[ind];
				ind++;
				period_find = data.find("\r\n.\r\n"); // if CRLF.CRLF found, stop reading
			}
			server_res = receive_DATA(comm_fd, 1);
			for (int i = ind - 3; i < ind; i++)
			{ read_buf[i] = '\0';}
			// write to file
			char from_msg[256]; // concatenate time information
			strcat(from_msg, "From ");
			strcat(from_msg, sender);
			strcat(from_msg, r_time);

			for (int i = 0; i < users.size(); i++)
			{
				string loc2 = dir + "/" + users[i].substr(0, u_len[i]) + ".mbox";
				char* loc = new char[loc2.length() + 1];
				strcpy(loc, loc2.c_str());
				pthread_mutex_lock(&key); // mutex to protect data corruption
				FILE* file = fopen(loc, "a+");

				int fNo = fileno(file);
				int lo = flock(fNo, LOCK_EX); // file lock if pop3 is using
				if (file != NULL && errno != EWOULDBLOCK)
				{
				fputs(from_msg, file);
				fputs(read_buf, file);
				}
				fclose(file);
			    pthread_mutex_unlock(&key);

				delete [] loc;
			}
			users.clear();
			u_len.clear();
			bzero(r_time, sizeof(r_time));

			if(v_flag == 1)
			{ fprintf (stderr, "[%d] S: DATA transaction completed: \n", comm_fd);}
		}
		else if(UPPER.find("RSET") == 0 && users.size() > 0)
		{
			users.clear(); // clear all the receipients
			u_len.clear();
			bzero(sender, sizeof(sender));

			if(v_flag == 1)
			{ fprintf (stderr, "[%d]: Mail transaction aborted\n", comm_fd);}
		}
		else if(UPPER.find("QUIT") == 0)
		{
			server_res = receive_QUIT(comm_fd);
			if(v_flag == 1)
			{ fprintf(stderr, "[%d] S: %s Connection closed\n", comm_fd,  "+OK Goodbye!");}

			close(comm_fd);
			pthread_exit(NULL);
		}
		else // err
		{
			server_res = error_msg(comm_fd);
			if(v_flag == 1)
			{ fprintf (stderr, "[%d] S: -ERR Unknown command\n", comm_fd);}
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

//-----------------------------Different response to different messages
int receive_HELO(int fd)
{ return response(250, fd);}

int receive_MAILFROM(int fd)
{ return response(251, fd);}

int receive_RCPTO(int fd, char name[], string folder)
{	bool check = traverseLocalUsers(name, folder);
	if(check == true) // check user exists
	{ return response(251, fd);}
	return response(550, fd);}//return error messages if not exists


int receive_DATA(int fd, int i)
{ if (i == 0)
{return response(354, fd);}//return ok to start transmission
return response(355, fd);}//transmission done

int receive_QUIT(int fd)
{ return response(221, fd);}

int receive_RSET(int fd)
{ return response(253, fd);}

int error_msg(int fd)
{ return response(500, fd);}

void writeTO(string msg, int fd)
{
	char res_to[msg.length()];
	for (int i = 0; i < msg.length(); i++)
	{ res_to[i] = msg[i];}
	write(fd, &res_to[0], sizeof(res_to));
}

//----------------------------- response message with number indices to show successful/failed status
int response(int i, int fd)
{
	string res_msg;
	int result;
	switch (i)
	{
	case 221:
		res_msg = "221 +OK Goodbye!\r\n";
		writeTO(res_msg, fd);
		result = 221;
		break;

	case 250:
		res_msg = "250 localhost chun3\r\n";
		writeTO(res_msg, fd);
		result = 250;
		break;
	case 251: // mail from
		res_msg = "250 OK\r\n";
		writeTO(res_msg, fd);
		result = 251;
		break;
	case 252: //rcpt to
		res_msg = "250 OK\r\n";
		writeTO(res_msg, fd);
		result = 252;
		break;

	case 253: //rset to
		res_msg = "250 OK reset\r\n";
		writeTO(res_msg, fd);
		result = 252;
		break;

	case 550: //rcpt to error
		res_msg = "550 OK\r\n";
		writeTO(res_msg, fd);
		result = 550;
		break;

	case 354: // data transmission start
		res_msg = "354 OK\r\n";
		writeTO(res_msg, fd);
		result = 354;
		break;

	case 355: //data transmission done
		res_msg = "250 OK\r\n";
		writeTO(res_msg, fd);
		result = 355;
		break;

	case 500:
		res_msg = "500 -ERR Unknown command\r\n";
		writeTO(res_msg, fd);
		result = 500;
		break;
	}
	return result;
}
//----------------------------- explore the files in the folder
bool traverseLocalUsers(char name[], string fold)
{
	bool check = false;
	DIR* folder;
	char* loc = new char[fold.length() + 1];
	strcpy(loc, fold.c_str());
	folder = opendir(loc); // go to the folder
	struct dirent *file;
	char filename[256];
	int files = 0;
	if (folder == NULL)
	{	cout << "folder not exist\n";
		exit(5);
	return check;}

	char *p;
	while(file = readdir(folder)) // traverse all mailbox
	{
		files++;
		strcpy(filename, file->d_name);
		p = strchr(filename, '.');
		int len = p - filename;
		if (len != 0)
		{
			char u[len];
			for(int i = 0; i < len; i++)
			{ u[i] = filename[i];}
			if (strncmp(name, u, len) == 0) // if user exist, return true
			{return true;}
		}
	}
	closedir(folder);
	delete [] loc;
//	delete [] p;
	return check;
}

