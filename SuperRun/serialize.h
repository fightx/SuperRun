#pragma once

#include <boost/filesystem.hpp>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

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

void LoadFromFile(boost::property_tree::wptree &config, const wchar_t *path)
{
	try{
		std::wifstream file(GetDataPath(path).c_str(), std::ios_base::binary);

		boost::property_tree::json_parser::read_json(file, config);

		file.close();
	}
	catch (...)
	{
		boost::filesystem::remove(GetDataPath(path).c_str());
	}
}

void SaveToFile(const boost::property_tree::wptree &config, const wchar_t *path)
{
	std::wofstream file(GetDataPath(path).c_str(), std::ios_base::binary);
	boost::property_tree::json_parser::write_json(file, config);
	file.close();
}