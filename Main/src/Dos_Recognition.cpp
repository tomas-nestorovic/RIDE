#include "stdafx.h"

	#define INI_RECOGNITION_ORDER	_T("recogn")

	#define INI_RECOGNITION_DOS_ID	_T("%08x")

	CDos::CRecognition::CRecognition()
		// ctor
		// - initialization
		: nDoses(0) {
		// - loading the Recognition Order from profile
		bool unknownDosAdded=false; // UnknownDos serves as the delimiter between recognized and ignored DOSes
		const CString s=app.GetProfileString(INI_GENERAL,INI_RECOGNITION_ORDER);
		LPCTSTR ps=s;
		for( TId dosId=0,nCharsRead=0; _stscanf(ps,INI_RECOGNITION_DOS_ID _T("%n"),&dosId,&nCharsRead)>0; ps+=nCharsRead )
			if (dosId!=CUnknownDos::Properties.id)
				for( POSITION pos=known.GetHeadPosition(); pos; ){
					PCProperties props=(PCProperties)known.GetNext(pos);
					if (props->id==dosId){
						order[++nDoses]=props; // indexing starts from 1
						break;
					}
				}
			else{
				order[++nDoses]=&CUnknownDos::Properties; // indexing starts from 1
				unknownDosAdded=true;
			}
		// - adding the Unknown DOS, just in case it wasn't added above (because the profile string didn't exist)
		if (!unknownDosAdded)
			order[++nDoses]=&CUnknownDos::Properties;
	}

	void CDos::CRecognition::__saveToProfile__() const{
		// saves the Recognition Order to profile
		TCHAR buf[1024],*p=buf;
		for( BYTE i=1; i<=nDoses; p+=::wsprintf(p,INI_RECOGNITION_DOS_ID,order[i++]->id) ); // indexing starts from 1
		app.WriteProfileString( INI_GENERAL, INI_RECOGNITION_ORDER, buf );
	}

	BYTE CDos::CRecognition::__addDosByPriorityDescending__(PCProperties props){
		// adds specified DOS to the earliest possible moment in recognition, respecting eventual previous user-defined ordering; returns the 1-based index at which it was added to
		BYTE i=__getOrderIndex__(&CUnknownDos::Properties);
		while (i>1 && props->recognitionPriority>order[i-1]->recognitionPriority) // indexing starts from 1
			i--;
		::memmove(	&order[i+1],
					&order[i],
					sizeof(PCProperties)*(++nDoses-i)
				);
		order[i]=props;
		return i;
	}

	BYTE CDos::CRecognition::__getOrderIndex__(PCProperties props) const{
		// returns 1-based Order index of the particular DOS (or 0 if DOS not found in the Order array)
		for( BYTE i=1; i<=nDoses; i++ ) // indexing starts from 1
			if (order[i]==props)
				return i;
		return 0;
	}

	POSITION CDos::CRecognition::__getFirstRecognizedDosPosition__() const{
		// returns the position of the first DOS that participates in Image recognition
		return	order[1]!=&CUnknownDos::Properties // indexing starts from 1
				? (POSITION)1
				: nullptr;
	}
	CDos::PCProperties CDos::CRecognition::__getNextRecognizedDos__(POSITION &pos) const{
		// returns the Properties of the next DOS that participates in Image recognition
		const PCProperties result=order[(BYTE)pos++];
		if (order[(BYTE)pos]==&CUnknownDos::Properties)
			pos=nullptr;
		return result;
	}

	CDos::PCProperties CDos::CRecognition::__perform__(PImage image,PFormat pOutFormatBoot) const{
		// returns Properties of DOS recognized in the specified Image (populates the output Format recognized in the boot Sector); returns UnknownDos if no DOS can be recognized; returns Null if recognition sequence cancelled by the user
		for( POSITION pos=__getFirstRecognizedDosPosition__(); pos; ){
			const PCProperties props=__getNextRecognizedDos__(pos);
			switch (props->fnRecognize(image,pOutFormatBoot)){
				case ERROR_SUCCESS:
					return props;
				case ERROR_CANCELLED:
					return nullptr;
			}
		}
		CUnknownDos::Properties.fnRecognize(image,pOutFormatBoot); // just a formality to properly fill up the FormatBoot
		return &CUnknownDos::Properties;
	}








	afx_msg void CMainWindow::__changeAutomaticDiskRecognitionOrder__(){
		// shows the Dialog to manually modify the recognition order of DOS
		// - defining the Dialog
		class CAutomaticRecognitionOrderDialog sealed:public Utils::CRideDialog{
		public:
			CDos::CRecognition recognition;
		private:
			const Utils::CRideFont symbolFont;
			CDos::PCProperties newlyDetectedDoses[256];

			void __repopulateListBoxesAndUpdateInteractivity__() const{
				// reads the Recognition Order and (re)populates with it the ListBoxes
				BYTE i=0;
				CDos::PCProperties props;
				CListBox lb;
				// . populating the ListBox with DOSes that take part in the recognition process
				lb.Attach( GetDlgItemHwnd(ID_DOS) );
					// : populating
					int scrollY=lb.GetTopIndex(), iSelected=lb.GetCurSel();
						lb.ResetContent();
						while (( props=recognition.order[++i] )!=&CUnknownDos::Properties) // indexing starts from 1
							lb.SetItemDataPtr( lb.AddString(props->name), (PVOID)props );
					lb.SetTopIndex(scrollY), lb.SetCurSel(iSelected);
					// : updating interaction possibilities
					EnableDlgItem( ID_UP, iSelected>0 );
					EnableDlgItem( ID_DOWN, iSelected>=0 && iSelected<lb.GetCount()-1 );
					EnableDlgItem( ID_REMOVE, iSelected>=0 );
					EnableDlgItem( ID_ORDER, lb.GetCount()>0 );
				lb.Detach();
				// . populating the ListBox with DOSes that don't take part in the recognition process
				lb.Attach( GetDlgItemHwnd(ID_HIDDEN) );
					// : populating
					scrollY=lb.GetTopIndex(), iSelected=lb.GetCurSel();
						lb.ResetContent();
						while (++i<=recognition.nDoses){
							props=recognition.order[i];
							lb.SetItemDataPtr( lb.AddString(props->name), (PVOID)props );
						}
					lb.SetTopIndex(scrollY), lb.SetCurSel(iSelected);
					// : updating interaction possibilities
					EnableDlgItem( ID_ADD, iSelected>=0 );
				lb.Detach();
			}

			void PreInitDialog() override{
				// dialog initialization
				// - base
				__super::PreInitDialog();
				// - searching for newly detected DOSes
				CDos::PCProperties *pNewlyDetectedDos=newlyDetectedDoses;
				for( POSITION pos=CDos::known.GetHeadPosition(); pos; ){
					const CDos::PCProperties props=(CDos::PCProperties)CDos::known.GetNext(pos);
					if (!recognition.__getOrderIndex__(props))
						recognition.__addDosByPriorityDescending__( *pNewlyDetectedDos++=props );
				}
				*pNewlyDetectedDos=nullptr; // terminating the array
				//recognition.__saveToProfile__();
				if (pNewlyDetectedDos>newlyDetectedDoses)
					Utils::Information( _T("Some new DOSes have been detected!\nPlease confirm the auto-recognition sequence where they've all been added to (and marked).") );
				// - populating the ListBoxes with current Recognition Order
				__repopulateListBoxesAndUpdateInteractivity__();
				// - setting graphical symbols to buttons that organize the Recognition Order
				SetDlgItemSingleCharUsingFont( ID_UP, 0xf0e1, symbolFont );
				SetDlgItemSingleCharUsingFont( ID_DOWN, 0xf0e2, symbolFont );
				SetDlgItemSingleCharUsingFont( ID_ADD, 0xf0e7, symbolFont );
				SetDlgItemSingleCharUsingFont( ID_REMOVE, 0xf0e8, symbolFont );
				// - (dis)allowing the Cancel button
				#ifndef _DEBUG
					EnableDlgItem( IDCANCEL, *newlyDetectedDoses==nullptr ); // if NewDoses were detected, the Dialog cannot be cancelled - Recognition Order with NewDetectedDoses must be confirmed
				#endif
			}

			LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override{
				// window procedure
				switch (msg){
					case WM_MEASUREITEM:
						// determining the height of a ListBox item (presumed that there are no other owner-drawn controls!)
						((PMEASUREITEMSTRUCT)lParam)->itemHeight=38*Utils::LogicalUnitScaleFactor;
						return 0;
					case WM_DRAWITEM:{
						// drawing the ListBox item (presumed that there are no other owner-drawn controls!)
						const PDRAWITEMSTRUCT pdis=(PDRAWITEMSTRUCT)lParam;
						if (pdis->itemID<0) return 0; // if the ListBox is empty, we are done
						if (!(pdis->itemAction&(ODA_SELECT|ODA_DRAWENTIRE))) return 0; // if no need to draw anything, we are done
						const CDos::PCProperties props=(CDos::PCProperties)pdis->itemData;
						const HDC dc=pdis->hDC;
						::SetBkMode(dc,TRANSPARENT);
						Utils::ScaleLogicalUnit(dc);
						RECT r=pdis->rcItem;
						Utils::UnscaleLogicalUnit((PINT)&r,4), Utils::UnscaleLogicalUnit((PINT)&pdis->rcItem,4);
						const Utils::CRideFont newDosSymbolFont(FONT_WINGDINGS,180,false,false);
						const HGDIOBJ hFont0=::SelectObject( dc, newDosSymbolFont );
							if (pdis->itemState&ODS_SELECTED){
								::FillRect( dc, &r, Utils::CRideBrush::Selection );
								::SetTextColor( dc, COLOR_WHITE );
							}else{
								::FillRect( dc, &r, Utils::CRideBrush::White );
								::SetTextColor( dc, COLOR_BLACK );
							}
							r.left+=4, r.bottom-=15;
							for( CDos::PCProperties *p=newlyDetectedDoses; *p; )
								if (*p++==props){
									static const WCHAR NewDosSymbol=0xf025;
									::DrawTextW( dc, &NewDosSymbol,1, &r, DT_SINGLELINE|DT_VCENTER );
									r.left+=25;
									break;
								}
						::SelectObject( dc, Utils::CRideFont::StdBold );
							::DrawText( dc, props->name,-1, &r, DT_SINGLELINE|DT_VCENTER );
						::SelectObject( dc, Utils::CRideFont::Small );
							r.top=r.bottom-8, r.bottom=pdis->rcItem.bottom;
							TCHAR desc[80];
							::wsprintf( desc, _T("Recognition priority: %d"), props->recognitionPriority );
							::DrawText( dc, desc,-1, &r, DT_SINGLELINE|DT_VCENTER );
						::SelectObject(dc,hFont0);
						return 0;
					}
					case WM_COMMAND:{
						// processing a command
						CListBox lb;
						lb.Attach( GetDlgItemHwnd(ID_DOS) ); // attaching the ListBox of recognized DOSes right away as most commands work with it
							CDos::PCProperties props=(CDos::PCProperties)lb.GetItemDataPtr(lb.GetCurSel());
							BYTE i=recognition.__getOrderIndex__(props); // indexing starts from 1
							switch (wParam){
								case ID_UP:
									// moving selected recognized DOS to an earlier position in the Recognition Order
									recognition.order[i]=recognition.order[i-1];
									recognition.order[--i]=props;
									lb.SetCurSel(--i);
									break;
								case ID_DOWN:
									// moving selected recognized DOS to a later position in the Recognition Order
									recognition.order[i]=recognition.order[i+1];
									lb.SetCurSel(i);
									recognition.order[++i]=props;
									break;
								case ID_ORDER:{
									// ordering automatically recognized DOSes by their RecognitionPriority descending
									const CDos::CRecognition original(recognition);
									const BYTE iUnknownDos=original.__getOrderIndex__(&CUnknownDos::Properties);
									::memmove(	&recognition.order[1], // indexing starts from 1
												&recognition.order[iUnknownDos],
												sizeof(CDos::PCProperties)*(( recognition.nDoses-=iUnknownDos-1 ))
											);
									for( BYTE j=1; j<iUnknownDos; recognition.__addDosByPriorityDescending__(original.order[j++]) );
									if (i){ // if any of recognized DOSes selected before clicking on the button ...
										for( i=1; recognition.order[i]!=props; i++ );
										lb.SetCurSel(i-1); // ... reselecting the DOS after they've been ordered by priority
									}
									break;
								}
								case ID_ADD:{
									// adding selected ignored DOS to the Recognition Order
									// . removing the DOS from "ignored portion" of the Recognition Order
									CListBox lbIgnored;
									lbIgnored.Attach( GetDlgItemHwnd(ID_HIDDEN) );
										props=(CDos::PCProperties)lbIgnored.GetItemDataPtr(lbIgnored.GetCurSel());
										i=recognition.__getOrderIndex__(props); // indexing starts from 1
										::memmove(	&recognition.order[i],
													&recognition.order[i+1],
													sizeof(CDos::PCProperties)*((recognition.nDoses--)-i)
												);
										lbIgnored.SetCurSel( std::min<>(lbIgnored.GetCurSel(),lbIgnored.GetCount()-2) );
									lbIgnored.Detach();
									// . adding (and selecting) the DOS to the "recognized portion" of the Recognition Order
									lb.SetItemDataPtr( lb.AddString(""), (PVOID)&CUnknownDos::Properties ); // a dummy item for the below selection index to be valid
									lb.SetCurSel( recognition.__addDosByPriorityDescending__(props)-1 );
									break;
								}
								case ID_REMOVE:{
									// removing selected recognized DOS from the Recognition Order
									const BYTE iUnknownDos=recognition.__getOrderIndex__(&CUnknownDos::Properties);
									::memmove(	&recognition.order[i],
												&recognition.order[i+1],
												sizeof(CDos::PCProperties)*(iUnknownDos-i)
											);
									recognition.order[iUnknownDos]=props;
									lb.SetCurSel( std::min<>(i-1,lb.GetCount()-2) );
									break;
								}
							}
						lb.Detach(); // detaching the above attached ListBox
						__repopulateListBoxesAndUpdateInteractivity__(); // "redrawing" the dialog
						break;
					}
					case WM_NOTIFY:{
						// processing a notification
						const LPCWPSTRUCT pcws=(LPCWPSTRUCT)lParam;
						if (pcws->wParam==ID_HELP_INDEX)
							switch (pcws->message){
								case NM_CLICK:
								case NM_RETURN:{
									// . defining the Dialog
									class CHelpDialog sealed:public Utils::CCommandDialog{
										void PreInitDialog() override{
											// dialog initialization
											// : base
											__super::PreInitDialog();
											// : supplying available actions
											__addCommandButton__( ID_DRIVE, _T("What is a recognition sequence good for?") );
											__addCommandButton__( IDCANCEL, MSG_HELP_CANCEL );
										}
									public:
										CHelpDialog()
											// ctor
											: Utils::CCommandDialog(_T("This might interest you:")) {
										}
									} d;
									// . showing the Dialog and processing its result
									TCHAR url[200];
									switch (d.DoModal()){
										case ID_DRIVE:
											Utils::NavigateToUrlInDefaultBrowser( Utils::GetApplicationOnlineHtmlDocumentUrl(_T("faq_recognition.html"),url) );
											break;
									}
									return TRUE;
								}
							}
					}
				}
				return __super::WindowProc(msg,wParam,lParam);
			}
		public:
			CAutomaticRecognitionOrderDialog()
				// ctor
				: Utils::CRideDialog(IDR_DOS_RECOGNITION)
				, symbolFont(FONT_WINGDINGS,110,false,true) {
			}
		} d;
		// - showing the Dialog and processing its result
		if (d.DoModal()==IDOK)
			d.recognition.__saveToProfile__();
	}
