#include <iostream>
#include <fstream>
#include <unistd.h>
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

static std::recursive_mutex w_lock;

const int buff_size = 4096;
static std::map< std::string, std::multiset<int> > data;

static char *input;
static char *output;

void readfd(int fd);
double percentile(std::string ev, double q);

void sig_handler(int)
{
    if (data.size())
    {
        std::ofstream out;
        std::streambuf *coutbuf;
        if (output)
        {
            out.open(output);
            coutbuf = std::cout.rdbuf();
            std::cout.rdbuf(out.rdbuf());
        }

        std::lock_guard<std::recursive_mutex> lock(w_lock);
        for (const auto& pair: data)
        {
            std::string ev = pair.first;
            std::cout << ev << " min=" << *(data[pair.first].begin()) <<
                                                                  " 50%=" << percentile(ev, 0.5) <<
                                                                  " 90%=" << percentile(ev, 0.9) <<
                                                                  " 99%=" << percentile(ev, 0.99) <<
                                                                  " 99.9%=" << percentile(ev, 0.999) << "\n";

            std::cout << "ExecTime\tTransNo\tWeight,%\tPercent\n";
            unsigned int s = 0;
            double p = 0;
            int i;
            for (i = *(data[ev].begin()); i <= *(std::prev(data[ev].end())); ++i)
            {
                s += data[ev].count(i);
                if (i % 5 == 0)
                {
                    p += (double)s / data[ev].size();
                    std::cout << i << "\t" << s << "\t" << (double)s*100 / data[ev].size() << "\t" << p*100 << "\n";
                    s = 0;
                }
            }
            if (i % 5) //still have some data
            {
                while (i % 5)
                    ++i;
                std::cout << i << "\t" << s << "\t" << (double)s*100 / data[ev].size() << "\t" << p*100 << "\n";
            }
        }
        if (output)
            std::cout.rdbuf(coutbuf);

    }
    else
        std::cout << "No data to work with\n";
}

int main(int argc, char *argv[])
{
    //command line arguments
    int opt;
    while ((opt = getopt(argc, argv, "i:o:")) != -1)
    {
        switch (opt)
        {
        case 'i':
            input = optarg;
            break;
        case 'o':
            output = optarg;
            break;
        default:
            std::cerr << "Usage: " << argv[0] << " [-i] input_file [-o] output_file\n";
            exit(1);
        }
    }

    if (input)
    {
        int filefd;
        if ((filefd = open(input, O_RDONLY)) == -1)
            perror("open() error"); //probably got wrong file path
        else
        {
            std::thread thread(readfd, filefd);
            thread.detach();
        }
    }

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
    const int one = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof (one));

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

double percentile(std::string ev, double q)
{
    double rank, rankInt, rankFrac;
    rank = q * (data[ev].size() - 1);
    rankFrac = modf(rank, &rankInt);
    int elValue = *std::next(data[ev].begin(), (int)rankInt);
    int elPlusOneValue = *std::next(data[ev].begin(), (int)rankInt + 1);
    return elValue + rankFrac * (elPlusOneValue - elValue);
}
