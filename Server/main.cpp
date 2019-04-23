#include <iostream>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <set>
#include <map>
#include <algorithm>
#include <cmath>
#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <thread>
#include <signal.h>
#include <mutex>

std::recursive_mutex w_lock;

const int buff_size = 4096;
static std::map< std::string, std::multiset<int> > data;

void readfd(int fd);
double lerp(double t, double v0, double v1)
{
	return (1 - t)*v0 + t*v1;
}

double percentile(std::string ev, double q)
{
    double point = lerp(q, -0.5, data[ev].size() - 0.5);
    int left = std::max(int(std::floor(point)), 0);
    int right = std::min(int(std::ceil(point)), int(data[ev].size() - 1));

    int dataLeft = *std::next(data[ev].begin(), left);
    int dataRight = *std::next(data[ev].begin(), right);

	return lerp(point - left, dataLeft, dataRight);
}

void sig_handler(int sig)
{
    if (data.size())
    {
        std::lock_guard<std::recursive_mutex> lock(w_lock);
        for (const auto& pair: data)
            std::cout << pair.first << " min=" << *(data[pair.first].begin()) <<
                                                                  " 50%=" << percentile(pair.first, 0.5) <<
                                                                  " 90%=" << percentile(pair.first, 0.9) <<
                                                                  " 99%=" << percentile(pair.first, 0.99) <<
                                                                  " 99.9%=" << percentile(pair.first, 0.999) << "\n";
    }
}

int main()
{
    //signal stuff
    struct sigaction act;
    bzero(&act, sizeof (act));
    act.sa_handler = sig_handler;
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    act.sa_mask = set;
    sigaction(SIGUSR1, &act, nullptr);

    //server stuff
    int listenfd, connfd;
    struct sockaddr_in servaddr;

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket() error");
        exit(1);
    }
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(54000);
    const int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof (opt));

    if (bind(listenfd, (sockaddr*)&servaddr, sizeof(servaddr)) == -1)
    {
        perror("bind() error");
        exit(1);
    }

    if (listen(listenfd, SOMAXCONN) == -1)
    {
        perror("listen() error");
        exit(1);
    }

    for (;;)
    {
        if ((connfd = accept(listenfd, nullptr, nullptr)) == -1)
        {
            if (errno == EINTR)
                continue;
            perror("accept() error");
            exit(1);
        }
        std::thread thread(readfd, connfd);
        thread.detach();
    }
}

void readfd(int fd)
{
    char numbuffer[10];
    std::string strbuffer;
    ssize_t bytes_read;
    char buffer[buff_size];
    int tabs = 0;
    int i = 0;

    while ((bytes_read = read(fd, buffer, buff_size)) > 0)
    {
        for (int j = 0; j < bytes_read; ++j)
        {
            if (buffer[j] == '\t')
            {
                ++tabs;
                continue;
            }
            if (buffer[j] == '\n')
            {
                numbuffer[i] = '\0';
                int val = atoi(numbuffer);
                if (val)
                {
                    std::lock_guard<std::recursive_mutex> lock(w_lock);
                    data[strbuffer].insert(val);
                }
                strbuffer.clear();
                tabs = 0;
                i = 0;
                continue;
            }
            if (tabs == 1)
            {
                strbuffer.push_back(buffer[j]);
            }
            if (tabs == 15)
            {
                numbuffer[i] = buffer[j];
                ++i;
            }
        }
    }

    if (close(fd) == -1)
    {
        perror("close() error");
        exit(1);
    }
}
