#include "tftp.h"

error_packet_t error_pack;

/*Function for opening the File*/
int open_file(char *fname, int flag)
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
			error_pack.opcode = ERROR;
			strcpy(error_pack.error_msg, "File not exist in server");
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
				error_pack.opcode = ERROR;
				strcpy(error_pack.error_msg, "File already exist in server");
				return 0;
			}
			perror("opening the file");
			return 0;
		}
	}
	/*returning the file descriptor*/
	return fd;
}


/*Function for get file from client*/
void get_file(int sock_fd, int fd, struct sockaddr_in client_addr)
{
	/*Declaring the variables*/
	int rcv_bytes;
	packet_t packet;
	ack_packet_t ack_packet;
	fd_set fdset;
	struct timeval tv;
	int retval, index = 0;

	socklen_t client_length = sizeof (client_addr);

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
					sendto(sock_fd, (void *)&ack_packet, sizeof (ack_packet), 0, (struct sockaddr *)&client_addr, client_length);
			}
			/*checking for the end packet*/
			else if (packet.r_packet.opcode == END)
				break;
		}
	}
}

/*Function for putting the file into client*/
void put_file(int sock_fd, int fd, struct sockaddr_in client_addr)
{
	/*Declaring the variables*/
	int blk_num = 0, rcv_bytes = 0;
	packet_t packet;
	data_packet_t data_pack;
	fd_set fdset;
	struct timeval tv;
	int retval, flag = 0;
	socklen_t client_length = sizeof (client_addr);

	data_pack.opcode = DATA;
	error_pack.opcode = ERROR;

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

			/*sending to the client*/
			sendto(sock_fd, (void *)&data_pack, sizeof (data_pack), 0, (struct sockaddr *)&client_addr, client_length);

			break;
		}


		while (1)
		{
			/*sending the data to client*/
			sendto(sock_fd, (void *)&data_pack, rcv_bytes + 4, 0, (struct sockaddr *)&client_addr, client_length);

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
	int sock_fd, data_sock_fd, blk_num = 0, new_port;
	struct sockaddr_in server_addr, client_addr;
	socklen_t server_length, client_length;
	packet_t packet;
	ack_packet_t ack_packet;
	pid_t pid;

	/*computing the length*/
	server_length = sizeof (server_addr);
	client_length = sizeof (client_addr);


	printf("Server is waiting....\n");

	/*creating the socket*/
	sock_fd = socket(AF_INET, SOCK_DGRAM, 0);

	/*storing the server details*/
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
	server_addr.sin_port = htons(SERVER_PORT);

	memset(server_addr.sin_zero, '\0', sizeof(server_addr.sin_zero));

	//binding the address and port number
	bind(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));

	while (1)
	{
		memset(&client_addr, 0, client_length);

		/*Recieving the data */
		recvfrom(sock_fd, (void *)&packet, sizeof (packet), 0, (struct sockaddr*)&client_addr, &client_length);

		/*incrementing the block num*/
		blk_num++;

		/*incrementing the port num*/
		new_port = SERVER_PORT + blk_num;


		/*creating the child*/
		pid = fork();

		switch (pid)
		{
			//on error returns -1
			case -1:
				printf("Fork(): Failed\n");
				exit(0);

			//returns 0 to child	
			case 0:

				/*closing the socket*/
				close(sock_fd);

				/*storing the server details*/
				sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
				server_addr.sin_port = htons(new_port);

				/*binding the details*/
				bind(sock_fd, (struct sockaddr *)&server_addr, server_length);

				printf("%s\n", inet_ntoa(server_addr.sin_addr));
				printf("%d\n", ntohs(server_addr.sin_port));

				/*checking for the read request*/
				if (packet.r_packet.opcode == RRQ)
				{
					/*opring the files*/
					if ((data_sock_fd = open_file(packet.r_packet.fname, READ)))
					{
						/*storing the ack details*/
						ack_packet.opcode = ACK;
						ack_packet.block_num = 0;

						/*sending the ack packet*/
						sendto(sock_fd, (void *)&ack_packet, sizeof (ack_packet), 0, (struct sockaddr *)&client_addr, client_length);

						printf("Acknowledgement sent for request\n");

						/*invoking the put file*/
						put_file(sock_fd, data_sock_fd, client_addr);

						/*closing the file*/
						close(data_sock_fd);
					}
					else
					{
						/*sending the error packet*/
						sendto(sock_fd, (void *)&error_pack, sizeof (error_pack), 0, (struct sockaddr *)&client_addr, client_length);
					}
				}

				/*sending the data for the write request*/
				else if (packet.r_packet.opcode == WRQ)
				{
					/*opening the files*/
					if ((data_sock_fd = open_file(packet.r_packet.fname, CREATE)))
					{
						/*storing the ack packet details*/
						ack_packet.opcode = ACK;
						ack_packet.block_num = 0;

						/*sending the ack packet*/
						sendto(sock_fd, (void *)&ack_packet, sizeof (ack_packet), 0, (struct sockaddr *)&client_addr, client_length);

						printf("Acknowledgement sent for request\n");

						/*invoking the get file function call*/
						get_file(sock_fd, data_sock_fd, client_addr);

						/*closing the file*/
						close(data_sock_fd);
					}
					else
					{
						/*sending the error packet*/
						sendto(sock_fd, (void *)&error_pack, sizeof (error_pack), 0, (struct sockaddr *)&client_addr, client_length);
					}

					/*closing the socket*/
					close(sock_fd);

					exit(0);

					default:
						;
				}
		}
	}
	return 0;
}







