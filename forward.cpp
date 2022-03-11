#ifndef __PRINT_MULTI_ARGS__
#define __PRINT_MULTI_ARGS__
#include <iostream>

template <typename T>
inline void Print(T t) { std::cout << t; }

template <typename T, typename... Args>
void Print(T t, Args... args)
{
	std::cout << t << " ";
	Print(args...);
}

template <typename... Args>
void Println(Args... args)
{
	Print(args...);
	std::cout << std::endl;
}

#endif

void PrintHelp()
{
	Print(R"(usage forward localport remoteaddr remoteport
forward 61111 192.168.1.1 22
)");
}

#include <network.hpp>
std::map<network::socket_fd, network::socket_fd> fdmap;

int NewForward(const char *remoteaddr, int remoteport)
{
	network::tcp::Client client(remoteaddr, remoteport);
	if (!client.Connect())
		return INVALID_SOCKET;
	return client.GetFd();
}

void Forward(int localport, const char *remoteaddr, int remoteport)
{
	Println(localport, remoteaddr, remoteport);
	network::tcp::Server server("127.0.0.1", localport);
	if (!server.Listen())
	{
		Println(server.Errno());
		return;
	}

	network::socket_fd sfd = server.GetFd();
	network::socket_fd maxfd = sfd;
	network::socket_fd cfd;
	network::socket_fd tofd;

	fd_set fdset, rlist;
	FD_ZERO(&fdset);
	FD_SET(sfd, &fdset);
	int count, size;
	unsigned int i;
	sockaddr_in clientaddr;
	network::socklen_t addrlen = sizeof(clientaddr);

	static constexpr int buffersize = 1024;
	static char buf[buffersize];

	network::socket_fd clientfdlist[1024];
	for (i = 0; i < 1024; i++)
		clientfdlist[i] = 0;
	for (;;)
	{
		rlist = fdset;
		count = select(maxfd + 1, &rlist, NULL, NULL, NULL);
		if (count == SOCKET_ERROR)
		{
			Println("socket error on I/O select");
			return;
		}
		if (FD_ISSET(sfd, &rlist))
		{
			count--;
			cfd = accept(sfd, (sockaddr *)&clientaddr, &addrlen);
			if (cfd == SOCKET_ERROR)
			{
				Println("accept socket failed");
				return;
			}
			tofd = NewForward(remoteaddr, remoteport);
			if (tofd != INVALID_SOCKET)
			{
				fdmap[cfd] = tofd;
				fdmap[tofd] = cfd;
				if (cfd > maxfd)
					maxfd = cfd;
				if (tofd > maxfd)
					maxfd = cfd;
				FD_SET(cfd, &fdset);
				FD_SET(tofd, &fdset);
				for (i = 0; i < 1024; i++)
				{
					if (clientfdlist[i] == 0)
					{
						clientfdlist[i] = cfd;
						break;
					}
				}
				for (i = 0; i < 1024; i++)
				{
					if (clientfdlist[i] == 0)
					{
						clientfdlist[i] = tofd;
						break;
					}
				}
			}
			else
			{
				closesocket(cfd);
				Println(WSAGetLastError());
			}
		}
		i = 0;
		while (count > 0)
		{
			cfd = clientfdlist[i];
			if (FD_ISSET(cfd, &rlist))
			{
				count--;
				size = recv(cfd, buf, buffersize, 0);
				if (size > 0)
				{
					size = send(fdmap.at(cfd), buf, size, 0);
					if (size == -1)
					{
						closesocket(fdmap.at(cfd));
						closesocket(cfd);
						FD_CLR(fdmap.at(cfd), &fdset);
						FD_CLR(cfd, &fdset);
						fdmap.erase(fdmap.at(cfd));
						fdmap.erase(cfd);
					}
				}
				else
				{
					closesocket(fdmap.at(cfd));
					closesocket(cfd);
					FD_CLR(fdmap.at(cfd), &fdset);
					FD_CLR(cfd, &fdset);
					fdmap.erase(fdmap.at(cfd));
					fdmap.erase(cfd);
				}
			}
			i++;
		}
	}
}

int main(int argc, char **argv)
{
	if (argc < 4)
	{
		PrintHelp();
		return 1;
	}
	Forward(atoi(argv[1]), argv[2], atoi(argv[3]));
}