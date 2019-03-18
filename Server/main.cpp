/*������� ����� �������� �� ����������� ����������:
��� ����� ��������, � ��������� ��� ��, ��� �����, � ��� �������)
������� ������� ���� ����, ��� ��� ������� ������� ����������� ����� �������� �� ���� ��������.

������� ������ �� ��������� - ����� �������� ���������� ���� ������ 50-�� ������.

��������� ��� ��������, ���������� ������ ���������� ������� ����������� ��� ��� ���� ��� �������
������������ ��������� ��������. �� �, ���� ������, ���� ��� �� ����� � ���� ����������� ������.
��������� ������ ������ �� ����������, ����������� � ������.*/

#include <iostream>
#include <io.h>
#include <fcntl.h>
#include <set>
#include <algorithm>

// 2 ������� ��� ������� �����������. ������ ������� �� ��������� ���������.
// ����� ������, ��� ��� ��������, ������������ � �������
double lerp(double t, double v0, double v1)
{
	return (1 - t)*v0 + t*v1;
}

double percentile(const std::multiset<int> &s, double q)
{
	double point = lerp(q, -0.5, s.size() - 0.5);
	int left = std::max(unsigned int(std::floor(point)), unsigned int(0));
	int right = std::min(unsigned int(std::ceil(point)), unsigned int(s.size() - 1));

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
		exit(1); // ������, �� ������ ������, ��� �������������� ������
	}

	char buffer[10]; // 10 - �����, ��� ����������
	int bytes_read;
	char t;
	int tabs = 0;
	int i = 0;

	std::multiset<int> data;

	while ((bytes_read = read(fd, &t, 1)) > 0) // ����� �� �����
	{
		if (t == '\t') // ������ ���������, ����� ����� �� ������ �������
		{
			++tabs;
			continue;
		}
		if (t == '\n') // ����� ������
		{
			buffer[i] = '\0'; // ��� ����, ����� ������������ atoi()
			int val = atoi(buffer);
			if (val) // ���� �� �������� 0, �� ��� ���� �� ����� ��������� ����������. ��� �������� ��� �� �����
				data.insert(atoi(buffer)); // ����� ��� ���� ����� ���������
			tabs = 0;
			i = 0;
			continue;
		}
		if (tabs == 15) // ����� ������ �������� ����� 15 ���������
		{
			buffer[i] = t;
			++i;
		}
	}

	if (close(fd) < 0)
	{
		std::cerr << "File closing error: " << strerror(errno) << "\n";
		exit(1);
	};

	// ��� ������ ���� �� �������. � ��� ����� ��� ��� ������� ���� ������, ����� ����� ����� ���� ����
	std::cout << "min=" << *(data.begin()) << " 50%=" << percentile(data, 0.5) << " 90%=" << percentile(data, 0.9) << " 99%=" << percentile(data, 0.99) << " 99.9%=" << percentile(data, 0.999) << "\n";

//	data.clear();

//	system("PAUSE");
}