
// VTable Hook

#pragma once

#include <Windows.h>
#include <string.h>

//-----------------------------------------------------------------------------

#define VTABLE_HOOK(name) CVTableHook name
#define REMOVE_VTABLE_HOOK(name) name.Remove()

#define HOOK_VTABLE(name, baseClass, functions) name.Init(baseClass, functions)
#define HOOK_VTABLE_AUTOGUESS(name, baseClass) name.Init(baseClass)

#define GET_VTABLE(name) name.GetVTable()
#define GET_VTABLE_FUNC(name, index) name.GetFunction(index)

#define HOOK_VTABLE_FUNC(name, hookFunction, index, originalFunction, funcType) originalFunction = (funcType)name.GetFunction(index); name.HookFunction(hookFunction, index)
#define HOOK_VTABLE_FUNC_ONLY(name, hookFunction, index) name.HookFunction(hookFunction, index)

#define UNHOOK_VTABLE_FUNC(name, index) name.UnhookFunction(index)

//-----------------------------------------------------------------------------

class CVTableHook
{
public:
	CVTableHook();
	~CVTableHook();

	void Init(void *pBaseClass, const int nFunctions);

	void Init(void *pBaseClass);

	void Remove();

	void *GetVTable() const;

	void *GetFunction(const int nIndex) const;

	bool HookFunction(void *pFunction, const int nIndex);

	bool UnhookFunction(const int nIndex);

	int TotalFunctions() const;

public:
	static void *HookFunction(void *pBaseClass, void *pHookFunction, const int nIndex);

	static void UnhookFunction(void *pBaseClass, void *pOriginalFunction, const int nIndex);

private:
	DWORD **m_pBaseClass;

	DWORD *m_pVTable;
	DWORD *m_pVTableOriginal;

	int m_nFunctions;
};