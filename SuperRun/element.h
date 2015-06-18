#pragma once

#include "util.h"

#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/thread/mutex.hpp>

#include "serialize.h"


#ifdef _MSC_VER
boost::filesystem::path p("dummy");
#endif

#include "pinyin.h"

const std::wstring Converte2Pinyin(const std::wstring &str)
{
	std::wstring res = L"";
	for (auto const& ch : str)
	{
		if (ch >= 0x4E00 && ch <= 0x9FC3)
		{
			res += pinyin_table[ch - 0x4E00];
		}
	}
	return res;
}

//搜索目录
struct scanner_directory
{
	std::wstring path;
	int level;
};

//核心数据
struct super_run
{
	std::wstring name;
	std::wstring description; //用于搜索
	std::wstring fullpath; //真实路径，用于打开操作
	std::wstring realpath; //指向路径，用于判断重复快捷方式
	uint32_t count;			// 执行次数
};

struct rank_list
{
	std::wstring name;
	std::wstring command;
	double rank;
};

class super_run_list
{
public:
	super_run_list(){
		has_change = false;
	}
	void append(const boost::filesystem::path &path, special_name_map &special_name)
	{
		if (!boost::iends_with(path.wstring(), L".lnk") && !boost::iends_with(path.wstring(), L".exe"))
		{
			return;
		}

		{
			boost::mutex::scoped_lock lock(mt2);
			if (exits_files.count(path.wstring()))
			{
				return;
			}
		}
		super_run run;
		run.count = 0;
		run.description = path.stem().wstring();

		if (special_name.count(path.filename().wstring()))
		{
			run.name = special_name[path.filename().wstring()];
			run.description = run.name + run.description;
		}
		else
		{
			run.name = path.stem().wstring();
		}

		run.fullpath = path.wstring();
		run.realpath = L"";

		if (boost::iends_with(run.fullpath, L".lnk"))
		{
			wchar_t szLinkFileExePath[MAX_PATH] = { 0 };
			ResolveIt(path.wstring().c_str(), szLinkFileExePath, MAX_PATH);
			if (szLinkFileExePath[0])
			{
				run.realpath = ExpandEnvironmentPath(szLinkFileExePath);
				boost::filesystem::path realpath(run.realpath);
				run.description += realpath.stem().wstring();
			}
		}

		if (boost::iends_with(run.fullpath, L".exe"))
		{
			wchar_t szExeFileDescription[MAX_PATH] = { 0 };
			if (GetVersionString(path.wstring().c_str(), L"FileDescription", szExeFileDescription, MAX_PATH))
			{
				if (szExeFileDescription[0])
				{
					std::wstring description = szExeFileDescription;
					description = boost::algorithm::trim_copy(description);
					if (description.length())
					{
						run.name = szExeFileDescription;
						run.description += szExeFileDescription;
					}
				}
			}
		}

		run.description += Converte2Pinyin(run.name);

		add(run);
	}
	void rename_path(const std::wstring &old_path, const std::wstring &new_path)
	{
		boost::mutex::scoped_lock lock(mt);
		for (auto &run : super_runs)
		{
			size_t start_pos = run.fullpath.find(old_path);
			if (start_pos != std::wstring::npos)
			{
				boost::mutex::scoped_lock lock(mt2);
				exits_files.erase(run.fullpath);

				run.fullpath.replace(start_pos, old_path.length(), new_path);

				exits_files[run.fullpath] = true;
				has_change = true;
			}
		}
	}
	void remove_file(const std::wstring &path)
	{
		boost::mutex::scoped_lock lock(mt2);
		if (exits_files.count(path))
		{
			exits_files.erase(path);

			boost::mutex::scoped_lock lock(mt);
			
			super_runs.remove_if([&path](const super_run & run) {
				return run.fullpath == path;
			});

			has_change = true;
		}
	}

	void append_file(const boost::filesystem::path &path)
	{
		boost::system::error_code ec;
		if (boost::filesystem::is_regular_file(path, ec))
		{
			special_name_map special_name;
			GetSpecialName(path.parent_path(), special_name);

			append(path, special_name);
		}
	}

	const std::list<super_run>& get()
	{
		boost::mutex::scoped_lock lock(mt);

		super_runs.sort([](const super_run & a, const super_run & b) {
			return StrCmpLogicalW(a.name.c_str(), b.name.c_str())<0;
		});
		super_runs.unique([](const super_run & a, const super_run & b) {
			return a.name == b.name && a.realpath == b.realpath;
		});

		has_change = false;

		return super_runs;
	}


	const uint32_t get_total()
	{
		uint32_t total = 100;
		boost::mutex::scoped_lock lock(mt);
		for (auto &run : super_runs)
		{
			total += run.count;
		}
		return total;
	}

	bool hasChanged()
	{
		boost::mutex::scoped_lock lock(mt);
		return has_change;
	}

	void AddCount(const wchar_t *path)
	{
		boost::mutex::scoped_lock lock(mt2);
		if (exits_files.count(path))
		{
			boost::mutex::scoped_lock lock(mt);
			for (auto &run : super_runs)
			{
				if (run.fullpath == path)
				{
					run.count++;
					return;
				}
			}
		}
	}
	void Load(const wchar_t *path)
	{
		try{
			boost::property_tree::wptree config;

			LoadFromFile(config, path);

			boost::system::error_code ec;
			for (auto list : config.get_child(L"list"))
			{
				super_run run = {};

				size_t index = 0;
				for (auto item : list.second)
				{
					switch (index)
					{
					case 0:
						run.name = item.second.get_value<std::wstring>();
						break;
					case 1:
						run.description = item.second.get_value<std::wstring>();
						break;
					case 2:
						run.fullpath = item.second.get_value<std::wstring>();
						break;
					case 3:
						run.realpath = item.second.get_value<std::wstring>();
						break;
					case 4:
						run.count = item.second.get_value<uint32_t>();
						break;
					}
					index++;
				}

				if (boost::filesystem::is_regular_file(run.fullpath, ec))
				{
					add(run);
				}
			}
			
		}
		catch (...)
		{
		}
	}
	void Save(const wchar_t *path)
	{

		boost::property_tree::wptree config;
		for (auto &run : get())
		{

			boost::property_tree::wptree item;

			boost::property_tree::wptree child1, child2, child3, child4, child5;

			child1.put(L"", run.name);
			child2.put(L"", run.description);
			child3.put(L"", run.fullpath);
			child4.put(L"", run.realpath);
			child5.put(L"", run.count);

			item.push_back(std::make_pair(L"", child1));
			item.push_back(std::make_pair(L"", child2));
			item.push_back(std::make_pair(L"", child3));
			item.push_back(std::make_pair(L"", child4));
			item.push_back(std::make_pair(L"", child5));

			config.push_back(std::make_pair(L"", item));
		}

		boost::property_tree::wptree props;
		props.add_child(L"list", config);
		SaveToFile(props, path);
	}
private:
	void add(super_run run)
	{
		{
			boost::mutex::scoped_lock lock(mt2);
			exits_files[run.fullpath] = true;
		}

		boost::mutex::scoped_lock lock(mt);
		super_runs.push_back(run);

		has_change = true;

		// std::wcout << run.name << '\t' << run.fullpath << std::endl;
	}
private:
	std::list<super_run> super_runs;
	std::map<std::wstring, bool> exits_files;
	boost::mutex mt;
	boost::mutex mt2;
	bool has_change;
};
