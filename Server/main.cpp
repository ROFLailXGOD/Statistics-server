#include <iostream>
#include <fstream>
#include <iomanip>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <numeric>
#include <map>
#include <algorithm>
#include <cmath>
#include <sys/socket.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <thread>
#include <signal.h>
#include <mutex>
#include <chrono>

static std::recursive_mutex w_lock;

const int buff_size = 4096;
static std::map< std::string, std::map<int, unsigned long> > data;

static char *input;
static char *fifo;
static char *output;

void udpconn();
void readfd(int fd, int fifoflag);
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
        auto start = std::chrono::high_resolution_clock::now();
        for (const auto& pair: data)
        {
            std::string ev = pair.first;
            const unsigned long size = std::accumulate(std::begin(data[ev]), std::end(data[ev]), (unsigned long)0,
                                                       [](const unsigned long previous, const auto& element)
                                                       { return previous + element.second; });
            std::cout << ev << " min=" << data[pair.first].begin()->first <<
                                                                  " 50%=" << percentile(ev, 0.5) <<
                                                                  " 90%=" << percentile(ev, 0.9) <<
                                                                  " 99%=" << percentile(ev, 0.99) <<
                                                                  " 99.9%=" << percentile(ev, 0.999) << "\n";

            std::cout << "ExecTime\tTransNo\tWeight,%\tPercent\n";
            unsigned long s = 0;
            unsigned long p = 0;
            int i;
            for (i = data[ev].begin()->first; i <= std::prev(data[ev].end())->first; ++i)
            {
                if (data[ev].count(i))
                    s += data[ev][i];
                if (i % 5 == 0)
                {
                    p += s;
                    std::cout << i << "\t" << s << "\t" << s*100.0 / size << "\t" << p*100.0 / size << "\n";
                    s = 0;
                }
            }
            if (i % 5) //still have some data
            {
                while (i % 5)
                    ++i;
                p += s;
                std::cout << i << "\t" << s << "\t" << s*100.0 / size << "\t" << p*100.0 / size << "\n";
            }
        }
        if (output)
            std::cout.rdbuf(coutbuf);
        auto finish = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = finish - start;
        std::cout << elapsed.count() << "\n";

    }
    else
        std::cout << "No data to work with\n";
}

int main(int argc, char *argv[])
{
    //command line arguments
    int opt;
    while ((opt = getopt(argc, argv, "i:f:o:")) != -1)
    {
        switch (opt)
        {
        case 'i':
            input = optarg;
            break;
        case 'f':
            fifo = optarg;
            break;
        case 'o':
            output = optarg;
            break;
        default:
            std::cerr << "Usage: " << argv[0] << " [-i] input_file [-f] fifo [-o] output_file\n";
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
            std::thread thread(readfd, filefd, 0);
            thread.detach();
        }
    }
    if (fifo)
    {
        if (mkfifo(fifo, 0666) == -1)
        {
            if (errno != EEXIST)
            {
                perror("mkfifo() error");
                exit(1);
            }
        }
        int filefd;
        if ((filefd = open(fifo, O_RDONLY)) == -1)
            perror("open() error"); //probably got wrong file path
        else
        {
            std::thread thread(readfd, filefd, 1);
            thread.detach();
        }
    }

    //signal stuff
    //upd
    std::thread thread(udpconn);
    thread.detach();
    //tcp
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

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("socket() error");
        exit(1);
    }
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(54000);
    const int one = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof (one));

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
        std::thread thread(readfd, connfd, 0);
        thread.detach();
    }
}

std::string to_string(double d)
{
    std::ostringstream stm;
    stm << std::fixed << std::setprecision(2) << d;
    return stm.str();
}

void udpconn()
{
    int sockfd;
    struct sockaddr_in servaddr, cliaddr;
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    {
        perror("socket() error");
        exit(1);
    }
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(54000);

    if (bind(sockfd, (sockaddr*)&servaddr, sizeof(servaddr)) == -1)
    {
        perror("bind() error");
        exit(1);
    }

    int bytes;
    socklen_t len;
    char buff[1024];

    for (;;)
    {
        len = sizeof (cliaddr);
        if ((bytes = recvfrom(sockfd, buff, 1024, 0, (sockaddr*)&cliaddr, &len)) == -1)
        {
            perror("recvfrom() error");
            exit(1);
        }

        std::string msg;
        if (bytes)
        {
            std::string ev(buff, bytes-1);
            if (data.find(ev) == data.end())
            {
                msg = "Event " + ev + " not found\n";
            }
            else
            {
                std::lock_guard<std::recursive_mutex> lock(w_lock);
                msg = ev + " min=" + to_string(data[ev].begin()->first) +
                               " 50%=" + to_string(percentile(ev, 0.5)) +
                               " 90%=" + to_string(percentile(ev, 0.9)) +
                               " 99%=" + to_string(percentile(ev, 0.99)) +
                               " 99.9%=" + to_string(percentile(ev, 0.999)) + "\n";
            }
        }

        if (sendto(sockfd, msg.c_str(), msg.length(), 0, (sockaddr*)&cliaddr, len) == -1)
        {
            perror("sendto() error");
            exit(1);
        }
    }
}

void readfd(int fd, int fifoflag)
{
    char numbuffer[10];
    std::string strbuffer;
    ssize_t bytes_read;
    char buffer[buff_size];
    int tabs = 0;
    int i = 0;

    auto start = std::chrono::high_resolution_clock::now();
    while (((bytes_read = read(fd, buffer, buff_size)) > 0) || fifoflag)
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
                    ++data[strbuffer][val];
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
    auto finish = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = finish - start;
//    std::cout << elapsed.count() << "\n";

    if (close(fd) == -1)
    {
        perror("close() error");
        exit(1);
    }
}

double percentile(std::string ev, double q)
{
    double rank, rankInt, rankFrac;
    const unsigned long size = std::accumulate(std::begin(data[ev]), std::end(data[ev]), (unsigned long)0,
                                               [](const unsigned long previous, const auto& element)
                                               { return previous + element.second; });
    rank = q * (size - 1) + 1;
    rankFrac = modf(rank, &rankInt);
    unsigned long sum = 0;
    std::map<int, unsigned long>::iterator it;
    for (it = data[ev].begin(); sum < (unsigned long)rankInt; ++it)
    {
        sum += it->second;
    }
    int elValue, elPlusOneValue;
    if (sum == (unsigned long)rankInt)
    {
        elValue = std::prev(it)->first;
        elPlusOneValue = it->first;
    }
    else
    {
        elValue = std::prev(it)->first;
        elPlusOneValue = std::prev(it)->first;
    }
    return elValue + rankFrac * (elPlusOneValue - elValue);
}
