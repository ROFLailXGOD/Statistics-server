/*Немного общих ощущений от полученного результата:
оно вроде работает, а насколько это то, что нужно, я без понятия)
Немного смущает факт того, что для точного расчёта перцентелей нужно работать со всей выборкой.
Вектор, конечно, справляется, но всё равно это небыстро. С заранее зарезервированной памятью под миллион
у меня ответ выдало за 50 секунд. В случае отсутствия резервации - на минуту больше.
Насколько мне известно, существуют методы примерного расчёта перцентелей как раз таки для случаев
постепенного получения объектов. Но я, если честно, пока что не копал в этом направлении вообще.
Результат работы совпал со значениями, полученными в Экселе.*/

#include <iostream>
#include <io.h>
#include <fcntl.h>
#include <vector>
#include <algorithm>

// 2 функции для расчёта перцентилей. Честно найдено на просторах Интернета.
// Автор заявил, что это алгоритм, используемый в МатЛабе
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
		exit(1); // вообще, не совсем уверен, как обрабатываются ошибки
	}

	char buffer[10]; // 10 - более, чем достаточно
	int bytes_read;
	char t;
	int tabs = 0;
	int i = 0;

	std::vector<int> data;
	data.reserve(1000000);

	while ((bytes_read = read(fd, &t, 1)) > 0) // читаю по байту
	{
		if (t == '\t') // считаю табуляции, чтобы дойти до нужной колонки
		{
			++tabs;
			continue;
		}
		if (t == '\n') // конец строки
		{
			buffer[i] = '\0'; // для того, чтобы использовать atoi()
			int val = atoi(buffer);
			if (val) // если мы получили 0, то это явно не время обработки транзакции. Это значение нам не нужно
				data.push_back(atoi(buffer)); // иначе это наше время обработки
			tabs = 0;
			i = 0;
			continue;
		}
		if (tabs == 15) // перед нужной колонкой ровно 15 табуляций
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

	std::sort(data.begin(), data.end()); // т.к. перцентилей нужно несколько, проще сразу 1 раз всё отсортировать

	// имя ивента пока не выводил. Я так понял там для каждого вида ивента, нужно будет вести свой учёт
	std::cout << "min=" << data.at(0) << " 50%=" << percentile(data, 0.5) << " 90%=" << percentile(data, 0.9) << " 99%=" << percentile(data, 0.99) << " 99.9%=" << percentile(data, 0.999) << "\n";

//	system("PAUSE");
}