#pragma once

#include <codecvt>
#include <locale>

#include <boost/filesystem.hpp>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "util.h"

boost::filesystem::path GetDataPath(const wchar_t *path)
{
	boost::filesystem::wpath data_path = GetAppPath() / L"Data";

	boost::system::error_code ec;
	if (!boost::filesystem::is_directory(data_path, ec))
	{
		boost::filesystem::remove(data_path);
		boost::filesystem::create_directory(data_path, ec);
	}

	return data_path / path;
}

boost::filesystem::path GetLangPath(const wchar_t *path)
{
	return GetAppPath() / L"Lang" / path;
}

bool LoadFromFile(boost::property_tree::wptree &config, const wchar_t *path)
{
	try{
		std::wifstream file(path, std::ios_base::binary);

		file.imbue(std::locale(std::locale(), new std::codecvt_utf8_utf16<wchar_t>));

		boost::property_tree::json_parser::read_json(file, config);

		file.close();
		return true;
	}
	catch (...)
	{
		boost::filesystem::remove(path);
	}

	return false;
}

void SaveToFile(const boost::property_tree::wptree &config, const wchar_t *path)
{
	std::wofstream file(path, std::ios_base::binary);

	file.imbue(std::locale(std::locale(), new std::codecvt_utf8_utf16<wchar_t>));

	boost::property_tree::json_parser::write_json(file, config);
	file.close();
}