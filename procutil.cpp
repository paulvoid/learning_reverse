#include "procutil.h"

#include <TlHelp32.h>
#include <filesystem>
#include <iostream>

#include "sigscan.h"

#define READ_LIMIT (1024 * 1024 * 2) // 2 MB

std::vector<HANDLE> ProcUtil::GetProcessesByImageName(const char *image_name, size_t limit, DWORD access)
{
	std::vector<HANDLE> result;

	PROCESSENTRY32 entry;
	entry.dwSize = sizeof(PROCESSENTRY32);

	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
	size_t count = 0;

	if (Process32First(snapshot, &entry) == TRUE)
	{
		while (count < limit && Process32Next(snapshot, &entry) == TRUE)
		{
			if (_stricmp(entry.szExeFile, image_name) == 0)
			{
				if (HANDLE process = OpenProcess(access, FALSE, entry.th32ProcessID))
				{
					result.push_back(process);
					count++;
				}
			}
		}
	}

	CloseHandle(snapshot);
	return result;
}

HANDLE ProcUtil::GetProcessByImageName(const char* image_name)
{
	auto processes = GetProcessesByImageName(image_name, 1);
	return processes.size() > 0 ? processes[0] : NULL;
}

std::vector<HMODULE> ProcUtil::GetProcessModules(HANDLE process)
{
	std::vector<HMODULE> result;

	DWORD last = 0;
	DWORD needed;

	while (true)
	{
		if (!EnumProcessModulesEx(process, result.data(), last, &needed, LIST_MODULES_ALL))
			throw WindowsException("unable to enum modules");

		result.resize(needed / sizeof(HMODULE));
		if (needed <= last)	return result;
		last = needed;
	}
}

ProcUtil::ModuleInfo ProcUtil::GetModuleInfo(HANDLE process, HMODULE module)
{
	ModuleInfo result;

	if (module == NULL)
	{
		/*
			GetModuleInformation works with hModule set to NULL with the caveat that lpBaseOfDll will be NULL aswell: https://doxygen.reactos.org/de/d86/dll_2win32_2psapi_2psapi_8c_source.html#l01102
			Solutions: 
				1) Enumerate modules in the process and compare file names
				2) Use NtQueryInformationProcess with ProcessBasicInformation to find the base address (as done here: https://doxygen.reactos.org/de/d86/dll_2win32_2psapi_2psapi_8c_source.html#l00142)
		*/

		char buffer[MAX_PATH];
		DWORD size = sizeof(buffer);

		if (!QueryFullProcessImageName(process, 0, buffer, &size)) // Requires at least PROCESS_QUERY_LIMITED_INFORMATION 
			throw WindowsException("unable to query process image name");

		bool found;

		printf("[ProcUtil] QueryFullProcessImageName(%p) returned %s\n", process, buffer);

		try
		{
			found = FindModuleInfo(process, buffer, result);
		}
		catch (WindowsException& e)
		{
			printf("[ProcUtil] GetModuleInfo(%p, NULL) failed: %s (%X)\n", process, e.what(), e.GetLastError());
			found = false;
		}

		if (!found) // Couldn't enum modules or GetModuleFileNameEx/GetModuleInformation failed
		{
			result.path = buffer;
			result.base = nullptr;
			result.size = 0;
			result.entry_point = nullptr;
		}
	}
	else
	{
		char buffer[MAX_PATH];
		if (!GetModuleFileNameEx(process, module, buffer, sizeof(buffer))) // Requires PROCESS_QUERY_INFORMATION | PROCESS_VM_READ 
			throw WindowsException("unable to get module file name");

		MODULEINFO mi;
		if (!GetModuleInformation(process, module, &mi, sizeof(mi))) // Requires PROCESS_QUERY_INFORMATION | PROCESS_VM_READ 
			throw WindowsException("unable to get module information");

		result.path = buffer;
		result.base = mi.lpBaseOfDll;
		result.size = mi.SizeOfImage;
		result.entry_point = mi.EntryPoint;
	}

	return result;
}

bool ProcUtil::FindModuleInfo(HANDLE process, const std::filesystem::path& path, ModuleInfo& out)
{
	printf("[ProcUtil] FindModuleInfo(%p, %s)\n", process, path.string().c_str());

	for (auto module : GetProcessModules(process))
	{
		try
		{
			ModuleInfo info = GetModuleInfo(process, module);

			printf("\tbase=%p, size=%zu, path=%s\n", info.base, info.size, info.path.string().c_str());

			if (std::filesystem::equivalent(info.path, path))
			{
				out = info;
				return true;
			}
		}
		catch (std::filesystem::filesystem_error& e)
		{
		}
	}

	return false;
}

void *ScanRegion(HANDLE process, const char *aob, const char *mask, const uint8_t *base, size_t size)
{
	std::vector<uint8_t> buffer;
	buffer.resize(READ_LIMIT);

	size_t aob_len = strlen(mask);

	while (size >= aob_len)
	{
		size_t bytes_read = 0;

		if (ReadProcessMemory(process, base, buffer.data(), size < buffer.size() ? size : buffer.size(), (SIZE_T *)&bytes_read) && bytes_read >= aob_len)
		{
			if (uint8_t *result = sigscan::scan(aob, mask, (uintptr_t)buffer.data(), (uintptr_t)buffer.data() + bytes_read))
			{
				return (uint8_t *)base + (result - buffer.data());
			}
		}
	   
		if (bytes_read > aob_len) bytes_read -= aob_len;

		size -= bytes_read;
		base += bytes_read;
	}

	return false;
}


void *ProcUtil::ScanProcess(HANDLE process, const char *aob, const char *mask, const uint8_t *start, const uint8_t *end)
{
	auto i = start;

	while (i < end)
	{
		MEMORY_BASIC_INFORMATION mbi;
		if (!VirtualQueryEx(process, i, &mbi, sizeof(mbi)))
		{
			return nullptr;
		}

		size_t size = mbi.RegionSize - (i - (const uint8_t *)mbi.BaseAddress);
		if (i + size >= end) size = end - i;

		if (mbi.State & MEM_COMMIT && mbi.Protect & PAGE_READABLE && !(mbi.Protect & PAGE_GUARD))
		{
			if (void *result = ScanRegion(process, aob, mask, i, size))
			{
				return result;
			}
		}

		i += size;
	}

	return nullptr;
}

bool ProcUtil::IsOS64Bit()
{
#ifdef _WIN64
	return true;
#else
	SYSTEM_INFO info;
	GetNativeSystemInfo(&info);

	return info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 ||
		info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM64; // lol arm
#endif
}

bool ProcUtil::IsProcess64Bit(HANDLE process)
{
	if (IsOS64Bit())
	{
		BOOL result;
		if (!IsWow64Process(process, &result))
			throw WindowsException("unable to check process wow64");

		return result == 0;
	}
	else
	{
		return false;
	}
}
std::uintptr_t ProcUtil::Scan(HANDLE process, std::uintptr_t BaseAddress, std::uintptr_t VFTableAddress) {
    SYSTEM_INFO systemInfo;
    std::uintptr_t pageSize;
    std::uintptr_t pageSize4ByteSplit;
    MEMORY_BASIC_INFORMATION memoryInfo;
    GetSystemInfo(&systemInfo);
    pageSize = systemInfo.dwPageSize;
    pageSize4ByteSplit = pageSize / 4;
    DWORD* buffer = new DWORD[pageSize];
    std::uintptr_t addr = BaseAddress;


    for (DWORD addr = BaseAddress; addr < 0x7FFFFFFF; addr += pageSize) {
        VirtualQueryEx(process, (LPCVOID)addr, &memoryInfo, pageSize);
        if (memoryInfo.Protect == PAGE_READWRITE) {
            ReadProcessMemory(process, (LPCVOID)addr, buffer, pageSize, 0);
            for (DWORD i = 0; i <= pageSize4ByteSplit; i++) {
                if (buffer[i] == VFTableAddress) {
                    delete[] buffer;
                    return (DWORD)(addr + (i * 4));
                }
            }
        }
    }

    std::cout << "Failed to find VFTable" << std::endl;

    delete[] buffer;
    return 0;
}
DWORD ProcUtil::GetPointerAddress(HANDLE process,DWORD address) {
    uintptr_t pointerAddress = GetDMAAddress(process,address, { 0x0 });
    return pointerAddress;
}

uintptr_t ProcUtil::GetDMAAddress(HANDLE process,uintptr_t ptr, std::vector<unsigned int> offsets) {
    uintptr_t addr = ptr;
    for (unsigned int i = 0; i < offsets.size(); ++i) {
        addr = Read<uintptr_t>(process,(LPCVOID)addr);
        //ReadProcessMemory(handle, (BYTE*)addr, &addr, sizeof(addr), 0);
        addr += offsets[i];
    }
    return addr;
}

std::vector<DWORD> ProcUtil::GetChildren(HANDLE process,DWORD instance) {
    std::vector<DWORD> children = {};

    DWORD v4 = GetPointerAddress(process,instance + 0x30);


    //std::cout << "v4: " << v4 << std::endl;
    DWORD childBegin = GetPointerAddress(process,v4 );
    //std::cout << "childBegin: " << childBegin << std::endl;
    DWORD childEnd = GetPointerAddress(process,v4 + 0x4);
    //std::cout << "childEnd: " << childEnd << std::endl;

    while (childBegin != childEnd) {
        DWORD child = GetPointerAddress(process,childBegin);
        //std::cout << "child: " << child << std::endl;
        //std::cout << "child class: " << GetClassType(process,child) << std::endl;
        children.push_back(child);
        childBegin += 0x8;
    }




    return children;
}

DWORD ProcUtil::GetService(HANDLE process,DWORD game, std::string className) {
    std::vector<DWORD> children = GetChildren(process,game);



    for (DWORD child : children) {
        if (GetClassType(process,child) == className) {
            return child;
        }
    }
}
std::string ProcUtil::GetClassType(HANDLE process,DWORD instance) {
    std::string className;

    DWORD classDescriptor = GetPointerAddress(process,instance + 0xC);
    className = ReadStringOfUnknownLength(process,GetPointerAddress(process,classDescriptor + 0x4));

    return className;
}

std::string ProcUtil::ReadStringOfUnknownLength(HANDLE process,DWORD address) {
    std::string string;
    char character = 0;
    int charSize = sizeof(character);
    int offset = 0;

    while (true) {
        character = Read<char>(process,(LPCVOID)(address + offset));
        if (character == 0) break;
        offset += charSize;

        string.push_back(character);
    }
    //std::cout << string << std::endl;

    return string;
}
// get name
std::string ProcUtil::GetName(HANDLE process,DWORD instance) {
    instance += 0x2C;
    uintptr_t nameAddress = GetPointerAddress(process,instance);
    std::string name = ReadStringOfUnknownLength(process,nameAddress);
    int size = Read<int>(process,(LPCVOID)(nameAddress + 0x10));


    if (size >= 16u) {
        uintptr_t newNameAddress = GetPointerAddress(process,nameAddress);
        return ReadStringOfUnknownLength(process,newNameAddress);
    } else {
        return name;
    }
}
// findfirstchild
DWORD ProcUtil::FindFirstChild(HANDLE process,DWORD instance, std::string name) {
    std::vector<DWORD> children = GetChildren(process,instance);

    for (DWORD child : children) {
        if (GetName(process,child) == name) {
            return child;
        }
    }
}
