/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bahaas <bahaas@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2021/07/28 16:53:09 by bahaas            #+#    #+#             */
/*   Updated: 2021/07/28 22:26:58 by bahaas           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "includes/ft_ping.h"


static int help()
{
	printf("Usage: ping [-aAbBdDfhLnOqrRUvV64] [-c count] [-i interval] [-I interface]\n"
			"\t    [-m mark] [-M pmtudisc_option] [-l preload] [-p pattern] [-Q tos]\n"
			"\t    [-s packetsize] [-S sndbuf] [-t ttl] [-T timestamp_option]\n"
			"\t    [-w deadline] [-W timeout] [hop1 ...] destination\n");
	printf("Usage: ping -6 [-aAbBdDfhLnOqrRUvV64] [-c count] [-i interval] [-I interface]\n"
			"\t     [-l preload] [-m mark] [-M pmtudisc_option]\n"
			"\t     [-N nodeinfo_option] [-p pattern] [-Q tclass] [-s packetsize]\n"
			"\t     [-S sndbuf] [-t ttl] [-T timestamp_option] [-w deadline]\n"
			"\t     [-W timeout] destination\n");
	return (0);
}

double	get_time(void)
{
	struct timeval	tv;
	double			end;

	gettimeofday(&tv, NULL);
	end = ((tv.tv_sec * 1000) + tv.tv_usec / 1000);
	return (end);
}

double	get_elapsed_time(double starter)
{
	struct timeval	tv;
	double			end;

	gettimeofday(&tv, NULL);
	end = ((tv.tv_sec * 1000) + tv.tv_usec / 1000);
	return (end - starter);
}

void set_rtt_stats(double rtt)
{
	if(params.time.min == 0 || rtt < params.time.min)
		params.time.min = rtt;
	if(params.time.max == 0 || rtt > params.time.max)
		params.time.max = rtt;
	params.time.total += rtt;
	params.time.avg = params.time.total / params.received_packets;
}

double get_mdev()
{
	double mdev;
	double avg;
	double avg_square;

	avg = params.time.total / params.received_packets;
	avg_square = (params.time.total * params.time.total) / params.received_packets;
	mdev = sqrt(avg_square - (avg * avg));
	return (mdev);
}

void print_stats()
{
	long time;
	double mdev;

	time = get_elapsed_time(params.start);
	if(params.received_packets == 1)
		time = 0;
	printf("\n--- %s params statistics ---\n", params.reversed_address);
	printf("%d packets transmitted, %d received, %d%% packet loss, time %ldms\n", params.sent_packets, params.received_packets, 0, time);
	printf("rtt min/avg/max/mdev = %.3lf/%.3lf/%.3lf/%.3lf ms\n", params.time.min, params.time.avg, params.time.max, get_mdev());
}

void	error_output(char *message)
{
	fprintf(stderr, "%s\n", message);
}

void	error_output_and_exit(char *message)
{
	fprintf(stderr, "%s\n", message);
	exit(1);
}

void ft_getadress(char *host_name)
{
	struct addrinfo hints;
	struct addrinfo *res = NULL;
	char address[INET_ADDRSTRLEN];

	ft_bzero(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_CANONNAME;
	if(getaddrinfo(host_name, NULL, &hints, &res) != 0)
	{
		printf("error in getaddress\n");
		return ;
	}
	params.reversed_address = res->ai_canonname;
	ft_memcpy(&params.sockaddr, res->ai_addr, sizeof(struct sockaddr_in));
	inet_ntop(AF_INET, &(params.sockaddr.sin_addr), address, INET_ADDRSTRLEN);
	params.address = ft_strdup(address);
	//	printf("address: %s\n", params.address);
	//	printf("reversed_address: %s\n", params.reversed_address);
}

void	create_socket(void)
{
	int				socket_fd;
	struct timeval	timeout;

	timeout.tv_sec = 1;
	timeout.tv_usec = 0;
	if ((socket_fd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) == -1)
		error_output_and_exit(SOCKET_ERROR);
	if ((setsockopt(socket_fd, IPPROTO_IP, IP_TTL, &(params.ttl), sizeof(params.ttl))) == -1)
		error_output_and_exit(SETSOCKOPT_ERROR);
	if ((setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, (const void *)&timeout, sizeof(timeout))) == -1)
		error_output_and_exit(SETSOCKOPT_ERROR);
	params.socket_fd = socket_fd;
}


unsigned short	checksum(void *address, int len)
{
	unsigned short	*buff;
	unsigned long	sum;

	buff = (unsigned short *)address;
	sum = 0;
	while (len > 1)
	{
		sum += *buff;
		buff++;
		len -= sizeof(unsigned short);
	}
	if (len)
		sum += *(unsigned char *)buff;
	sum = (sum >> 16) + (sum & 0xFFFF);
	sum += (sum >> 16);
	return ((unsigned short)~sum);
}

char			send_packet(t_packet *packet)
{
	ssize_t sent_bytes;

	sent_bytes = sendto(params.socket_fd, packet, sizeof(*packet), 0,
			(struct sockaddr*)&params.sockaddr, sizeof(params.sockaddr));
	if (sent_bytes <= 0)
	{
		if (errno == ENETUNREACH)
			error_output(NO_CONNEXION_ERROR);
		else
			error_output(SENDTO_ERROR);
		return (ERROR_CODE);
	}
	return (SUCCESS_CODE);
}

void			set_time(struct timeval *destination)
{
	if (gettimeofday(destination, NULL) == -1)
		error_output_and_exit(TIMEOFDAY_ERROR);
}

char			check_reply(t_reply *reply)
{
	struct ip	*packet_content;

	packet_content = (struct ip *)reply->receive_buffer;
	if (packet_content->ip_p != IPPROTO_ICMP)
	{
		//	if (params.flags & V_FLAG)
		//		error_output(REPLY_ERROR);
		return (ERROR_CODE);
	}
	reply->icmp = (struct icmp *)(reply->receive_buffer + (packet_content->ip_hl << 2));
	if (reply->icmp->icmp_type == 11 && reply->icmp->icmp_code == 0)
	{
		return (TTL_EXCEEDED_CODE);
	}
	else if (BSWAP16(reply->icmp->icmp_id) != params.process_id || BSWAP16(reply->icmp->icmp_seq) != params.seq)
	{
		init_reply(reply);
		return (receive_reply(reply));
	}
	return (SUCCESS_CODE);
}

char	receive_reply(t_reply *reply)
{
	reply->received_bytes = recvmsg(params.socket_fd, &(reply->msghdr), 0);
	if (reply->received_bytes > 0)
	{
		return (check_reply(reply));
	}
	else
	{
		if ((errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))
		{
			;
		}
		else
			error_output(RECV_ERROR);
		return (ERROR_CODE);
	}
	return (SUCCESS_CODE);
}

double			calculate_elapsed_time(struct timeval start, struct timeval end)
{
	return (((double)((double)end.tv_sec - (double)start.tv_sec) * 1000) +
			(double)((double)end.tv_usec - (double)start.tv_usec) / 1000);
}

void	display_sequence(int received_bytes, t_reply reply, struct timeval start_timestamp, struct timeval end_timestamp)
{
	short		reply_ttl;
	double		time_elapsed;
	struct ip	*packet_content;

	packet_content = (struct ip *)reply.receive_buffer;
	reply_ttl = (short)packet_content->ip_ttl;
	time_elapsed = calculate_elapsed_time(start_timestamp, end_timestamp);
	//time_elapsed = get_elapsed_time();
	if(params.flags & D)
	{
		//double		curr_time;
		//curr_time = get_time();
		//printf("[%lf] ", curr_time);
	}
	if (ft_strcmp(params.address, params.user_requested_address))
	{
		printf("%lu bytes from %s (%s): icmp_seq=%d ttl=%d time=%.3lf ms\n", received_bytes - sizeof(struct ip),
				params.reversed_address, params.address, params.seq, reply_ttl, time_elapsed);
	}
	else
	{
		printf("%lu bytes from %s: icmp_seq=%d ttl=%d time=%.3lf ms\n", received_bytes - sizeof(struct ip),
				params.address, params.seq, reply_ttl, time_elapsed);
	}
	set_rtt_stats(time_elapsed);
}

void			wait_interval(struct timeval start)
{
	struct timeval	current_time;
	struct timeval	goal_time;

	if (params.interval)
	{
		current_time = start;
		goal_time.tv_sec = current_time.tv_sec + (long)params.interval;
		goal_time.tv_usec = current_time.tv_usec + (long)((params.interval - (long)params.interval) * 1000000);
		while (timercmp(&current_time, &goal_time, <))
		{
			set_time(&current_time);
		}
	}
}

void			ping_loop(void)
{
	t_packet			packet;
	struct timeval		current_start_timestamp;
	struct timeval		current_ending_timestamp;
	t_reply				reply;
	char				check;

	printf("PING %s (%s) 56(84) bytes of data.\n", params.reversed_address, params.address);
	params.start = get_time();
	set_time(&params.starting_time);
	while(params.quit == 0)
	{
		set_time(&current_start_timestamp);
		init_packet(&packet, current_start_timestamp);
		params.sent_packets++;
		check = send_packet(&packet);
		if (check == SUCCESS_CODE)
		{
			init_reply(&reply);
			check = receive_reply(&reply);
			if (check == SUCCESS_CODE)
			{
				params.received_packets++;
				set_time(&current_ending_timestamp);
				display_sequence(reply.received_bytes, reply, current_start_timestamp, current_ending_timestamp);;
			}
			else if (check == TTL_EXCEEDED_CODE)
			{
				//		display_exceeded_sequence(reply);
				//		params.error_packets++;
			}
		}
		if(params.opts.count)
		{
			params.opts.count--;
			if(!params.opts.count)
			{
				print_stats();
				break;
			}
		}	
		params.seq++;
		wait_interval(current_start_timestamp);
	}
}

void	get_count(char **av, int *i, int j)
{
	int	count;

	if (av[*i][j + 1] == '\0' && av[*i + 1] != NULL)
	{
		params.opts.count = ft_atoi(av[*i + 1]);	
		++*i;
	}
	else if (ft_isdigit(av[*i][j + 1]))
	{
		params.opts.count = ft_atoi(&av[*i][j + 1]);
	}
	else
		printf("erro count opt\n");
}

int parsing(int ac, char **av)
{
	if(ac < 2)
		return(help());
	for(int i = 1; i < ac; i++)
	{
		if(av[i][0] == '-')
		{
			int opt = 1;
			for(int j = 1; av[i][j] && opt; j++)
			{
				switch (av[i][j])
				{
					case 'v': // 
						params.flags |= V;
						break;
					case 'h': //help display
						return(help());
					case 'c': //count packet has to be sent
						get_count(av, &i, j);
						opt = 0;
						break;
					case 'D': //timestamp before each line;
						params.flags |= D;
						break;
					default: //wrong opt
						printf("ping: Invalid option -- '%c'\n", av[i][j]);
						return(help());
				}
			}
		}
		else
		{
			params.user_requested_address = av[i];
			return (true);
		}
	}
	return (true);
}

int		main(int ac, char **av)
{
	if (parsing(ac, av))
	{
		init(ac, av);
		set_signal();
		if (params.address)
			create_socket();
		if (params.socket_fd != -1)
			ping_loop();
	}
	return (0);
}
