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
#include <vector>
#include <iterator>
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
static bool sh = false;
static std::map< std::string, std::map<int, unsigned long> > data;

static char *input;
static char *fifo;
static char *output;

void udpconn();
void readfd(int fd, int fifoflag);
double percentile(std::string ev, double q);
double arithmetic_mean(std::string ev);
double median(std::string ev);
double standard_deviation(std::string ev, int n = 1);
double variance(std::string ev);
double range(std::string ev);
double skewness(std::string ev);
double kurtosis(std::string ev);

std::string center(const std::string s, const int w)
{
    std::stringstream ss, spaces;
    int padding = w - s.size();                 // count excess room to pad
    for(int i=0; i<padding/2; ++i)
        spaces << " ";
    ss << spaces.str() << s << spaces.str();    // format with padding
    if(padding>0 && padding%2!=0)               // if odd #, add 1 space
        ss << " ";
    return ss.str();
}

std::string prd(const double x, const int decDigits, const int width) {
    std::stringstream ss;
    ss << std::fixed << std::right;
    ss.fill(' ');        // fill space around displayed #
    ss.width(width);     // set  width around displayed #
    ss.precision(decDigits); // set # places after decimal
    ss << x;
    return ss.str();
}

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
            if (sh == false)
            {
                std::cout << "Arithmetic mean is " << arithmetic_mean(ev) << "\n";
                std::cout << "Median is " << median(ev) << "\n";
                std::cout << "Standard deviation is " << standard_deviation(ev) << "\n";
                std::cout << "Variance is " << variance(ev) << "\n";
                std::cout << "Range is " << range(ev) << "\n";
                std::cout << "Skewness is " << skewness(ev) << "\n";
                std::cout << "Kurtosis is " << kurtosis(ev) << "\n";
            }

            std::cout << "\n";

            std::cout << center("ExecTime", 10) << " | " << center("TransNo", 10) << " | " << center("Weight,%", 10) << " | " << center("Percent", 10) << "\n";
            std::cout << std::string(10*4 + 3*3, '-') << "\n";

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
                    std::cout << prd(i, 0, 10) << " | " << prd(s, 0, 10) << " | " << prd(s*100.0/size, 4, 10) << " | " << prd(p*100.0/size, 4, 10) << "\n";
                    s = 0;
                }
            }
            if (i % 5) //still have some data
            {
                while (i % 5)
                    ++i;
                p += s;
                std::cout << prd(i, 0, 10) << " | " << prd(s, 0, 10) << " | " << prd(s*100.0/size, 4, 10) << " | " << prd(p*100.0/size, 4, 10) << "\n";
            }
            std::cout << "\n";
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
    while ((opt = getopt(argc, argv, "i:f:o:s")) != -1)
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
        case 's':
            sh = true;
            break;
        default:
            std::cerr << "Usage: " << argv[0] << " [-i] input_file [-f] fifo [-o] output_file [-s]\n";
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
            std::string input(buff, bytes-1);
            std::istringstream iss(input);
            std::vector<std::string> events(std::istream_iterator<std::string>{iss},
                                             std::istream_iterator<std::string>());
            for (unsigned long i = 0; i < events.size(); ++i)
            {
                if (data.find(events[i]) == data.end())
                {
                    msg += "Event " + events[i] + " is not found. Available events:\n";
                    if (data.size())
                    {
                        std::lock_guard<std::recursive_mutex> lock(w_lock);
                        for (const auto& pair: data)
                        {
                            msg += " • " + pair.first + "\n";
                        }
                    }
                    else
                    {
                        msg += "no events are available\n";
                    }
                }
                else
                {
                    std::lock_guard<std::recursive_mutex> lock(w_lock);
                    msg += events[i] + " min=" + to_string(data[events[i]].begin()->first) +
                                   " 50%=" + to_string(percentile(events[i], 0.5)) +
                                   " 90%=" + to_string(percentile(events[i], 0.9)) +
                                   " 99%=" + to_string(percentile(events[i], 0.99)) +
                                   " 99.9%=" + to_string(percentile(events[i], 0.999)) + "\n";
                    if (sh == false)
                    {
                        msg += "Arithmetic mean is " + to_string(arithmetic_mean(events[i])) + "\n";
                        msg += "Median is " + to_string(median(events[i])) + "\n";
                        msg += "Standard deviation is " + to_string(standard_deviation(events[i])) + "\n";
                        msg += "Variance is " + to_string(variance(events[i])) + "\n";
                        msg += "Range is " + to_string(range(events[i])) + "\n";
                        msg += "Skewness is " + to_string(skewness(events[i])) + "\n";
                        msg += "Kurtosis is " + to_string(kurtosis(events[i])) + "\n";
                    }
                    msg += "\n";
                }
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

double arithmetic_mean(std::string ev)
{
    const unsigned long size = std::accumulate(std::begin(data[ev]), std::end(data[ev]), (unsigned long)0,
                                               [](const unsigned long previous, const auto& element)
                                               { return previous + element.second; });
    int s = 0;
    std::map<int, unsigned long>::iterator it;
    for (it = data[ev].begin(); it != data[ev].end(); ++it)
    {
        s += it->first*it->second;
    }
    return s*1./size;
}

double median(std::string ev)
{
    const unsigned long size = std::accumulate(std::begin(data[ev]), std::end(data[ev]), (unsigned long)0,
                                               [](const unsigned long previous, const auto& element)
                                               { return previous + element.second; });
    int rank = size / 2;
    int sum = 0;
    std::map<int, unsigned long>::iterator it;
    for (it = data[ev].begin(); sum < (unsigned long)rank; ++it)
    {
        sum += it->second;
    }

    if (rank == sum)
    {
        if (size % 2)
        {
            return it->first;
        }
        else
        {
            return (std::prev(it)->first + it->first)/2.;
        }
    }
    else
    {
        return std::prev(it)->first;
    }
}

double standard_deviation(std::string ev, int n)
{
    double mean = arithmetic_mean(ev);
    double sum = 0;
    std::map<int, unsigned long>::iterator it;
    for (it = data[ev].begin(); it != data[ev].end(); ++it)
    {
        sum += (it->first-mean)*(it->first-mean)*it->second;
    }
    const unsigned long size = std::accumulate(std::begin(data[ev]), std::end(data[ev]), (unsigned long)0,
                                               [](const unsigned long previous, const auto& element)
                                               { return previous + element.second; });
    sum /= size-n;
    return sqrt(sum);
}

double variance(std::string ev)
{
    return standard_deviation(ev)*standard_deviation(ev);
}

double range(std::string ev)
{
    return std::prev(data[ev].end())->first - data[ev].begin()->first;
}

double skewness(std::string ev)
{
    double mean = arithmetic_mean(ev);
    double sum = 0;
    std::map<int, unsigned long>::iterator it;
    for (it = data[ev].begin(); it != data[ev].end(); ++it)
    {
        sum += (it->first-mean)*(it->first-mean)*(it->first-mean)*it->second;
    }
    const unsigned long size = std::accumulate(std::begin(data[ev]), std::end(data[ev]), (unsigned long)0,
                                               [](const unsigned long previous, const auto& element)
                                               { return previous + element.second; });
    sum = sum*size/((size-1)*(size-2));
    return sum/(standard_deviation(ev)*standard_deviation(ev)*standard_deviation(ev));
}

double kurtosis(std::string ev)
{
    double mean = arithmetic_mean(ev);
    double sum = 0;
    std::map<int, unsigned long>::iterator it;
    for (it = data[ev].begin(); it != data[ev].end(); ++it)
    {
        sum += (it->first-mean)*(it->first-mean)*(it->first-mean)*(it->first-mean)*it->second;
    }
    const unsigned long size = std::accumulate(std::begin(data[ev]), std::end(data[ev]), (unsigned long)0,
                                               [](const unsigned long previous, const auto& element)
                                               { return previous + element.second; });
    sum = sum*size*(size+1)/((size-1)*(size-2)*(size-3));
    sum /= standard_deviation(ev)*standard_deviation(ev)*standard_deviation(ev)*standard_deviation(ev);
    return sum - 3.*(size-1)*(size-1)/((size-2)*(size-3));
}
