/*������� ����� �������� �� ����������� ����������:
��� ����� ��������, � ��������� ��� ��, ��� �����, � ��� �������)
������� ������� ���� ����, ��� ��� ������� ������� ����������� ����� �������� �� ���� ��������.
������, �������, �����������, �� �� ����� ��� ��������. � ������� ����������������� ������� ��� �������
� ���� ����� ������ �� 50 ������. � ������ ���������� ���������� - �� ������ ������.
��������� ��� ��������, ���������� ������ ���������� ������� ����������� ��� ��� ���� ��� �������
������������ ��������� ��������. �� �, ���� ������, ���� ��� �� ����� � ���� ����������� ������.
��������� ������ ������ �� ����������, ����������� � ������.*/

#include <iostream>
#include <io.h>
#include <fcntl.h>
#include <vector>
#include <algorithm>

// 2 ������� ��� ������� �����������. ������ ������� �� ��������� ���������.
// ����� ������, ��� ��� ��������, ������������ � �������
double lerp(double t, double v0, double v1)
{
	return (1 - t)*v0 + t*v1;
}

double percentile(const std::vector<int> &v, double q)
{
	double point = lerp(q, -0.5, v.size() - 0.5);
	int left = std::max(unsigned int(std::floor(point)), unsigned int(0));
	int right = std::min(unsigned int(std::ceil(point)), v.size() - 1);

	int dataLeft = v.at(left);
	int dataRight = v.at(right);

	return lerp(point - left, dataLeft, dataRight);
}

int main()
{
	int fd;

	if ((fd = open("input_file.txt", O_RDONLY)) == -1)
	{
		std::cout << "Error";
		exit(1); // ������, �� ������ ������, ��� �������������� ������
	}

	char buffer[10]; // 10 - �����, ��� ����������
	int bytes_read;
	char t;
	int tabs = 0;
	int i = 0;

	std::vector<int> data;
	data.reserve(1000000);

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
				data.push_back(atoi(buffer)); // ����� ��� ���� ����� ���������
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
		std::cout << "Error";
		exit(1);
	};

	std::sort(data.begin(), data.end()); // �.�. ����������� ����� ���������, ����� ����� 1 ��� �� �������������

	// ��� ������ ���� �� �������. � ��� ����� ��� ��� ������� ���� ������, ����� ����� ����� ���� ����
	std::cout << "min=" << data.at(0) << " 50%=" << percentile(data, 0.5) << " 90%=" << percentile(data, 0.9) << " 99%=" << percentile(data, 0.99) << " 99.9%=" << percentile(data, 0.999) << "\n";

//	system("PAUSE");
}