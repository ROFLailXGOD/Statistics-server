#include <iostream>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <set>
#include <algorithm>
#include <cmath>

const int buff_size = 4096;

double lerp(double t, double v0, double v1)
{
	return (1 - t)*v0 + t*v1;
}

double percentile(const std::multiset<int> &s, double q)
{
	double point = lerp(q, -0.5, s.size() - 0.5);
    int left = std::max(int(std::floor(point)), 0);
    int right = std::min(int(std::ceil(point)), int(s.size() - 1));

	int dataLeft = *std::next(s.begin(), left);
	int dataRight = *std::next(s.begin(), right);

	return lerp(point - left, dataLeft, dataRight);
}

int main()
{
	int fd;

	if ((fd = open("input_file.txt", O_RDONLY)) == -1)
	{
		std::cerr << "File opening error: " << strerror(errno) << "\n";
                exit(1);
	}

    char numbuffer[10];
    ssize_t bytes_read;
    char buffer[buff_size];
	int tabs = 0;
	int i = 0;

	std::multiset<int> data;

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
                    data.insert(atoi(numbuffer));
                tabs = 0;
                i = 0;
                continue;
            }
            if (tabs == 15)
            {
                numbuffer[i] = buffer[j];
                ++i;
            }
        }
	}

	if (close(fd) < 0)
	{
		std::cerr << "File closing error: " << strerror(errno) << "\n";
		exit(1);
	};

	std::cout << "min=" << *(data.begin()) << " 50%=" << percentile(data, 0.5) << " 90%=" << percentile(data, 0.9) << " 99%=" << percentile(data, 0.99) << " 99.9%=" << percentile(data, 0.999) << "\n";

//	data.clear();

//	system("PAUSE");
}
