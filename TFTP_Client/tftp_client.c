#include "tftp.h"

static struct sockaddr_in server_addr;
socklen_t server_length = sizeof (server_addr);

/*parse function*/
int parse_function(char *command, char ***argu)
{
	int i_index = 0, j_index = 0, length;

	/*compute the length of the command*/
	length = strlen(command);

	/*Running the loop upto length*/
	while (i_index <= length)
	{
		/*for the frst arg*/
		if (j_index == 0)

			/*allocating the memory*/
			*argu = calloc(1, sizeof(char *));

		else

			/*reallocating the memory*/
			*argu = realloc(*argu, (j_index + 1) * sizeof(char *));

		/*checking for space and null character*/
		if (command[i_index] == ' ' || command[i_index] == '\0')
		{
			/*storing the null character*/
			command[i_index] = '\0';

			/*allocating the memory*/
			*(*argu + j_index) = malloc(strlen(command) + 1);

			/*copying the string*/
			strcpy(*(*argu + j_index), command);

			/*increment index*/
			j_index++;

			/*storing the next address*/
			command = command + i_index + 1;
		}
		/*incremt the index*/
		i_index++;
	}
	/*reallocating the memory*/
	*argu = realloc(*argu, (j_index + 1) * sizeof(char *));

	/*storing the null*/
	*(*argu + j_index) = NULL;

	/*returning the count*/
	return j_index;
}


/* Sending request to server */
int send_request(int sock_fd, int request, char *fname, char *mode)
{
	/*Declaring the variables*/
	request_packet_t req_packet;
	packet_t packet;
	fd_set fdset;
	struct timeval tv;
	int retval;

	/*setting the time*/
	FD_ZERO(&fdset);
	FD_SET(sock_fd, &fdset);
	tv.tv_sec = 1;
	tv.tv_usec = 0;

	/*storing the details*/
	req_packet.opcode = request;
	strcpy(req_packet.fname, fname);
	strcpy(req_packet.mode, mode);

	printf("Sending request packet\n");

	/*sending the packet to server*/
	sendto(sock_fd, (void *)&req_packet, sizeof (req_packet), 0, (struct sockaddr *)&server_addr, server_length);

	/*recieving the packet from the server*/
	recvfrom(sock_fd, (void *)&packet, sizeof (packet), 0, (struct sockaddr *)&server_addr, &server_length);


	/*checking for the error packet*/
	if (packet.r_packet.opcode == ERROR)
	{
		printf("ERROR : %s\n", packet.e_packet.error_msg);
		return 0;
	}
	/*checking for the ack packet*/
	else if (packet.r_packet.opcode == ACK)
	{
		printf("Acknowledgement received for request packet\n");
		return 1;
	}
}


/*Function for opening the File*/
int open_files(char *fname, int flag)
{
	int fd, fd_flags;

	/*check for the flag is to open the file in read only mode*/
	if (flag == READ)
	{
		fd_flags = O_RDONLY;

		/*opening the file and handling the errors*/
		if ((fd = open (fname, fd_flags)) == -1)
		{
			printf("%s File is not exist\n", fname);
			return 0;
		}
	}

	/*check for the flag is to open the file in the write only mode*/
	else if (flag == CREATE)
	{
		fd_flags = O_CREAT | O_WRONLY | O_EXCL;

		/*opening the file and handling the errors*/
		if ((fd = open (fname, fd_flags, 0644)) == -1)
		{
			/*checking the errno to check if the file is exists or not*/
			if (errno == EEXIST)
			{
				printf("%s File already exists\n", fname);
				return 0;
			}
			perror("opening the file");
			return 0;
		}
	}
	/*returning the file descriptor*/
	return fd;
}


/*Function for get file*/
void get_file(int sock_fd, int fd)
{
	/*Declaring the variables*/
	int rcv_bytes;
	packet_t packet;
	ack_packet_t ack_packet;
	fd_set fdset;
	struct timeval tv;
	int retval, index = 0;

	while (1)
	{
		/*setting the time*/
		FD_ZERO(&fdset);
		FD_SET(sock_fd, &fdset);
		tv.tv_sec = 1;
		tv.tv_usec = 0;

		/*To monitor the multiple file descriptors*/
		retval = select(sock_fd + 1, &fdset, NULL, NULL, &tv);

		/*Handling the errors*/
		if (retval == -1)
			perror("select()");

		/*if the valid file descriptor is present*/
		else if (retval)
		{
			/*recieving the packet from the server for connection*/
			rcv_bytes = recvfrom(sock_fd, (void *)&packet, sizeof (packet), 0, NULL, NULL);

			/*checking for the data packet*/
			if (packet.r_packet.opcode == DATA)
			{
				/*comparing the data packet num and ack packet num*/
				if (packet.d_packet.block_num == (ack_packet.block_num + 1))
				{
					/*writing into the file*/
					write(fd, packet.d_packet.data, rcv_bytes - 4);
					printf("Data packet %d recieved\n", packet.d_packet.block_num);
					printf("Sending the acknowledgement for block num %d\n",packet.d_packet.block_num);
				}
				else
					printf("Resending the acknowledgemnt for block num %d\n", packet.d_packet.block_num);

				/*storing the ack packte details*/
				ack_packet.opcode = ACK;
				ack_packet.block_num = packet.d_packet.block_num;

				index++;

				/*Blocking the packet*/
				if (index != 3)
					sendto(sock_fd, (void *)&ack_packet, sizeof (ack_packet), 0, (struct sockaddr *)&server_addr, server_length);

			}
			/*checking for the end packet*/
			else if (packet.r_packet.opcode == END)
				break;
		}
	}
}


/*Function for putting the file into server*/
void put_file(int sock_fd, int fd)
{
	/*Declaring the variables*/
	int blk_num = 0, rcv_bytes = 0;
	packet_t packet;
	data_packet_t data_pack;
	fd_set fdset;
	struct timeval tv;
	int retval, flag = 0;
			
	/*setting the time*/
	FD_ZERO(&fdset);
	FD_SET(sock_fd, &fdset);
	tv.tv_sec = 1;
	tv.tv_usec = 0;

	data_pack.opcode = DATA;

	while (1)
	{
		/*reading the contents of file and storing into the data packet*/
		if ((rcv_bytes = read(fd, data_pack.data, MAX_SIZE)) != 0)
		{
			/*Handling the errors*/
			if (rcv_bytes == -1)
			{
				perror("read");
				return;
			}

			/*incrementing the block num*/
			blk_num++;

			/*storing the data block num*/
			data_pack.block_num = blk_num;

			printf("Sending the packet %d\n", blk_num);
		}
		else
		{
			/*Storing the end packet opcode*/
			data_pack.opcode = END;

			/*sending to the server*/
			sendto(sock_fd, (void *)&data_pack, sizeof (data_pack), 0, (struct sockaddr *)&server_addr, server_length);
			break;
		}

		while (1)
		{
			/*sending the data to server*/
			sendto(sock_fd, (void *)&data_pack, rcv_bytes + 4, 0, (struct sockaddr *)&server_addr, server_length);

			/*setting the time*/
			FD_ZERO(&fdset);
			FD_SET(sock_fd, &fdset);
			tv.tv_sec = 1;
			tv.tv_usec = 0;

			/*To monitor the multiple file descriptors*/
			retval = select(sock_fd + 1, &fdset, NULL, NULL, &tv);

			/*Handling the errors*/
			if (retval == -1)
				perror("select()");

			/*If the valid file descreptor is present*/
			else if (retval)
			{
				/*recieving the packet*/
				recvfrom(sock_fd, (void *)&packet, sizeof (packet), 0, NULL, NULL);

				/*comparing the ack blk num and data blk num*/
				if (packet.a_packet.block_num == data_pack.block_num)
				{
					printf("Acknowledgement packet for block num %d\n", packet.a_packet.block_num);
					break;
				}
			}
			printf("Resending the packet %d\n", data_pack.block_num);
		}
	}
}


/*Main Program*/
int main()
{
	/*Declaring the variables*/
	int sock_fd, data_sock_fd, argc, serv_flag = 0, port;
	char command[100];
	char **argu = NULL;

	/*creating the socket*/
	sock_fd = socket(AF_INET, SOCK_DGRAM, 0);

	while (1)
	{

		memset(&command, 0, sizeof (command));

		/*Reading the command*/
		printf("[ tftp ] $ ");
		scanf("%[^\n]", command);
		__fpurge(stdin);

		if (strlen(command) == 0)
			continue;

		/*invoking the reading the inputs function*/
		argc = parse_function(command, &argu);

		/*storing the server port details*/
		server_addr.sin_port = htons(SERVER_PORT);

		/*checking for the connect command*/
		if (strcmp(argu[0], "connect") == 0)
		{
			/*storing the details*/
			server_addr.sin_family = AF_INET;
			server_addr.sin_addr.s_addr = inet_addr(argu[1]);
			server_addr.sin_port = htons(SERVER_PORT);
			
			memset(server_addr.sin_zero, '\0', sizeof (server_addr.sin_zero));

			/*setting the flag*/
			serv_flag = 1;
		}

		/*if the server flag is set....connction estabished successsfully*/
		if (serv_flag)
		{
			/*checking for the get command to get the file from the remote host*/
			if (strcmp(argu[0], "get") == 0)
			{
				/*opening the file*/
				if ((data_sock_fd = open_files(argu[1], CREATE)))
				{
					/*sending the request for read request*/
					if (send_request(sock_fd, RRQ, argu[1], argu[2]))
					{
						printf("Connection Established Successfully\n");
						
						/*invoking the get file function*/
						get_file(sock_fd, data_sock_fd);

						/*closing the file*/
						close(data_sock_fd);
					}
				}
			}
			/*checking for the put command to put the file from client to the remote host*/
			else if (strcmp(argu[0], "put") == 0)
			{
				/*opening the file*/
				if ((data_sock_fd = open_files(argu[1], READ)))
				{
					/*sending the request for write request*/
					if (send_request(sock_fd, WRQ, argu[1], argu[2]))
					{
						printf("Connection Established Successfully\n");

						/*invoking the put file function*/
						put_file(sock_fd, data_sock_fd);

						/*closing the file*/
						close(data_sock_fd);
					}
				}
			}
		}
		else
			printf("Connect to the server\n");

		/*checking for the exit command*/
		if (strcmp(argu[0], "bye") == 0 || strcmp(argu[0], "quit") == 0 || strcmp(argu[0], "exit") == 0)
			break;
	}
	/*closing the socket*/
	close(sock_fd);
	return 0;
}
