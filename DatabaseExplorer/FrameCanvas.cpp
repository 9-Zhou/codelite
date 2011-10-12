#include "CreateForeignKey.h"

#include "FrameCanvas.h"
#include "ErdTable.h"
#include "ErdView.h"


FrameCanvas::FrameCanvas(wxSFDiagramManager* manager,IDbAdapter* dbAdapter, wxWindow* parent, wxPanel* parentPanel, wxWindowID id):
	wxSFShapeCanvas(manager,parent,id, wxDefaultPosition, wxDefaultSize, wxHSCROLL | wxVSCROLL | wxSTATIC_BORDER) {
	m_pParentPanel = (ErdPanel*) parentPanel;

	m_pDbAdapter = dbAdapter;


	GetHistoryManager().SetMode(wxSFCanvasHistory::histUSE_SERIALIZATION);
	GetHistoryManager().SetHistoryDepth(10);

	AddStyle(sfsGRADIENT_BACKGROUND);
	SetGradientFrom( wxColour(230, 230, 230) );
	SetGradientTo( wxColour(255, 255, 255) );

	SetHoverColour( wxColour(200, 200, 200) );

	AddStyle(sfsGRID_USE);
	AddStyle(sfsGRID_SHOW);
	SetGridLineMult( 10 );
	#ifdef __WXMSW__
	SetGridStyle( wxDOT );
	#else
	SetGridStyle( wxSHORT_DASH );
	#endif

	AddStyle(sfsPROCESS_MOUSEWHEEL);
	SetMinScale(0.2);
	SetMaxScale(2);

	GetDiagramManager()->ClearAcceptedShapes();
	GetDiagramManager()->AcceptShape(wxT("All"));

	SaveCanvasState();
	
	wxAcceleratorEntry entries[4];
	entries[0].Set(wxACCEL_CTRL,  (int) 'C',     wxID_COPY);
	entries[1].Set(wxACCEL_CTRL,  (int) 'X',     wxID_CUT);
	entries[2].Set(wxACCEL_CTRL, (int) 'V',      wxID_PASTE);
	entries[3].Set(wxACCEL_CTRL, (int) 'A',      wxID_SELECTALL);
	wxAcceleratorTable accel(4, entries);
	SetAcceleratorTable(accel);
	
	Connect( wxID_SELECTALL, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( FrameCanvas::OnSelectAll ) );
}

FrameCanvas::~FrameCanvas() {
}

void FrameCanvas::UpdateERD() {
	ShapeList lstShapes;
	GetDiagramManager()->GetShapes( CLASSINFO(ErdTable), lstShapes );
	for( ShapeList::iterator it = lstShapes.begin(); it != lstShapes.end(); ++it ) {
		((ErdTable*)*it)->UpdateColumns();
	}

	lstShapes.Clear();
	GetDiagramManager()->GetShapes( CLASSINFO(ErdView), lstShapes );
	for( ShapeList::iterator it = lstShapes.begin(); it != lstShapes.end(); ++it ) {
		((ErdView*)*it)->UpdateView();
	}

	UpdateVirtualSize();
	Refresh( false );
}

void FrameCanvas::OnConnectionFinished(wxSFLineShape* connection) {
	//wxMessageBox(wxT("hotovo2"));
	m_pParentPanel->SetToolMode(ErdPanel::modeDESIGN);

	return;
}

void FrameCanvas::OnLeftDown(wxMouseEvent& event) {
	wxSFShapeBase* pShape = NULL;

	switch(m_pParentPanel->GetToolMode()) {

		case ErdPanel::modeTABLE: {
			pShape = GetDiagramManager()->AddShape(new ErdTable(), NULL, event.GetPosition(), sfINITIALIZE, sfDONT_SAVE_STATE);
			//pShape = GetDiagramManager()->AddShape(CLASSINFO(wxSFRoundRectShape), event.GetPosition(), sfDONT_SAVE_STATE);
			if (pShape) {
				pShape->AcceptConnection(wxT("All"));
				pShape->AcceptSrcNeighbour(wxT("All"));
				pShape->AcceptTrgNeighbour(wxT("All"));

				DBETable* tab = new DBETable();
				tab->SetName(wxT("NewTable"));
				pShape->SetUserData(tab);

				((ErdTable*)pShape)->UpdateColumns();
				pShape->Refresh();

				SaveCanvasState();
			}
		}
		break;
		case ErdPanel::modeLine: {
			if (GetMode() == modeREADY) {
				ErdTable* pTab = wxDynamicCast(GetShapeUnderCursor()->GetGrandParentShape(), ErdTable);
				if (pTab){
					wxSFTextShape* pText = wxDynamicCast(GetShapeUnderCursor(),wxSFTextShape);
					if (pText) {
						m_srcCol = pText->GetText().substr(3);
					} else m_srcCol = wxT("");
					StartInteractiveConnection(CLASSINFO(wxSFOrthoLineShape), event.GetPosition());
				}
			} else wxSFShapeCanvas::OnLeftDown(event);
		}
		break;

		case ErdPanel::modeVIEW: {
			pShape = GetDiagramManager()->AddShape(new ErdView(), NULL, event.GetPosition(), sfINITIALIZE, sfDONT_SAVE_STATE);
			if (pShape) {
				pShape->AcceptConnection(wxT("All"));
				pShape->AcceptSrcNeighbour(wxT("All"));
				pShape->AcceptTrgNeighbour(wxT("All"));

				View* view = new View();
				view->SetName(_("New view"));
				view->SetSelect(wxT("SELECT * FROM table"));
				pShape->SetUserData(view);

				((ErdView*)pShape)->UpdateView();
				pShape->Refresh();

				SaveCanvasState();
			}
		}
		break;

		default:

			// do default actions
			wxSFShapeCanvas::OnLeftDown(event);
	}

	if( pShape ) {
		if(!event.ControlDown()) {
			m_pParentPanel->SetToolMode(ErdPanel::modeDESIGN);
		}
	}

}
void FrameCanvas::OnRightDown(wxMouseEvent& event) {
	m_mousePos = event.GetPosition();
	wxMenu mnu;
	mnu.Connect(wxEVT_COMMAND_MENU_SELECTED, (wxObjectEventFunction)&FrameCanvas::OnPopupClick, NULL, this);
	
	mnu.Append(IDR_POPUP_COPY, _("Copy"))->Enable(CanCopy());
	mnu.Append(IDR_POPUP_CUT, _("Cut"))->Enable(CanCut());
	mnu.Append(IDR_POPUP_PASTE, _("Paste"))->Enable(CanPaste());
	mnu.AppendSeparator();
	
	mnu.Append(IDR_POPUP_NEW_TABLE, _("Add new ERD table"));
	mnu.Append(IDR_POPUP_NEW_VIEW, _("Add new ERD view"));
	mnu.AppendSeparator();
	wxSFShapeBase* sBase = GetShapeUnderCursor();
	if (sBase) {

		ErdTable* table = wxDynamicCast(sBase->GetGrandParentShape(), ErdTable);
		if (table) {
			mnu.Append(IDR_POPUP_MI1, 	_("Add column"));
			mnu.Append(IDR_POPUP_MI2, 	_("Add create sql to clippoard"));
			mnu.AppendSeparator();
			mnu.Append(IDR_POPUP_NEW_KEY, _("Create foreign key"));
			mnu.Append(IDR_POPUP_MI3, _("Create view for table"));			
		}
	}

	PopupMenu(&mnu);
}


void FrameCanvas::OnKeyDown(wxKeyEvent& event) {
	wxSFShapeCanvas::OnKeyDown(event);
}

void FrameCanvas::OnPopupClick(wxCommandEvent &evt) {
	//void *data=static_cast<wxMenu *>(evt.GetEventObject())->GetClientData();
	   if (evt.GetId() == IDR_POPUP_MI1) {
			ErdTable* table = wxDynamicCast(GetShapeUnderCursor()->GetGrandParentShape(), ErdTable);
			if (table) {
				table->AddColumn(_("ID"),m_pDbAdapter->GetDbTypeByUniversalName(IDbType::dbtTYPE_INT));
				table->AddColumn(_("text"),m_pDbAdapter->GetDbTypeByUniversalName(IDbType::dbtTYPE_TEXT));
				table->UpdateColumns();
				table->Refresh();
				SaveCanvasState();
			}
		}else if (evt.GetId() == IDR_POPUP_MI2) {
			bool dropTable = ( wxMessageBox( _("Add drop table statement?"),_("Drop table"),wxYES_NO ) == wxYES );
			if (wxTheClipboard->Open()) {
				ErdTable* table = wxDynamicCast(GetShapeUnderCursor()->GetGrandParentShape(), ErdTable);
				if (table) {
					//TODO:LANG:
					wxTheClipboard->SetData(new wxTextDataObject(m_pDbAdapter->GetCreateTableSql(table->GetTable(), dropTable)));
				}
				wxTheClipboard->Close();
			}
		}else if (evt.GetId() == IDR_POPUP_MI3) {
			ErdTable* table = wxDynamicCast(GetShapeUnderCursor()->GetGrandParentShape(), ErdTable);
			if (table) {
				DBETable* pTab = table->GetTable();
				wxPoint point(round(table->GetAbsolutePosition().x), round(table->GetAbsolutePosition().y));
				point.x = table->GetRectSize().x + 10;
				ErdView* pView =  (ErdView*) GetDiagramManager()->AddShape(new ErdView(), NULL, point, sfINITIALIZE, sfDONT_SAVE_STATE);
				if (pView) {
					View* view = new View();
					view->SetName(pTab->GetName()+wxT("VW"));
					view->SetParentName(pTab->GetParentName());
					view->SetSelect(m_pDbAdapter->GetDefaultSelect(pTab->GetParentName(),pTab->GetName()));
					pView->SetUserData(view);
					pView->UpdateView();
					pView->Refresh();
					SaveCanvasState();
				}
			}
		}else if (evt.GetId() == IDR_POPUP_COPY){
			Copy();
		}else if (evt.GetId() ==  IDR_POPUP_CUT){
			Cut();	
		}else if (evt.GetId() == IDR_POPUP_PASTE){
			Paste();
			UpdateERD();
		}else if (evt.GetId() == IDR_POPUP_NEW_TABLE){
			wxSFShapeBase* pShape = GetDiagramManager()->AddShape(new ErdTable(), NULL, m_mousePos, sfINITIALIZE, sfDONT_SAVE_STATE);
			//pShape = GetDiagramManager()->AddShape(CLASSINFO(wxSFRoundRectShape), event.GetPosition(), sfDONT_SAVE_STATE);
			if (pShape) {
				pShape->AcceptConnection(wxT("All"));
				pShape->AcceptSrcNeighbour(wxT("All"));
				pShape->AcceptTrgNeighbour(wxT("All"));

				DBETable* tab = new DBETable();
				tab->SetName(wxT("NewTable"));
				pShape->SetUserData(tab);

				((ErdTable*)pShape)->UpdateColumns();
				pShape->Refresh();

				SaveCanvasState();
			}
		}else if (evt.GetId() == IDR_POPUP_NEW_VIEW){
			wxSFShapeBase* pShape = GetDiagramManager()->AddShape(new ErdView(), NULL, m_mousePos, sfINITIALIZE, sfDONT_SAVE_STATE);
			if (pShape) {
				pShape->AcceptConnection(wxT("All"));
				pShape->AcceptSrcNeighbour(wxT("All"));
				pShape->AcceptTrgNeighbour(wxT("All"));

				View* view = new View();
				view->SetName(wxT("New view"));
				view->SetSelect(wxT("SELECT * FROM table"));
				pShape->SetUserData(view);

				((ErdView*)pShape)->UpdateView();
				pShape->Refresh();

				SaveCanvasState();
			}
			
		}else if (evt.GetId() == IDR_POPUP_NEW_KEY){
			if (GetMode() == modeREADY) {
					wxSFTextShape* pText = wxDynamicCast(GetShapeUnderCursor(),wxSFTextShape);
					if (pText) {
						m_srcCol = pText->GetText().substr(3);
					} else m_srcCol = wxT("");
					StartInteractiveConnection(CLASSINFO(wxSFOrthoLineShape), m_mousePos);
				}
			
		}

		
	
}

void FrameCanvas::OnLeftDoubleClick(wxMouseEvent& event) {
	wxSFShapeBase* sp = GetShapeUnderCursor();
	if (sp) {
		ErdTable* table = wxDynamicCast(sp->GetGrandParentShape(),ErdTable);
		if (table) {
			if (table->GetTable() ) {
				TableSettings settingDialog(this, m_pDbAdapter);
				settingDialog.SetTable(table->GetTable(),(wxSFDiagramManager*) table->GetParentManager());
				settingDialog.ShowModal();
				table->UpdateColumns();
				table->Refresh();
				SaveCanvasState();
			}
		}
		ErdView* view = wxDynamicCast(sp->GetGrandParentShape(), ErdView);
		if (view) {
			if (view->GetView()) {
				ViewSettings settingDialog(this,m_pDbAdapter);
				settingDialog.SetView(view->GetView(),(wxSFDiagramManager*) view->GetParentManager() );
				settingDialog.ShowModal();
				view->UpdateView();
				view->Refresh();
				SaveCanvasState();
			}
		}
	}
	wxSFShapeCanvas::OnLeftDoubleClick(event);
}

void FrameCanvas::OnDrop(wxCoord x, wxCoord y, wxDragResult def, const ShapeList& dropped) {
	ShapeList::compatibility_iterator node = dropped.GetFirst();
	dndTableShape* dndTab = NULL;
	wxSFShapeBase* pShape = NULL;
	while( node ) {
		dndTab = wxDynamicCast(node->GetData(),dndTableShape);
		node = node->GetNext();
	}

	if (dndTab) {
		if (dndTab->GetUserData()->IsKindOf(CLASSINFO(DBETable))) {
			m_pDbAdapter->ConvertTable((DBETable*) dndTab->GetUserData());
			pShape = GetDiagramManager()->AddShape(new ErdTable((DBETable* ) dndTab->GetUserData()), NULL, wxPoint( x,y), sfINITIALIZE, sfDONT_SAVE_STATE);
		}
		if (dndTab->GetUserData()->IsKindOf(CLASSINFO(View))) {
			pShape = GetDiagramManager()->AddShape(new ErdView((View* ) dndTab->GetUserData()), NULL, wxPoint( x,y), sfINITIALIZE, sfDONT_SAVE_STATE);
		}

		if (pShape) {
			pShape->AcceptConnection(wxT("All"));
			pShape->AcceptSrcNeighbour(wxT("All"));
			pShape->AcceptTrgNeighbour(wxT("All"));
			//pShape->Update();

			SaveCanvasState();
		}

		dndTab->SetUserData(NULL);
		GetDiagramManager()->RemoveShape(dndTab);
	} else {
		SaveCanvasState();
	}

	UpdateERD();
}

wxString FrameCanvas::GetSqlScript() {
	wxString retStr = wxT("");

	ShapeList lstShapes;
	GetDiagramManager()->GetShapes( CLASSINFO(ErdTable), lstShapes );
	ShapeList::compatibility_iterator node = lstShapes.GetFirst();
	while( node ) {
		ErdTable* tab = wxDynamicCast(node->GetData(),ErdTable);
		if (tab) {
			retStr.append(m_pDbAdapter->GetCreateTableSql(tab->GetTable(),true));
		}
		node = node->GetNext();
	}
	lstShapes.Clear();
	GetDiagramManager()->GetShapes( CLASSINFO(ErdView), lstShapes );
	node = lstShapes.GetFirst();
	while( node ) {
		ErdView* view = wxDynamicCast(node->GetData(),ErdView);
		if (view) {
			retStr.append(m_pDbAdapter->GetCreateViewSql(view->GetView(),true));
		}
		node = node->GetNext();
	}
	lstShapes.Clear();
	GetDiagramManager()->GetShapes( CLASSINFO(ErdTable), lstShapes );
	node = lstShapes.GetFirst();
	while( node ) {
		ErdTable* tab = wxDynamicCast(node->GetData(),ErdTable);
		if (tab) {
			retStr.append(m_pDbAdapter->GetAlterTableConstraintSql(tab->GetTable()));
		}
		node = node->GetNext();
	}
	return retStr;
}

bool FrameCanvas::OnPreConnectionFinished(wxSFLineShape* connection) {
	wxSFTextShape* pText = wxDynamicCast(GetShapeUnderCursor(), wxSFTextShape);
	if (pText) {
		m_dstCol = pText->GetText().substr(3);
	}

	ErdTable *pSrcT = wxDynamicCast(GetDiagramManager()->GetItem(connection->GetSrcShapeId()),ErdTable);
	ErdTable *pTrgT = wxDynamicCast(GetDiagramManager()->GetItem(connection->GetTrgShapeId()),ErdTable);
	if( pSrcT && pTrgT ) {
		CreateForeignKey dlg(this, pSrcT, pTrgT, m_srcCol, m_dstCol);
		dlg.ShowModal();
	}

	m_pParentPanel->SetToolMode(ErdPanel::modeDESIGN);

	return false;
}
void FrameCanvas::OnPaste(const ShapeList& pasted)
{
	ShapeList::compatibility_iterator node = pasted.GetFirst();
	while( node ) {
		ErdTable* tab = wxDynamicCast(node->GetData(), ErdTable);
		if (tab) m_pDbAdapter->ConvertTable(tab->GetTable());
		node = node->GetNext();
	}
	
	
	wxSFShapeCanvas::OnPaste(pasted);
}

void FrameCanvas::OnSelectAll(wxCommandEvent& evt)
{
	SelectAll();
}
