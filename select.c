#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#define max(x, y) ( (x) > (y) ? (x) : (y) )
typedef struct connector
{
	size_t size;
	size_t bufsize;
	int fr;
	char *point;
	int fw;
	char *buf;
}con;
int minpow(int i, int j)
{
	int k,f, err;
	k = j;
	f = i;
	for (j=1; j<k; j++)
	{
		f = f * i;
		if (f > 131072) return 131072;
	}
	return f;
}
int main(int argc, char *argv[])
{
	int i, n, nfds, sel;
	pid_t pid;
	int fd1[2], fd2[2];
	struct timeval timeout;
	fd_set readfds, writefds, r, w;
	con *par;
	con chld;
	if (argc != 3)
	{
		printf("Bad input\n");
		exit(1);
	}
	n = atoi(argv[1]);
	if (n < 0)
	{
		printf("Bad input\n");
		exit(1);
	}
	fd2[0] = open(argv[2], O_RDONLY);
	if (fd2[0] == -1)
	{
		printf("Can't open file\n");
		exit(1);
	}
	par = malloc(n * sizeof(*par));
	nfds = 0;
	timeout.tv_sec = 2;
	timeout.tv_usec = 0;
	FD_ZERO(&readfds);
	FD_ZERO(&writefds);
	FD_ZERO(&w);
	FD_ZERO(&r);
	for (i=0; i<n; i++)
	{
		chld.fr = fd2[0];
		if(pipe(fd1) == -1) exit(1);
		if(pipe(fd2) == -1) exit(1);
		pid = fork();
		if (pid < 0) exit(1);
		if (pid > 0)
		{
			par[i].fr = fd1[0];
			if (fcntl(par[i].fr, F_SETFL, O_NONBLOCK) == -1) exit(1);
			FD_SET(par[i].fr, &readfds);
			par[i].fw = fd2[1];
			if (fcntl(par[i].fw, F_SETFL, O_NONBLOCK) == -1) exit(1);
			close(fd1[1]);
			par[i].size = 0;
			par[i].bufsize = minpow(3, n-i+4);
			par[i].buf = malloc(par[i].bufsize);
			par[i].point = par[i].buf;
			nfds = max(nfds, fd1[0]+1);
			if (i<n-1) nfds = max(nfds, fd2[1]+1);
		}
		if (pid == 0)
		{
			close(fd1[0]);
			close(fd2[1]);
			chld.fw = fd1[1];
			chld.bufsize = minpow(3, n-i+4);
			chld.buf = malloc(chld.bufsize);
			for (int j =0 ;j<= i; j++)
			{
				close(par[j].fr);
				close(par[j].fw);
			}
			free(par);
			break;
		}
	}
	if (pid>0)
	{
		close(fd2[0]);
		close(fd2[1]);
		close(chld.fr);
		close(chld.fw);
		par[n-1].fw = STDOUT_FILENO;
		nfds = max(nfds, par[n-1].fw + 1);
		if (fcntl(par[n-1].fw, F_SETFL, O_NONBLOCK) == -1) exit(1);
		while(1)
		{
			r = readfds;
			w = writefds;
			sel = select(nfds, &r, &w, NULL, &timeout);
			if (sel == 0)
			{
				printf("something goes wrong...\n");
				 exit(1);
			}
			if (sel < 0) exit(1);
			for (i=0; i<n; i++)
			{
				if(FD_ISSET(par[i].fr, &r))
				{
					par[i].size = read(par[i].fr, par[i].buf, par[i].bufsize);
					if (par[i].size < 0) exit(1);
					FD_CLR(par[i].fr, &readfds);
					FD_SET(par[i].fw, &writefds);
					if (par[i].size == 0)
					{
						FD_CLR(par[i].fw, &writefds);
						close(par[i].fr);
						close(par[i].fw);
					}
					if (par[i].size == 0 && i == n-1) exit(0);
				}
				if(FD_ISSET(par[i].fw, &w))
				{
					int point = write(par[i].fw, par[i].point, par[i].size);
					if (point < 0) exit(1);
					par[i].point+= point;
					par[i].size-=point;
					if (par[i].size == 0)
					{
						FD_CLR(par[i].fw, &writefds);
						FD_SET(par[i].fr, &readfds);
						par[i].point = par[i].buf;
					}
				}
			}
		}
	}
	if (pid == 0)
	{
		while(1)
		{
			chld.size = read(chld.fr, chld.buf, chld.bufsize);
			if (chld.size < 0) exit(1);
			if (chld.size == 0) exit(0);
			write(chld.fw, chld.buf, chld.size);
		}
	}
	exit(0);
}
