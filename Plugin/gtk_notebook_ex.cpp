#include "cl_defs.h"

#if CL_USE_NATIVEBOOK

#include <wx/xrc/xmlres.h>
#include <wx/choicebk.h>
#include <wx/notebook.h>
#include "notebook_ex_nav_dlg.h"
#include "gtk_notebook_ex.h"
#include <wx/button.h>
#include "wx/sizer.h"
#include <wx/log.h>
#include <wx/wupdlock.h>

#include <gtk/gtk.h>
#include <wx/imaglist.h>

class MyNotebookPage: public wxObject
{
public:
    MyNotebookPage()
    {
        m_image = -1;
        m_page = NULL;
        m_box = NULL;
    }

    wxString           m_text;
    int                m_image;
    GtkWidget         *m_page;
    GtkLabel          *m_label;
    GtkWidget         *m_box;     // in which the label and image are packed
};

// The close button callback
static void OnNotebookButtonClicked(GtkWidget *widget, gpointer data)
{
	MyGtkPageInfo* pgInfo = reinterpret_cast<MyGtkPageInfo*>(data);
	if(pgInfo) {
		pgInfo->m_book->GTKHandleButtonCloseClicked(pgInfo);
	}
}

const wxEventType wxEVT_COMMAND_BOOK_PAGE_CHANGED         = XRCID("notebook_page_changing");
const wxEventType wxEVT_COMMAND_BOOK_PAGE_CHANGING        = XRCID("notebook_page_changed");
const wxEventType wxEVT_COMMAND_BOOK_PAGE_CLOSING         = XRCID("notebook_page_closing");
const wxEventType wxEVT_COMMAND_BOOK_PAGE_CLOSED          = XRCID("notebook_page_closed");
const wxEventType wxEVT_COMMAND_BOOK_PAGE_MIDDLE_CLICKED  = XRCID("notebook_page_middle_clicked");
const wxEventType wxEVT_COMMAND_BOOK_PAGE_X_CLICKED       = XRCID("notebook_page_x_btn_clicked");
const wxEventType wxEVT_COMMAND_BOOK_BG_DCLICK            = XRCID("notebook_page_bg_dclick");

Notebook::Notebook(wxWindow *parent, wxWindowID id, const wxPoint &pos, const wxSize &size, long style)
		: wxNotebook(parent, id, pos, size, wxNB_DEFAULT)
		, m_popupWin(NULL)
		, m_contextMenu(NULL)
		, m_style(style)
		, m_notify (true)
		, m_imgList(NULL)
{
	Initialize();
	SetPadding(wxSize(0, 0));
	Connect(wxEVT_COMMAND_NOTEBOOK_PAGE_CHANGED,  wxNotebookEventHandler(Notebook::OnIternalPageChanged),  NULL, this);
	Connect(wxEVT_COMMAND_NOTEBOOK_PAGE_CHANGING, wxNotebookEventHandler(Notebook::OnIternalPageChanging), NULL, this);
	Connect(wxEVT_NAVIGATION_KEY,                 wxNavigationKeyEventHandler(Notebook::OnNavigationKey),  NULL, this);
	Connect(wxEVT_MIDDLE_DOWN,                    wxMouseEventHandler(Notebook::OnMouseMiddle),            NULL, this);
	Connect(wxEVT_LEFT_DCLICK,                    wxMouseEventHandler(Notebook::OnMouseLeftDClick),        NULL, this);
	Connect(wxEVT_CONTEXT_MENU,                   wxContextMenuEventHandler(Notebook::OnMenu),             NULL, this);
}

Notebook::~Notebook()
{
	Disconnect(wxEVT_COMMAND_NOTEBOOK_PAGE_CHANGED,  wxNotebookEventHandler(Notebook::OnIternalPageChanged),  NULL, this);
	Disconnect(wxEVT_COMMAND_NOTEBOOK_PAGE_CHANGING, wxNotebookEventHandler(Notebook::OnIternalPageChanging), NULL, this);
	Disconnect(wxEVT_NAVIGATION_KEY,                 wxNavigationKeyEventHandler(Notebook::OnNavigationKey),  NULL, this);
	Disconnect(wxEVT_MIDDLE_DOWN,                    wxMouseEventHandler(Notebook::OnMouseMiddle),            NULL, this);
	Disconnect(wxEVT_LEFT_DCLICK,                    wxMouseEventHandler(Notebook::OnMouseLeftDClick),        NULL, this);
	Disconnect(wxEVT_CONTEXT_MENU,                   wxContextMenuEventHandler(Notebook::OnMenu),             NULL, this);

	std::map<wxWindow*, MyGtkPageInfo*>::iterator iter = m_gtk_page_info.begin();
	for(; iter != m_gtk_page_info.end(); iter++) {
		gtk_widget_destroy(iter->second->m_button);
		delete iter->second;
	}
	m_gtk_page_info.clear();
	
	if(m_imgList) {
		delete m_imgList;
		m_imgList = NULL;
	}
}

void Notebook::AddPage(wxWindow *win, const wxString &text, bool selected, const wxBitmap& bmp)
{
	if(win->GetParent() != this) {
		win->Reparent(this);
	}
	
	int imgid = DoGetBmpIdx(bmp);
	if (wxNotebook::AddPage(win, text, selected, imgid)) {
		win->Connect(wxEVT_KEY_DOWN, wxKeyEventHandler(Notebook::OnKeyDown),  NULL, this);
		PushPageHistory(win);
		if(HasCloseButton()) {
			int idx = (int)GetPageCount();
			GTKAddCloseButton(idx-1);
		}
	}
}

void Notebook::Initialize()
{
	static bool style_applied = false;
	if(!style_applied) {
		gtk_rc_parse_string(
		   "style \"tab-close-button-style\" {                              "
		   "      GtkWidget::focus-padding = 0                              "
		   "      GtkWidget::focus-line-width = 0                           "
		   "      xthickness = 0                                            "
		   "      ythickness = 0                                            "
		   " }                                                              "
		   "  widget \"*.tab-close-button\" style \"tab-close-button-style\""
		   );
		style_applied = true;   
	}
}

void Notebook::SetSelection(size_t page, bool notify)
{
	if (page >= GetPageCount())
		return;

	m_notify = notify;
	wxNotebook::SetSelection(page);
	m_notify = true;
	
	PushPageHistory(GetPage(page));
	GetPage(page)->SetFocus();
}


size_t Notebook::GetSelection()
{
	return static_cast<size_t>(wxNotebook::GetSelection());
}

wxWindow* Notebook::GetPage(size_t page)
{
	if (page >= GetPageCount())
		return NULL;

	return wxNotebook::GetPage(page);
}

bool Notebook::RemovePage(size_t page, bool notify)
{
	if (notify) {
		//send event to noitfy that the page has changed
		NotebookEvent event(wxEVT_COMMAND_BOOK_PAGE_CLOSING, GetId());
		event.SetSelection( page );
		event.SetEventObject( this );
		GetEventHandler()->ProcessEvent(event);

		if (!event.IsAllowed()) {
			return false;
		}
	}

	wxWindow* win = GetPage(page);
	win->Disconnect(wxEVT_KEY_DOWN, wxKeyEventHandler(Notebook::OnKeyDown),  NULL, this);

	bool rc = wxNotebook::RemovePage(page);
	if (rc) {
		GTKDeletePgInfo(win);
		PopPageHistory(win);
	}

	if (rc && notify) {
		//send event to noitfy that the page has been closed
		NotebookEvent event(wxEVT_COMMAND_BOOK_PAGE_CLOSED, GetId());
		event.SetSelection( page );
		event.SetEventObject( this );
		GetEventHandler()->ProcessEvent(event);
	}

	return rc;
}

bool Notebook::DeletePage(size_t page, bool notify)
{
	if (page >= GetPageCount())
		return false;

	if (notify) {
		//send event to noitfy that the page has changed
		NotebookEvent event(wxEVT_COMMAND_BOOK_PAGE_CLOSING, GetId());
		event.SetSelection( page );
		event.SetEventObject( this );
		GetEventHandler()->ProcessEvent(event);

		if (!event.IsAllowed()) {
			return false;
		}
	}

	wxWindow* win = GetPage(page);
	win->Disconnect(wxEVT_KEY_DOWN, wxKeyEventHandler(Notebook::OnKeyDown),  NULL, this);
	GTKDeletePgInfo(win);

	bool rc = wxNotebook::DeletePage(page);
	if (rc) {
		PopPageHistory(win);
	}

	if (rc && notify) {
		//send event to noitfy that the page has been closed
		NotebookEvent event(wxEVT_COMMAND_BOOK_PAGE_CLOSED, GetId());
		event.SetSelection( page );
		event.SetEventObject( this );
		GetEventHandler()->ProcessEvent(event);
	}

	return rc;

}

wxString Notebook::GetPageText(size_t page) const
{
	if (page >= GetPageCount())
		return wxT("");

	return wxNotebook::GetPageText(page);
}

void Notebook::OnNavigationKey(wxNavigationKeyEvent &e)
{
	if ( e.IsWindowChange() ) {
		if (DoNavigate())
			return;
	}

	e.Skip();
}

const wxArrayPtrVoid& Notebook::GetHistory() const
{
	return m_history;
}

void Notebook::InsertPage(size_t index, wxWindow* win, const wxString& text, bool selected, const wxBitmap& bmp)
{
	win->Reparent(this);
	int imgId = DoGetBmpIdx(bmp);
	if (wxNotebook::InsertPage(index, win, text, selected, imgId)) {
		win->Connect(wxEVT_KEY_DOWN, wxKeyEventHandler(Notebook::OnKeyDown),  NULL, this);
		PushPageHistory(win);
		if(HasCloseButton()) {
			GTKAddCloseButton(index);
		}
	}
}

void Notebook::SetRightClickMenu(wxMenu* menu)
{
	m_contextMenu = menu;
}

wxWindow* Notebook::GetCurrentPage()
{
	size_t selection = GetSelection();
	if (selection != Notebook::npos) {
		return GetPage(selection);
	}
	return NULL;
}

size_t Notebook::GetPageIndex(wxWindow *page)
{
	if ( !page )
		return Notebook::npos;

	for (size_t i=0; i< GetPageCount(); i++) {
		if (GetPage(i) == page) {
			return i;
		}
	}
	return Notebook::npos;
}

size_t Notebook::GetPageIndex(const wxString& text)
{
	for (size_t i=0; i< GetPageCount(); i++) {

		if (GetPageText(i) == text) {
			return i;
		}
	}
	return Notebook::npos;
}

bool Notebook::SetPageText(size_t index, const wxString &text)
{
	if (index >= GetPageCount())
		return false;
	return wxNotebook::SetPageText(index, text);
}

bool Notebook::DeleteAllPages(bool notify)
{
	bool res = true;
	size_t count = GetPageCount();
	for (size_t i=0; i<count && res; i++) {
		res = DeletePage(0, notify);
	}
	return res;
}

void Notebook::PushPageHistory(wxWindow *page)
{
	if (page == NULL)
		return;

	int where = m_history.Index(page);
	//remove old entry of this page and re-insert it as first
	if (where != wxNOT_FOUND) {
		m_history.Remove(page);
	}
	m_history.Insert(page, 0);
}

void Notebook::PopPageHistory(wxWindow *page)
{
	int where = m_history.Index(page);
	while (where != wxNOT_FOUND) {
		wxWindow *tab = static_cast<wxWindow *>(m_history.Item(where));
		m_history.Remove(tab);

		//remove all appearances of this page
		where = m_history.Index(page);
	}
}

wxWindow* Notebook::GetPreviousSelection()
{
	if (m_history.empty()) {
		return NULL;
	}
	//return the top of the heap
	return static_cast<wxWindow*>( m_history.Item(0));
}

void Notebook::OnIternalPageChanged(wxNotebookEvent &e)
{
	DoPageChangedEvent(e);
}

void Notebook::OnIternalPageChanging(wxNotebookEvent &e)
{
	DoPageChangingEvent(e);
}

void Notebook::OnMouseMiddle(wxMouseEvent &e)
{
	long flags(0);
	int where = wxNotebook::HitTest( e.GetPosition(), &flags );

	if (where != wxNOT_FOUND && HasCloseMiddle()) {
		
		//send event to noitfy that the page is changing
		NotebookEvent event(wxEVT_COMMAND_BOOK_PAGE_MIDDLE_CLICKED, GetId());
		event.SetSelection   ( where );
		event.SetOldSelection( wxNOT_FOUND );
		event.SetEventObject ( this );
		GetEventHandler()->AddPendingEvent(event);
	}
}

void Notebook::DoPageChangedEvent(wxBookCtrlBaseEvent& e)
{
	if (!m_notify) {
		e.Skip();
		return;
	}

	//send event to noitfy that the page is changing
	NotebookEvent event(wxEVT_COMMAND_BOOK_PAGE_CHANGED, GetId());
	event.SetSelection   ( e.GetSelection()    );
	event.SetOldSelection( e.GetOldSelection() );
	event.SetEventObject ( this );
	GetEventHandler()->ProcessEvent(event);
	PushPageHistory( GetPage(e.GetSelection()) );
	e.Skip();
}

void Notebook::DoPageChangingEvent(wxBookCtrlBaseEvent& e)
{
	if (!m_notify) {
		e.Skip();
		return;
	}


	//send event to noitfy that the page is changing
	NotebookEvent event(wxEVT_COMMAND_BOOK_PAGE_CHANGING, GetId());
	event.SetSelection   ( e.GetSelection()    );
	event.SetOldSelection( e.GetOldSelection() );
	event.SetEventObject ( this );
	GetEventHandler()->ProcessEvent(event);

	if ( !event.IsAllowed() ) {
		e.Veto();
	}
	e.Skip();
}


void Notebook::AssignImageList(wxImageList* imageList)
{
	wxNotebook::AssignImageList(imageList);
}

void Notebook::SetImageList(wxImageList* imageList)
{
	m_imgList = imageList;
	wxNotebook::SetImageList(m_imgList);
}

void Notebook::OnKeyDown(wxKeyEvent& e)
{
	if (e.GetKeyCode() == WXK_TAB && e.m_controlDown) {
		if (DoNavigate())
			return;

	} else {
		e.Skip();
	}
}

bool Notebook::DoNavigate()
{
	if ( !m_popupWin && GetPageCount() > 1) {

		m_popupWin = new NotebookNavDialog( this );
		m_popupWin->ShowModal();

		wxWindow *page = m_popupWin->GetSelection();
		m_popupWin->Destroy();
		m_popupWin = NULL;

		SetSelection( GetPageIndex(page), true );
		return true;
	}
	return false;
}

void Notebook::OnMenu(wxContextMenuEvent& e)
{
	int where = HitTest( ScreenToClient(::wxGetMousePosition()) );
	if(where != wxNOT_FOUND && m_contextMenu) {
		SetSelection(where, false);
		// dont notify the user about changes
		PopupMenu(m_contextMenu);
	}
	e.Skip();
}

void Notebook::OnMouseLeftDClick(wxMouseEvent& e)
{
	long flags(0);
	int where = HitTest( e.GetPosition(), &flags );

	if (where == wxNOT_FOUND) {
		
		//send event to noitfy that the page is changing
		NotebookEvent event(wxEVT_COMMAND_BOOK_BG_DCLICK, GetId());
		event.SetEventObject ( this );
		GetEventHandler()->AddPendingEvent(event);
	}
}

void Notebook::GTKAddCloseButton(int idx)
{
	// add button
	GtkWidget *image;
	
	MyNotebookPage *pg = (MyNotebookPage*) wxNotebook::GetNotebookPage(idx);
	MyGtkPageInfo *pgInfo = new MyGtkPageInfo;
	wxWindow* page = GetPage((size_t)idx);
	
	pgInfo->m_button = gtk_button_new();
	pgInfo->m_box    = pg->m_box;
	pgInfo->m_book   = this;
	
	image  = gtk_image_new_from_stock (GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU);
	gtk_widget_set_size_request(image, 10, 10);
	gtk_button_set_image (GTK_BUTTON(pgInfo->m_button), image);
	gtk_widget_set_name(pgInfo->m_button, "tab-close-button");
	gtk_button_set_relief(GTK_BUTTON(pgInfo->m_button), GTK_RELIEF_NONE);
	gtk_box_pack_start   (GTK_BOX(pg->m_box), pgInfo->m_button, FALSE, FALSE, 0);
	
	gtk_signal_connect (GTK_OBJECT (pgInfo->m_button), "clicked", GTK_SIGNAL_FUNC (OnNotebookButtonClicked), pgInfo);
	m_gtk_page_info[page] = pgInfo;
	
	GTKShowCloseButton();
}

void Notebook::GTKDeletePgInfo(wxWindow* page)
{
	std::map<wxWindow*, MyGtkPageInfo*>::iterator iter = m_gtk_page_info.find(page);
	if(iter != m_gtk_page_info.end()) {
		delete iter->second;
		m_gtk_page_info.erase(iter);
	}
}

MyGtkPageInfo* Notebook::GTKGetPgInfo(wxWindow* page)
{
	std::map<wxWindow*, MyGtkPageInfo*>::iterator iter = m_gtk_page_info.find(page);
	if(iter == m_gtk_page_info.end())
		return NULL;
	return iter->second;
}

void Notebook::GTKShowCloseButton()
{
	for(size_t i=0; i<GetPageCount(); i++) {
		MyGtkPageInfo* pgInfo = GTKGetPgInfo(GetPage(i));
		if(pgInfo){
			gtk_widget_show(pgInfo->m_button);
			gtk_widget_show(pgInfo->m_box);
		}
	}
}

void Notebook::GTKHandleButtonCloseClicked(MyGtkPageInfo* pgInfo)
{
	// Notebook button was clicked
	int idx = GTKIndexFromPgInfo(pgInfo);
	if(idx == wxNOT_FOUND)
		return;
	
	NotebookEvent event(wxEVT_COMMAND_BOOK_PAGE_X_CLICKED, GetId());
	event.SetSelection   ( idx );
	event.SetOldSelection( wxNOT_FOUND );
	event.SetEventObject ( this );
	GetEventHandler()->AddPendingEvent(event);
}

int Notebook::GTKIndexFromPgInfo(MyGtkPageInfo* pgInfo)
{
	for(size_t i=0; i<GetPageCount(); i++) {
		MyGtkPageInfo* info = GTKGetPgInfo(GetPage(i));
		if(info == pgInfo)
			return (int)i;
	}
	return wxNOT_FOUND;
}

int Notebook::DoGetBmpIdx(const wxBitmap& bmp)
{
	int idx = wxNOT_FOUND;
	if(bmp.IsOk()) {
		if(m_imgList == NULL) {
			SetImageList(new wxImageList(bmp.GetWidth(), bmp.GetHeight(), true));
		}
		idx = m_imgList->Add(bmp);
	}
	return idx;
}

#endif
