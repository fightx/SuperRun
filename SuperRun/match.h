#pragma once

#include <string>

#include <boost/algorithm/string.hpp>
std::wstring remove_spaces(std::wstring const& search)
{
	std::wstring ret = L"";
	for (auto const& ch : search)
	{
		if (ch != ' ')
			ret += ch;
	}
	return ret;
}

//返回一个匹配近似度0~1，0为完全不匹配
//search应当转换为小写
double ranker(const std::wstring &search, const std::wstring &text)
{
	if (search.empty())
		return 1;

	std::wstring::size_type n = 0, m = 0;
	int difference = 0;

	while (n < search.size() && m < text.size())
	{
		if (search[n] == text[m] || toupper(search[n]) == text[m])
		{
			n++;
		}
		else
		{
			difference++;
		}

		m++;
	}

	if(n==search.size())
	{

		 return 1.0 - 1.0 * difference / m;
	}

	return 0;
}