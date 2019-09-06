/* Multi-platform GUI SDL module for the BeebEm emulator
  Written by Eelco Huininga 2016
*/

namespace gui {
	int	MsgBox (HWND hwnd, const char *message_p, const char *title_p, int type);
};
int MessageBox(HWND hwnd, const char *message_p, const char *title_p, int type)
void SetWindowText(HWND hwnd, const char *title_p)
void Sleep(DWORD ticks)
DWORD GetTickCount(void)
BOOL ModifyMenu(HMENU hMnu, UINT uPosition, UINT uFlags, PTR uIDNewItem, LPCTSTR lpNewItem)
UINT GetMenuState(HMENU hMenu, UINT uId, UINT uFlags)
DWORD CheckMenuItem(HMENU hmenu, UINT uIDCheckItem, UINT uCheck)
BOOL MoveFileEx(LPCTSTR lpExistingFileName, LPCTSTR lpNewFileName, DWORD dwFlags)
BOOL EnableMenuItem(HMENU hMenu,UINT uIDEnableItem,UINT uEnable)
UINT GetPrivateProfileInt(LPCTSTR lpAppName, LPCTSTR lpKeyName, INT nDefault, LPCTSTR lpFileName)
DWORD GetPrivateProfileString(LPCTSTR lpAppName, LPCTSTR lpKeyName, LPCTSTR lpDefault, LPTSTR lpReturnedString, DWORD nSize, LPCTSTR lpFileName)
