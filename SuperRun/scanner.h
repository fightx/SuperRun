#pragma once

#include "util.h"
#include "element.h"

#include <muiload.h>
#pragma comment(lib, "muiload.lib")

#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>


//递归遍历指定层数的文件夹
bool ScanDirectory(super_run_list *list, const std::wstring& fullpath, int level, const HANDLE exit_event)
{
	boost::filesystem::wpath directory(fullpath);

	// C: 这样的路径是非法路径
	boost::system::error_code ec;
	if (fullpath.length()<3 || !boost::filesystem::is_directory(directory, ec))
	{
		return false;
	}

	if (level>0)
	{
		special_name_map special_name;
		GetSpecialName(directory, special_name);

		boost::system::error_code ec;
		boost::filesystem::directory_iterator dir(directory, ec), end;
		for (; dir != end; ++dir)
		{
			//检查退出事件，停止遍历文件夹
			if (::WaitForSingleObject(exit_event, 0) == WAIT_OBJECT_0) return true;

			//这是一个文件，加入列表
			if (boost::filesystem::is_regular_file(dir->path(), ec))
			{
				list->append(dir->path(), special_name);
			}

			//如果是文件夹则递归
			if (boost::filesystem::is_directory(dir->path(), ec))
			{
				ScanDirectory(list, dir->path().wstring(), level - 1, exit_event);
			}
		}
	}

	return true;
}
