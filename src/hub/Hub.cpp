#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <commdlg.h>
#include <ctime>
#include <cwchar>
#include <dwmapi.h>
#include <filesystem>
#include <memory>
#include <shlobj.h>
#include <shobjidl.h>
#include <stdexcept>
#include <string>
#include <twodog/data/Json.hpp>
#include <twodog/data/ProjectCreation.hpp>
#include <twodog/data/RecentProjects.hpp>
#include <uxtheme.h>
#include <vector>
namespace {
constexpr int browseId = 101, createId = 102, openId = 103, recentId = 104,
              launchId = 105, removeId = 106;
constexpr COLORREF background = RGB(20, 24, 31), sidebar = RGB(25, 30, 39),
                   surface = RGB(32, 39, 50), selected = RGB(37, 57, 79),
                   accent = RGB(94, 167, 255), ink = RGB(230, 236, 245),
                   muted = RGB(153, 168, 187), border = RGB(48, 58, 73),
                   danger = RGB(245, 153, 151);
struct Hub {
  HWND name{}, location{}, status{}, list{}, open{}, remove{}, create{},
      browse{}, import{}, nameLabel{}, pathLabel{};
  HFONT font{}, title{}, heading{}, smallFont{};
  HBRUSH backgroundBrush{}, surfaceBrush{}, sidebarBrush{};
  float scale = 1;
  bool error = false;
  std::filesystem::path editor;
  std::unique_ptr<twodog::RecentProjects> recent;
  int px(int value) const {
    return static_cast<int>(static_cast<float>(value) * scale);
  }
};
std::wstring wide(const std::string &value) {
  const int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                    static_cast<int>(value.size()), nullptr, 0);
  std::wstring out(static_cast<std::size_t>(n), L' ');
  if (n)
    MultiByteToWideChar(CP_UTF8, 0, value.data(),
                        static_cast<int>(value.size()), out.data(), n);
  return out;
}
std::string utf(const std::wstring &value) {
  const int n = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                                    static_cast<int>(value.size()), nullptr, 0,
                                    nullptr, nullptr);
  if (!n && !value.empty())
    throw std::runtime_error("Invalid project name");
  std::string out(static_cast<std::size_t>(n), ' ');
  if (n)
    WideCharToMultiByte(CP_UTF8, 0, value.data(),
                        static_cast<int>(value.size()), out.data(), n, nullptr,
                        nullptr);
  return out;
}
std::wstring field(HWND control) {
  std::wstring text(static_cast<std::size_t>(GetWindowTextLengthW(control)) + 1,
                    L'\0');
  const int length =
      GetWindowTextW(control, text.data(), static_cast<int>(text.size()));
  text.resize(static_cast<std::size_t>(length));
  return text;
}
std::wstring quoted(const std::wstring &value) {
  std::wstring result = L"\"";
  std::size_t slashes = 0;
  for (wchar_t c : value) {
    if (c == L'\\') {
      ++slashes;
      continue;
    }
    result.append(c == L'"' ? slashes * 2 + 1 : slashes, L'\\');
    result += c;
    slashes = 0;
  }
  result.append(slashes * 2, L'\\');
  return result + L'"';
}

void rectangle(HDC dc, RECT r, COLORREF color) {
  HBRUSH brush = CreateSolidBrush(color);
  FillRect(dc, &r, brush);
  DeleteObject(brush);
}
void text(HDC dc, HFONT font, COLORREF color, const std::wstring &value, RECT r,
          UINT format = DT_LEFT | DT_SINGLELINE | DT_VCENTER |
                        DT_END_ELLIPSIS) {
  const auto previous = SelectObject(dc, font);
  SetTextColor(dc, color);
  SetBkMode(dc, TRANSPARENT);
  DrawTextW(dc, value.c_str(), static_cast<int>(value.size()), &r,
            format | DT_NOPREFIX);
  SelectObject(dc, previous);
}
void status(Hub &hub, const std::wstring &message, bool error = false) {
  hub.error = error;
  SetWindowTextW(hub.status, message.c_str());
  InvalidateRect(hub.status, nullptr, TRUE);
}
HWND control(HWND parent, Hub &hub, const wchar_t *type, const wchar_t *label,
             int id, DWORD extra) {
  const HWND child =
      CreateWindowExW(0, type, label, WS_CHILD | WS_VISIBLE | extra, 0, 0, 1, 1,
                      parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                      GetModuleHandleW(nullptr), nullptr);
  SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(hub.font), TRUE);
  SetWindowTheme(child, L"", L"");
  return child;
}
int selection(const Hub &hub) {
  return static_cast<int>(SendMessageW(hub.list, LB_GETCURSEL, 0, 0));
}
bool available(const std::filesystem::path &root) {
  std::error_code error;
  return std::filesystem::is_regular_file(root / "project.json", error);
}
void selectionChanged(Hub &hub) {
  const int index = selection(hub);
  const bool valid = index >= 0 && static_cast<std::size_t>(index) <
                                       hub.recent->entries().size();
  EnableWindow(hub.remove, valid);
  EnableWindow(
      hub.open,
      valid &&
          available(
              hub.recent->entries()[static_cast<std::size_t>(index)].root));
  if (valid &&
      !available(hub.recent->entries()[static_cast<std::size_t>(index)].root))
    status(hub,
           L"Percorso non disponibile. Usa Apri da disco per ritrovarlo, "
           L"oppure rimuovi la voce.",
           true);
  else
    status(hub,
           valid
               ? L"Progetto selezionato. Premi Apri progetto o fai doppio clic."
               : L"Crea un progetto oppure apri un project.json esistente.");
}
void refresh(HWND window, Hub &hub) {
  SendMessageW(hub.list, LB_RESETCONTENT, 0, 0);
  for (const auto &entry : hub.recent->entries())
    SendMessageW(hub.list, LB_ADDSTRING, 0,
                 reinterpret_cast<LPARAM>(wide(entry.name).c_str()));
  if (!hub.recent->entries().empty())
    SendMessageW(hub.list, LB_SETCURSEL, 0, 0);
  ShowWindow(hub.list, hub.recent->entries().empty() ? SW_HIDE : SW_SHOW);
  selectionChanged(hub);
  InvalidateRect(window, nullptr, TRUE);
}
void layout(HWND window, Hub &hub) {
  RECT r{};
  GetClientRect(window, &r);
  const int w = r.right, h = r.bottom;
  auto move = [&](HWND child, int x, int y, int width, int height) {
    MoveWindow(child, hub.px(x), hub.px(y), hub.px(width), hub.px(height),
               TRUE);
  };
  move(hub.nameLabel, 24, 211, 216, 22);
  move(hub.name, 24, 239, 216, 36);
  move(hub.pathLabel, 24, 291, 216, 22);
  move(hub.location, 24, 319, 216, 36);
  move(hub.browse, 24, 369, 216, 36);
  move(hub.create, 24, 419, 216, 42);
  MoveWindow(hub.import, w - hub.px(221), hub.px(42), hub.px(189), hub.px(40),
             TRUE);
  MoveWindow(hub.list, hub.px(296), hub.px(166),
             std::max(hub.px(200), w - hub.px(328)),
             std::max(hub.px(100), h - hub.px(300)), TRUE);
  MoveWindow(hub.remove, hub.px(296), h - hub.px(111), hub.px(178), hub.px(40),
             TRUE);
  MoveWindow(hub.open, w - hub.px(221), h - hub.px(111), hub.px(189),
             hub.px(40), TRUE);
  MoveWindow(hub.status, hub.px(296), h - hub.px(55),
             std::max(hub.px(200), w - hub.px(328)), hub.px(42), TRUE);
  InvalidateRect(window, nullptr, TRUE);
}
void launch(HWND window, Hub &hub, const std::filesystem::path &root) {
  if (!std::filesystem::is_regular_file(hub.editor))
    throw std::runtime_error(
        "Build the editor first: scripts/build-editor.ps1");
  const auto settings = twodog::Json::read(root / L"project.json");
  if (settings.at("version").integer() != 1)
    throw std::runtime_error("Unsupported project format");
  std::wstring command =
      quoted(hub.editor.wstring()) + L" --project " + quoted(root.wstring());
  STARTUPINFOW si{};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi{};
  if (!CreateProcessW(hub.editor.c_str(), command.data(), nullptr, nullptr,
                      FALSE, 0, nullptr, hub.editor.parent_path().c_str(), &si,
                      &pi))
    throw std::runtime_error("Could not start TwoDogEditor.exe");
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  try {
    hub.recent->remember(root);
    refresh(window, hub);
  } catch (const std::exception &e) {
    status(hub, L"Editor avviato; cronologia non salvata: " + wide(e.what()),
           true);
    return;
  }
  status(hub, L"Editor avviato: " +
                  wide(settings.text("name", utf(root.filename().wstring()))));
}
void browse(HWND window, Hub &hub) {
  IFileOpenDialog *dialog = nullptr;
  const auto created =
      CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                       IID_PPV_ARGS(&dialog));
  if (FAILED(created))
    throw std::runtime_error("Cannot open folder picker");
  DWORD flags = 0;
  dialog->GetOptions(&flags);
  dialog->SetOptions(flags | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM |
                     FOS_PATHMUSTEXIST);
  if (SUCCEEDED(dialog->Show(window))) {
    IShellItem *item = nullptr;
    if (SUCCEEDED(dialog->GetResult(&item))) {
      PWSTR path = nullptr;
      if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
        SetWindowTextW(hub.location, path);
        CoTaskMemFree(path);
      }
      item->Release();
    }
  }
  dialog->Release();
}

void paint(HWND window, Hub &hub) {
  PAINTSTRUCT paintData{};
  HDC target = BeginPaint(window, &paintData);
  RECT r{};
  GetClientRect(window, &r);
  HDC dc = CreateCompatibleDC(target);
  HBITMAP bitmap = CreateCompatibleBitmap(target, r.right, r.bottom);
  const auto old = SelectObject(dc, bitmap);
  rectangle(dc, r, background);
  rectangle(dc, {0, 0, hub.px(264), r.bottom}, sidebar);
  auto label = [&](HFONT font, COLORREF color, const std::wstring &s, int x,
                   int y, int w, int h) {
    text(dc, font, color, s,
         {hub.px(x), hub.px(y), hub.px(x + w), hub.px(y + h)});
  };
  label(hub.title, ink, L"2Dog", 24, 28, 215, 48);
  label(hub.font, muted, L"HUB / WORKSPACE", 26, 82, 214, 24);
  rectangle(dc, {hub.px(24), hub.px(136), hub.px(240), hub.px(137)}, border);
  label(hub.smallFont, accent, L"NUOVO PROGETTO", 24, 169, 216, 24);
  label(hub.smallFont, muted, L"Una scena vuota, pronta per te.", 24, 475, 220, 24);
  text(dc, hub.smallFont, muted, L"ENGINE 0.6  /  2D + C#",
       {hub.px(24), r.bottom - hub.px(57), hub.px(240), r.bottom - hub.px(23)});
  label(hub.title, ink, L"Progetti", 296, 30, 350, 49);
  label(hub.font, muted, L"Riprendi da dove avevi lasciato.", 296, 88, 480, 24);
  label(hub.smallFont, muted,
        L"RECENTI  /  " + std::to_wstring(hub.recent->entries().size()), 296,
        132, 400, 24);
  if (hub.recent->entries().empty()) {
    label(hub.heading, ink, L"Il tuo prossimo gioco inizia qui", 320, 222, 530,
          34);
    label(hub.font, muted,
          L"Crea un progetto a sinistra, oppure aprine uno esistente.", 320,
          266, 580, 30);
    label(hub.font, muted, L"I progetti aperti compariranno in questa lista.",
          320, 305, 530, 30);
  }
  BitBlt(target, 0, 0, r.right, r.bottom, dc, 0, 0, SRCCOPY);
  SelectObject(dc, old);
  DeleteObject(bitmap);
  DeleteDC(dc);
  EndPaint(window, &paintData);
}
void drawItem(Hub &hub, DRAWITEMSTRUCT &item) {
  const auto dc = item.hDC;
  auto r = item.rcItem;
  if (item.CtlID == recentId) {
    if (item.itemID == static_cast<UINT>(-1) ||
        item.itemID >= hub.recent->entries().size())
      return;
    const auto &entry = hub.recent->entries()[item.itemID];
    const bool chosen = (item.itemState & ODS_SELECTED) != 0;
    rectangle(dc, r, chosen ? selected : surface);
    rectangle(dc, {r.left, r.bottom - 1, r.right, r.bottom}, background);
    if (chosen)
      rectangle(dc, {r.left, r.top, r.left + hub.px(3), r.bottom}, accent);
    RECT icon{r.left + hub.px(16), r.top + hub.px(19), r.left + hub.px(51),
              r.top + hub.px(54)};
    rectangle(dc, icon, border);
    text(dc, hub.heading, accent, L"2D", icon,
         DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    text(dc, hub.heading, ink, wide(entry.name),
         {r.left + hub.px(66), r.top + hub.px(10), r.right - hub.px(155),
          r.top + hub.px(36)});
    text(dc, hub.smallFont, muted, entry.root.wstring(),
         {r.left + hub.px(66), r.top + hub.px(39), r.right - hub.px(18),
          r.top + hub.px(64)},
         DT_LEFT | DT_SINGLELINE | DT_PATH_ELLIPSIS);
    std::wstring stamp = L"Non disponibile";
    if (available(entry.root)) {
      const auto time = static_cast<std::time_t>(entry.opened);
      std::tm local{};
      wchar_t formatted[32]{};
      if (localtime_s(&local, &time) == 0)
        std::wcsftime(formatted, 32, L"%d/%m/%Y", &local);
      stamp = formatted;
    }
    text(dc, hub.smallFont, available(entry.root) ? muted : danger, stamp,
         {r.right - hub.px(150), r.top + hub.px(12), r.right - hub.px(16),
          r.top + hub.px(36)},
         DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
    if ((item.itemState & ODS_FOCUS) != 0) {
      InflateRect(&r, -3, -3);
      DrawFocusRect(dc, &r);
    }
  } else {
    const bool primary = item.CtlID == launchId || item.CtlID == createId;
    const bool disabled = (item.itemState & ODS_DISABLED) != 0,
               pressed = (item.itemState & ODS_SELECTED) != 0;
    rectangle(dc, r,
              (item.CtlID == createId || item.CtlID == browseId) ? sidebar
                                                                 : background);
    HBRUSH brush = CreateSolidBrush(
        disabled ? surface
                 : (pressed ? selected : (primary ? accent : surface)));
    HPEN pen = CreatePen(PS_SOLID, 1, primary && !disabled ? accent : border);
    const auto oldBrush = SelectObject(dc, brush),
               oldPen = SelectObject(dc, pen);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, hub.px(10), hub.px(10));
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
    text(dc, hub.font,
         disabled ? muted : (primary && !pressed ? background : ink),
         field(item.hwndItem), r, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
    if ((item.itemState & ODS_FOCUS) != 0) {
      InflateRect(&r, -4, -4);
      DrawFocusRect(dc, &r);
    }
  }
}
LRESULT CALLBACK handler(HWND window, UINT message, WPARAM wp, LPARAM lp) {
  if (message == WM_NCCREATE)
    SetWindowLongPtrW(
        window, GWLP_USERDATA,
        reinterpret_cast<LONG_PTR>(
            reinterpret_cast<CREATESTRUCTW *>(lp)->lpCreateParams));
  auto *hub = reinterpret_cast<Hub *>(GetWindowLongPtrW(window, GWLP_USERDATA));
  if (!hub)
    return DefWindowProcW(window, message, wp, lp);
  if (message == WM_CREATE) {
    auto font = [&](int size, int weight) {
      return CreateFontW(-hub->px(size), 0, 0, 0, weight, FALSE, FALSE, FALSE,
                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                         CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH,
                         L"Segoe UI");
    };
    hub->font = font(15, FW_NORMAL);
    hub->title = font(32, FW_SEMIBOLD);
    hub->heading = font(17, FW_SEMIBOLD);
    hub->smallFont = font(12, FW_NORMAL);
    hub->backgroundBrush = CreateSolidBrush(background);
    hub->surfaceBrush = CreateSolidBrush(surface);
    hub->sidebarBrush = CreateSolidBrush(sidebar);
    hub->nameLabel =
        control(window, *hub, L"STATIC", L"Nome del progetto", 0, 0);
    hub->name = control(window, *hub, L"EDIT", L"My Game", 0,
                        ES_AUTOHSCROLL | WS_TABSTOP | WS_BORDER);
    hub->pathLabel =
        control(window, *hub, L"STATIC", L"Cartella di destinazione", 0, 0);
    hub->location = control(window, *hub, L"EDIT", L"", 0,
                            ES_AUTOHSCROLL | WS_TABSTOP | WS_BORDER);
    hub->browse = control(window, *hub, L"BUTTON", L"Scegli cartella...",
                          browseId, WS_TABSTOP | BS_OWNERDRAW);
    hub->create = control(window, *hub, L"BUTTON", L"Crea progetto", createId,
                          WS_TABSTOP | BS_OWNERDRAW);
    hub->import = control(window, *hub, L"BUTTON", L"Apri da disco...", openId,
                          WS_TABSTOP | BS_OWNERDRAW);
    hub->list = control(window, *hub, L"LISTBOX", L"Progetti recenti", recentId,
                        WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_HASSTRINGS |
                            LBS_OWNERDRAWFIXED | LBS_NOINTEGRALHEIGHT);
    SendMessageW(hub->list, LB_SETITEMHEIGHT, 0, hub->px(76));
    hub->remove = control(window, *hub, L"BUTTON", L"Rimuovi dalla lista",
                          removeId, WS_TABSTOP | BS_OWNERDRAW);
    hub->open = control(window, *hub, L"BUTTON", L"Apri progetto", launchId,
                        WS_TABSTOP | BS_OWNERDRAW);
    hub->status = control(window, *hub, L"STATIC", L"", 0, 0);
    SendMessageW(hub->status, WM_SETFONT, reinterpret_cast<WPARAM>(hub->smallFont),
                 TRUE);
    try {
      hub->recent->load();
      refresh(window, *hub);
    } catch (const std::exception &e) {
      refresh(window, *hub);
      status(*hub, L"Cronologia non leggibile: " + wide(e.what()), true);
    }
    const BOOL dark = TRUE;
    DwmSetWindowAttribute(window, 20, &dark, sizeof(dark));
    layout(window, *hub);
    return 0;
  }
  if (message == WM_SIZE) {
    layout(window, *hub);
    return 0;
  }
  if (message == WM_GETMINMAXINFO) {
    auto *info = reinterpret_cast<MINMAXINFO *>(lp);
    info->ptMinTrackSize = {hub->px(950), hub->px(650)};
    return 0;
  }
  if (message == WM_ERASEBKGND)
    return 1;
  if (message == WM_PAINT) {
    paint(window, *hub);
    return 0;
  }
  if (message == WM_MEASUREITEM) {
    reinterpret_cast<MEASUREITEMSTRUCT *>(lp)->itemHeight =
        static_cast<UINT>(hub->px(76));
    return TRUE;
  }
  if (message == WM_DRAWITEM) {
    drawItem(*hub, *reinterpret_cast<DRAWITEMSTRUCT *>(lp));
    return TRUE;
  }
  if (message == WM_CTLCOLORSTATIC || message == WM_CTLCOLOREDIT ||
      message == WM_CTLCOLORLISTBOX) {
    const auto dc = reinterpret_cast<HDC>(wp);
    const auto child = reinterpret_cast<HWND>(lp);
    const bool fieldControl =
        message == WM_CTLCOLOREDIT || message == WM_CTLCOLORLISTBOX;
    const bool side = child == hub->nameLabel || child == hub->pathLabel;
    SetTextColor(dc, child == hub->status && hub->error
                         ? danger
                         : (fieldControl ? ink : muted));
    SetBkColor(dc, fieldControl ? surface : (side ? sidebar : background));
    return reinterpret_cast<LRESULT>(
        fieldControl ? hub->surfaceBrush
                     : (side ? hub->sidebarBrush : hub->backgroundBrush));
  }
  if (message == WM_COMMAND) {
    try {
      const int id = LOWORD(wp);
      if (id == recentId && HIWORD(wp) == LBN_SELCHANGE)
        selectionChanged(*hub);
      if (id == browseId)
        browse(window, *hub);
      if (id == createId) {
        if (!std::filesystem::is_regular_file(hub->editor))
          throw std::runtime_error("Build the editor first");
        const auto root = twodog::createEmptyProject(
            std::filesystem::path(field(hub->location)), utf(field(hub->name)));
        launch(window, *hub, root);
      }
      if (id == launchId || (id == recentId && HIWORD(wp) == LBN_DBLCLK)) {
        const int index = selection(*hub);
        if (index >= 0) {
          const auto root =
              hub->recent->entries().at(static_cast<std::size_t>(index)).root;
          launch(window, *hub, root);
        }
      }
      if (id == removeId) {
        const int index = selection(*hub);
        if (index >= 0) {
          hub->recent->remove(static_cast<std::size_t>(index));
          refresh(window, *hub);
          status(*hub,
                 L"Voce rimossa. I file del progetto sono rimasti sul disco.");
        }
      }
      if (id == openId) {
        wchar_t path[32768]{};
        OPENFILENAMEW dialog{};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = window;
        dialog.lpstrFile = path;
        dialog.nMaxFile = 32768;
        dialog.lpstrFilter = L"2Dog project (project.json)\0project.json\0";
        dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (GetOpenFileNameW(&dialog))
          launch(window, *hub, std::filesystem::path(path).parent_path());
      }
    } catch (const std::exception &e) {
      status(*hub, wide(e.what()), true);
      MessageBoxW(window, wide(e.what()).c_str(), L"2Dog Hub",
                  MB_OK | MB_ICONERROR);
    }
    return 0;
  }
  if (message == WM_DESTROY) {
    for (const auto font : {hub->font, hub->title, hub->heading, hub->smallFont})
      if (font)
        DeleteObject(font);
    for (const auto brush :
         {hub->backgroundBrush, hub->surfaceBrush, hub->sidebarBrush})
      if (brush)
        DeleteObject(brush);
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProcW(window, message, wp, lp);
}
} // namespace
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
  if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)))
    return 1;
  SetProcessDPIAware();
  Hub hub;
  const HDC screen = GetDC(nullptr);
  hub.scale = static_cast<float>(GetDeviceCaps(screen, LOGPIXELSX)) / 96.0F;
  ReleaseDC(nullptr, screen);
  std::vector<wchar_t> path(32768);
  const DWORD count =
      GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
  if (!count || count >= path.size()) {
    CoUninitialize();
    return 1;
  }
  hub.editor =
      std::filesystem::path(std::wstring(path.data(), count)).parent_path() /
      L"TwoDogEditor.exe";
  PWSTR local = nullptr;
  if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &local))) {
    CoUninitialize();
    return 1;
  }
  hub.recent = std::make_unique<twodog::RecentProjects>(
      std::filesystem::path(local) / L"2Dog/Hub/recent-projects.json");
  CoTaskMemFree(local);
  WNDCLASSW cls{};
  cls.hInstance = instance;
  cls.lpfnWndProc = handler;
  cls.lpszClassName = L"TwoDogHub";
  cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  RegisterClassW(&cls);
  const HWND window = CreateWindowExW(
      WS_EX_CONTROLPARENT, cls.lpszClassName, L"2Dog Hub",
      WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT,
      hub.px(1120), hub.px(740), nullptr, nullptr, instance, &hub);
  if (!window) {
    CoUninitialize();
    return 1;
  }
  ShowWindow(window, show);
  MSG message{};
  BOOL result = 0;
  while ((result = GetMessageW(&message, nullptr, 0, 0)) > 0) {
    if (message.message == WM_KEYDOWN && message.wParam == VK_RETURN &&
        message.hwnd == hub.list) {
      SendMessageW(window, WM_COMMAND, launchId, 0);
      continue;
    }
    if (!IsDialogMessageW(window, &message)) {
      TranslateMessage(&message);
      DispatchMessageW(&message);
    }
  }
  CoUninitialize();
  return result < 0 ? 1 : 0;
}
