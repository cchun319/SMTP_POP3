#include <stdlib.h>
#include <stdio.h>
#include <openssl/md5.h>
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
#include <map>
#include <condition_variable>
#include <mutex>

using namespace std;

struct mbox_info
{
	vector<int> status;
	vector<int> index;
	vector<int> sizeOfMessage;
	vector<string> uid;
	vector<string> msgs;
	int boxSize;
	int num_msgs;
	int aval_msgs;
};

void *threadFunc(void *arg);
void signalHandler( int signum );
int error_msg(int fd);
int receive_PASS(int fd, char password[]);
int receive_USER(int fd, char name[]);
bool traverseLocalUsers(char name[], string nameOfFolder);
int receive_QUIT(int fd);
int statCalculation(char user[], int len, mbox_info& info, string nameOfFolder);
int deleteMsg(char user[], int len, mbox_info& info, string nameOfFolder);
void writeTO(string msg, int fd);
int response(int i, int fd);
void makeUID(mbox_info& info);
void makeAllUnDelete(mbox_info& info);
void retrieveMsg(int ind, mbox_info& info, int fd);
void getLockMap(string nameOfFolder);

FILE* openFile(char user[], int len, string nameOfFolder);
int doLock(FILE* out);
string remakeMes(mbox_info& info);
void writeIntoFile(FILE* wr, string mes, int err);

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

std::mutex mtx;
std::condition_variable cv;

map<string, pthread_mutex_t> lock1; //construct map to store information (name, thread_mutex)
map<string, bool> lock_status;

void computeDigest(char *data, int dataLengthBytes, unsigned char *digestBuffer)
{
	/* The digest will be written to digestBuffer, which must be at least MD5_DIGEST_LENGTH bytes long */
	MD5_CTX c;
	MD5_Init(&c);
	MD5_Update(&c, data, dataLengthBytes);
	MD5_Final(digestBuffer, &c);
}

int main(int argc, char *argv[])
{
	/* Your code here */
	srand (time(NULL));
	int parse_cmd;
	int pop3_port = 11000;

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
			pop3_port = atoi(optarg); //change port number
			if (pop3_port < 0)
			{	printf("port number needs to be positive");
			return 1;}
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
	servaddr.sin_port = htons(pop3_port);
	bind(listen_fd, (struct sockaddr*)&servaddr, sizeof(servaddr));

	int listen_status = listen(listen_fd, 10);
	int comm_fd;

	getLockMap(dir);

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
	char greeting[] = "+OK POP3 server ready\r\n";

	write(comm_fd, greeting, sizeof(greeting) - 1);

	if(v_flag == 1)
	{fprintf(stderr, "[%d] New connection\n", comm_fd);}

	int q_flag = 1;
	char buf[1000];
	bzero(buf, sizeof(buf));
	string buffer;
	char user[100];
	int user_len;
	mbox_info mbox;
	bool user_flag = false; // check if user exist in folder
	bool pass_flag = false; // check if user's password is correct
	bool opt_flag; // flag for transaction
	FILE* u_box;
	int err_f;

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
		for(int i = 0; i < ind; i++) // do case-insensitive
		{	UPPER += toupper(buffer.at(i));}

		if(v_flag == 1)
		{ fprintf (stderr, "[%d] C: ", comm_fd );
		cout << buffer;}

		int server_res;

		if(UPPER.find("USER ") == 0 && buffer.length() > 5) // browse the directory to check user exist or not, if not -> err
		{
			//check local users name
			char* t = strchr(buf, '\r');
			char* s = strchr(buf, ' ');
			int start = s - buf + 1;
			user_len = t - s - 1;
			for (int i = 0; i < user_len; i++)
			{ user[i] = buf[start + i];}
			server_res = receive_USER(comm_fd, user);
			if (server_res == 250)
			{ user_flag = true; }

			if(v_flag == 1)
			{  fprintf (stderr, "[%d] S: %s existed: %s\n", comm_fd, user, user_flag ? "true" : "false" );}
		}
		else if(UPPER.find("PASS ") == 0 && user_flag == true && buffer.length() > 5)
		{
			char* t_s = strchr(buf, '\r');
			char* s_s = strchr(buf, ' ');
			int pw_len = t_s -s_s - 1;
			char pw[pw_len];
			for (int i = 0; i < pw_len; i++)
			{ pw[i] = buf[s_s - buf + i + 1];}
			server_res = receive_PASS(comm_fd, pw);

			if (server_res == 250 && user_flag)
			{
				pass_flag = true;
				if (lock_status[user] == true) // locked, send waiting message
				{	{ if(v_flag == 1){fprintf(stderr, "-ERR account is be logined, waiting for the user logout");}}}
				while(true)
				{
					int flag = pthread_mutex_trylock(&lock1[user]);
					if(flag == 0)
					{ 	lock_status[user] = true;
					int cal_res =  statCalculation(user, user_len, mbox, dir); // get index of user's mailbox: message, mailboxsize, status, sizeofmessage
					break;}
				}

			// open file
			// start file block
			u_box = openFile(user, user_len, dir);
			err_f = doLock(u_box);
			}
			opt_flag = (pass_flag && user_flag);

			if(v_flag == 1)
			{  fprintf (stderr, "[%d] S: %s pass: %s\n", comm_fd, user, pass_flag ? "true" : "false" );}
		}
		else if(UPPER.find("STAT") == 0 && opt_flag)
		{
			//show number of messages and size of mailbox
			string statistics = "+OK " + to_string(mbox.num_msgs) + " " + to_string(mbox.boxSize) + "\r\n";
			writeTO(statistics, comm_fd);
			if(v_flag == 1)
			{  fprintf (stderr, "[%d] S: # of messages: %d size of box: %d\n", comm_fd, mbox.num_msgs, mbox.boxSize);}
		}
		else if(UPPER.find("UIDL") == 0 && opt_flag)//append unique ID in the back of each messages
		{
			if(mbox.uid.size() == 0)//if unique id is not made, call make UID to make it
			{ makeUID(mbox);}
			string mes;
			if(buffer.length() > 6)
			{ 	int des_u = stoi(buffer.substr(5,buffer.length()));

			if (des_u <= mbox.num_msgs && des_u > 0) // if assgined, return the assgined UID of that index
			{
				mes = to_string(mbox.index[des_u - 1]) + " " + mbox.uid[des_u - 1] + "\r\n";
				writeTO(mes, comm_fd);
				if(v_flag == 1)
				{ fprintf (stderr, "[%d] S: %s", comm_fd, mes.c_str());}
				mes.clear();
			}
			else
			{	server_res = response(500, comm_fd);
			if(v_flag == 1)
			{ fprintf (stderr, "[%d]: %s", comm_fd, "out of bound");}}}
			else{
				server_res = response(250, comm_fd);
				for(int i = 0; i < mbox.num_msgs; i ++) // if not assgined, return the all UID of that index
				{	mes = to_string(mbox.index[i]) + " " + mbox.uid[i] + "\r\n";
				writeTO(mes, comm_fd);
				if(v_flag == 1)
				{ fprintf (stderr, "[%d] S: %s", comm_fd, mes.c_str());}
				mes.clear();}
				mes = ".\r\n";
				writeTO(mes, comm_fd);}
		}
		else if(UPPER.find("DELE ") == 0 && opt_flag && buffer.length() > 5)
		{
			if(buffer.length() > 6)
			{ 	int del = stoi(buffer.substr(5,buffer.length()));
			if (del <= mbox.num_msgs && del > 0) // check the assigned index in the range
			{
				if (mbox.status[del - 1] == 0)
				{mbox.status[del - 1] = 1; // change deleted flag to 1
				mbox.aval_msgs --; // number of undeleted message
				mbox.boxSize -= mbox.sizeOfMessage[del - 1]; // update size of mailbox
				server_res = response(250, comm_fd);
				}
				else
				{
					server_res = response(500, comm_fd);
					if(v_flag == 1)
					{ fprintf (stderr, "[%d]: %s", comm_fd, "already delected\n");}}
			}
			else
			{
				if(v_flag == 1)
				{ fprintf (stderr, "[%d]: %s", comm_fd, "out of bound\n");}}}
			else{ if(v_flag == 1)
			{ fprintf (stderr, "[%d] S: %s", comm_fd, "no index\n");}}

		}
		else if(UPPER.find("LIST") == 0 && opt_flag)
		{
			if(buffer.length() > 6)
			{ int des = stoi(buffer.substr(5,buffer.length()));

			string mesL;
			if (des <= mbox.num_msgs && des > 0 && mbox.status[des - 1] == 0)//return index and size of the assigned message
			{
				mesL = "+OK " + to_string(mbox.index[des - 1]) + " " + to_string(mbox.sizeOfMessage[des - 1]) + "\r\n";
				writeTO(mesL, comm_fd);
				if(v_flag == 1)
				{ fprintf (stderr, "[%d] S: %s", comm_fd, mesL.c_str());}
				mesL.clear();
			}
			else
			{ 	server_res = response(500, comm_fd);
			if(v_flag == 1)
			{ fprintf (stderr, "[%d]: %s", comm_fd, "out of bound\n");}}}
			else
			{	string mesL2 = "+OK " + to_string(mbox.num_msgs) + "\r\n";
			writeTO(mesL2, comm_fd);
			mesL2.clear();
			for(int i = 0; i < mbox.num_msgs; i ++)//return index and size of every message
			{
				if(mbox.status[i] == 0)
				{
					mesL2 = to_string(mbox.index[i]) + " " + to_string(mbox.sizeOfMessage[i]) + "\r\n";
					writeTO(mesL2, comm_fd);
					if(v_flag == 1)
					{ fprintf (stderr, "[%d] S: %s", comm_fd, mesL2.c_str());}
					mesL2.clear();
				}
			}
			mesL2 = ".\r\n";
			writeTO(mesL2, comm_fd);
			}
		}
		else if(UPPER.find("RETR ") == 0 && opt_flag && buffer.length() > 5)
		{
			if(buffer.length() > 6)
			{ 	int ret = stoi(buffer.substr(5,buffer.length()));
			if (ret <= mbox.num_msgs && ret > 0 && mbox.status[ret - 1] == 0) // if the message is not deleted and exist in the mailbox
			{
				writeTO("+OK " + to_string(mbox.sizeOfMessage[ret - 1]) + "\r\n", comm_fd);
				if(v_flag == 1)
				{ fprintf (stderr, "[%d] S: +OK size: %d\n", comm_fd, mbox.sizeOfMessage[ret - 1]);}
				retrieveMsg(ret, mbox, comm_fd);
			}
			else
			{ 	server_res = response(500, comm_fd);
			if(v_flag == 1)
			{ fprintf (stderr, "[%d] S: %s", comm_fd, " out of bounds or deleted\n");}}}
			else
			{
				server_res = response(500, comm_fd);
				if(v_flag == 1)
				{ fprintf (stderr, "[%d] S: %s", comm_fd, "not assign index\n");}}
		}
		else if(UPPER.find("RSET") == 0 && opt_flag)
		{
			makeAllUnDelete(mbox);//turn flags of every message to undeleted
			server_res = response(250, comm_fd);
			if(v_flag == 1)
			{ fprintf (stderr, "[%d] S: %s", comm_fd, "RESET\n");}
		}
		else if(UPPER.find("QUIT") == 0 && opt_flag)
		{
			string remes = remakeMes(mbox);
			writeIntoFile(u_box, remes,  err_f); // write remain text into file and release lock
			server_res = receive_QUIT(comm_fd);
			if(v_flag == 1)
			{ fprintf(stderr, "[%d] S: %s Connection closed\r\n", comm_fd, "+OK Goodbye!");}
			pthread_mutex_unlock(&lock1[user]);
			lock_status[user] = false;
			// turn off lock for this user
			close(comm_fd);
			pthread_exit(NULL);
		}
		else if(UPPER.find("NOOP") == 0 && opt_flag)
		{{ fprintf (stderr, "[%d] S: NOOP received and do nothing\n", comm_fd);}}
		else
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
	{	write(client_fd[i],&err_shut[0], sizeof(err_shut));
	pthread_cancel(threads[i]);}
	exit(signum);
}


int receive_PASS(int fd, char password[])
{ char PW[] = "cis505";
if (strcmp(PW, password) == 0) // compare password and return server response
{ return response(250, fd); }
return response(500, fd);
}

int receive_USER(int fd, char name[])
{
	bool check = traverseLocalUsers(name, dir);
	if(check == true) // check user exists
	{ return response(250, fd);}
	return response(550, fd);
}

int receive_QUIT(int fd)
{ return response(221, fd);}

int error_msg(int fd)
{ return response(500, fd);}

void writeTO(string msg, int fd)
{
	char res_to[msg.length()];
	for (int i = 0; i < msg.length(); i++)
	{ res_to[i] = msg[i];}
	write(fd, &res_to[0], sizeof(res_to));
}

int response(int i, int fd)
{
	string res_msg;
	int result;
	switch (i)
	{
	case 221:
		res_msg = "+OK Goodbye!\r\n";
		writeTO(res_msg, fd);
		result = 221;
		break;

	case 250:
		res_msg = "+OK \r\n";
		writeTO(res_msg, fd);
		result = 250;
		break;

	case 500:
		res_msg = "-ERR Unknown command\r\n";
		writeTO(res_msg, fd);
		result = 500;
		break;
	}
	return result;
}

bool traverseLocalUsers(char name[], string nameOfFolder)
{
	bool check = false;
	DIR* folder;
	char * nf = new char[nameOfFolder.length() + 1];
	strcpy(nf, nameOfFolder.c_str());
	folder = opendir(nf);
	struct dirent *file;
	char filename[256];
	int files = 0;
	if (folder == NULL)
	{	cout << "folder not exist\n";
		exit(5);
	return check;}

	char *p;
	while(file = readdir(folder)) // go to all file in the folder
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
			if (strncmp(name, u, len) == 0) // mailbox exists if names match
			{ check = true;
			break;}
		}
	}
	//	delete [] p;
	delete [] nf;
	closedir(folder);
	return check;
}

int statCalculation(char user[], int len, mbox_info& info, string folder)
{
	int index = 0, BoxSize = 0, MsgSize = 0; // number of message

	string user_s = user;
	string location = "./" + folder + "/" + user_s.substr(0, len) +".mbox";

	char* loc_p = new char[location.length() + 1];
	strcpy(loc_p, location.c_str());
	int size = 1000, ind = 0;
	char *buf = (char*)malloc(size);
	int ch;
	string msg;
	FILE* mbox = fopen(loc_p, "r");
	if(mbox)
	{
		while (ch > 0) // end of file
		{
			ind = 0;
			do{
				ch = fgetc(mbox);
				if (ch > 0)
				{ buf[ind++] = (char)ch;}
				if (ind >= size - 1)
				{ size *= 2;
				buf = (char*) realloc(buf, size);}
			}while(ch > 0 && ch != '\n'); // end of a line
			buf[ind] = '\0';
			string line = buf;

			if((line.find("From ") == 0 && info.index.size() > 0 )|| ch < 0) // count size of message
			{ info.sizeOfMessage.push_back(MsgSize);
			BoxSize += MsgSize;
			MsgSize = 0;
			info.msgs.push_back(msg);
			msg.clear();}
			msg += line;

			if(line.find("From ") == 0) // count number of message and index
			{ind = 0;
			index++;
			info.index.push_back(index);
			info.status.push_back(0);}
			MsgSize += ind;

		}
		info.boxSize = BoxSize;
		info.num_msgs = index;
		info.aval_msgs = index;
	}
	else
	{ 	free(buf);
	delete [] loc_p;
	return -1;}

	fclose(mbox);
	free(buf);
	delete [] loc_p;
	return 0;
}

FILE* openFile(char user[], int len, string nameOfFolder)
{
	string user_s = user;
	string location = nameOfFolder + "/" + user_s.substr(0, len) +".mbox";
	char* loc_p = new char[location.length() + 1];
	strcpy(loc_p, location.c_str());
	FILE* out = fopen(loc_p, "w"); // write to a file
	delete [] loc_p;
	return out;
}

int doLock(FILE* out)// lock the file in case simultaneous writing by pop3 and smtp
{
	int fNo = fileno(out);
	int lo = flock(fNo, LOCK_EX | LOCK_NB);
	return errno;
}

string remakeMes(mbox_info& info)
{
	string modified;
	for (int i = 0; i < info.num_msgs; i++)
	{
		if(info.status[i] == 1) // concatenate all unddeleted messages together
		{continue;}
		modified += info.msgs[i];
	}
	return modified;
}

void writeIntoFile(FILE* wr, string mes, int err) // close file also release the lock
{
	char toFile[mes.length()];
	strcpy(toFile, mes.c_str());
	if(err != EWOULDBLOCK)
	{	fputs(toFile, wr);}
	fclose(wr);
}

void makeUID(mbox_info& info)
{
	for(int i = 0; i < info.num_msgs; i++)
	{
		int Sz = 70;
		char has[Sz];
		vector <char>data(info.msgs[i].begin(), info.msgs[i].end()); // put message as character array into vector
		data.push_back('\0');
		unsigned char digest[70];
		computeDigest(&data[0], Sz, digest);
		for(int i = 0; i < 16; i++)
		{    sprintf(&has[i*2], "%02x", (unsigned int)digest[i]);}
		info.uid.push_back(has);
		data.clear();
	}
}

void makeAllUnDelete(mbox_info& info)
{
	info.boxSize = 0;
	info.aval_msgs = info.num_msgs; //number of available messages is equal to number of all messages
	for(int i = 0; i < info.num_msgs; i++)
	{	info.status[i] = 0;
	info.boxSize += info.sizeOfMessage[i];}
}

void retrieveMsg(int ind, mbox_info& info, int fd)
{
	char wholeMsg[info.msgs[ind - 1].length() + 1];
	strcpy(wholeMsg, info.msgs[ind - 1].c_str());
	string line;
	int i = 0;

	while(wholeMsg[i] != '\0')
	{
		line += wholeMsg[i];
		if (wholeMsg[i] == '\n')
		{
			if(line.find("From ") == 0) // ignore the first line added by smtp server
			{line.clear();}
			else
			{writeTO(line, fd);// write all others line back to client
			line.clear();}
		}
		i++;
	}
	writeTO(".\r\n", fd);
}

void getLockMap(string nameOfFolder)
{
	bool check = false;
	DIR* folder;
	char * nf = new char[nameOfFolder.length() + 1];
	strcpy(nf, nameOfFolder.c_str());
	folder = opendir(nf);
	struct dirent *file;
	char filename[256];
	int files = 0;
	if (folder == NULL)
	{	cout << "folder not exist\n";
		exit(5);}

	char *p;
	while(file = readdir(folder)) // go to all file in the folder
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
			lock_status[u] = false;
			pthread_mutex_t key;
			pthread_mutex_init (&key, NULL);
			lock1[u] = key; // create the mutex lock key for each user
		}
	}
	delete [] nf;
	closedir(folder);
}



